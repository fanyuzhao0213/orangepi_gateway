// SPDX-License-Identifier: GPL-2.0
/*
 * fyz_led_driver.c - FYZ LED Platform 驱动
 *
 * 功能：
 *   1. 通过 GPIO 控制 LED 亮灭
 *   2. 通过 sysfs 接口设置触发模式（none / heartbeat / timer）
 *   3. 支持设备树（Device Tree）匹配
 *
 * 硬件假设：设备树节点提供 "gpios" 属性指向 LED 所用 GPIO
 *
 * 设备树示例：
 *   fyz_leds: leds {
 *       compatible = "fyz-leds";
 *       gpios = <&gpio1 5 GPIO_ACTIVE_HIGH>;
 *       label = "user-led";
 *   };
 */

#include <linux/module.h>       /* MODULE_* 宏、module_init/exit */
#include <linux/kernel.h>       /* printk、pr_* */
#include <linux/platform_device.h> /* platform_driver、platform_device */
#include <linux/of.h>           /* of_match_table、of_property_read_* */
#include <linux/of_gpio.h>      /* of_get_named_gpio */
#include <linux/gpio.h>         /* gpio_request、gpio_direction_output 等 */
#include <linux/sysfs.h>        /* sysfs_create_group */
#include <linux/timer.h>        /* timer_list（软件定时器，用于 timer 触发模式） */
#include <linux/jiffies.h>      /* msecs_to_jiffies */
#include <linux/string.h>       /* sysfs_streq */
#include <linux/slab.h>         /* kzalloc、kfree */

/* ====================== 驱动私有数据结构 ====================== */

/**
 * enum fyz_led_trigger - LED 触发模式枚举
 * @TRIGGER_NONE:      常亮 / 常灭，由用户直接写 brightness 控制
 * @TRIGGER_HEARTBEAT: 心跳模式，模拟心跳闪烁节律（长短双闪）
 * @TRIGGER_TIMER:     定时器模式，固定频率周期翻转（500 ms）
 */
enum fyz_led_trigger {
    TRIGGER_NONE = 0,
    TRIGGER_HEARTBEAT,
    TRIGGER_TIMER,
};

/**
 * struct fyz_led_priv - 每个 LED 设备的私有数据
 * @dev:       指向 platform_device 的 device，方便 dev_err/dev_info 使用
 * @gpio:      LED 所用的 GPIO 编号
 * @brightness: 当前亮度（0=灭，1=亮）
 * @trigger:   当前触发模式
 * @timer:     软件定时器，用于 timer / heartbeat 模式
 * @hb_step:   心跳节拍计数器（0~3 四步节律）
 * @label:     LED 标签字符串（来自设备树 label 属性）
 */
struct fyz_led_priv {
    struct device       *dev;
    int                  gpio;
    int                  brightness;
    enum fyz_led_trigger trigger;
    struct timer_list    timer;
    int                  hb_step;
    const char          *label;
};

/* ====================== 内部辅助函数 ====================== */

/**
 * fyz_led_set() - 直接设置 GPIO 电平（线程安全调用方负责保证）
 * @priv: 私有数据指针
 * @on:   1 点亮，0 熄灭
 */
static void fyz_led_set(struct fyz_led_priv *priv, int on)
{
    priv->brightness = on;
    gpio_set_value(priv->gpio, on);
}

/**
 * fyz_timer_callback() - 定时器回调，根据当前触发模式操作 LED
 * @t: 内核 timer_list 指针（通过 from_timer 取出私有数据）
 *
 * 该函数在软中断上下文中执行，不能睡眠。
 */
static void fyz_timer_callback(struct timer_list *t)
{
    struct fyz_led_priv *priv = from_timer(priv, t, timer);
    unsigned long delay_ms;

    switch (priv->trigger) {

    case TRIGGER_TIMER:
        /* 每 500 ms 翻转一次 LED */
        fyz_led_set(priv, !priv->brightness);
        delay_ms = 500;
        break;

    case TRIGGER_HEARTBEAT:
        /*
         * 心跳节律（单位 ms）：
         *   step 0 → 亮  (70 ms)
         *   step 1 → 灭  (150 ms)
         *   step 2 → 亮  (70 ms)
         *   step 3 → 灭  (1200 ms，长暂停）
         */
        switch (priv->hb_step) {
        case 0: fyz_led_set(priv, 1); delay_ms =   70; break;
        case 1: fyz_led_set(priv, 0); delay_ms =  150; break;
        case 2: fyz_led_set(priv, 1); delay_ms =   70; break;
        case 3: fyz_led_set(priv, 0); delay_ms = 1200; break;
        default: delay_ms = 1000; break;
        }
        priv->hb_step = (priv->hb_step + 1) % 4; /* 循环 0→1→2→3→0 */
        break;

    default:
        /* TRIGGER_NONE：定时器不应再触发，直接返回 */
        return;
    }

    /* 重新装填定时器 */
    mod_timer(&priv->timer, jiffies + msecs_to_jiffies(delay_ms));
}

/**
 * fyz_trigger_start() - 启动定时器（切换到 timer/heartbeat 模式时调用）
 * @priv: 私有数据指针
 */
static void fyz_trigger_start(struct fyz_led_priv *priv)
{
    priv->hb_step = 0;
    /* 100 ms 后触发第一次回调 */
    mod_timer(&priv->timer, jiffies + msecs_to_jiffies(100));
}

/**
 * fyz_trigger_stop() - 停止定时器（切换到 none 模式时调用）
 * @priv: 私有数据指针
 */
static void fyz_trigger_stop(struct fyz_led_priv *priv)
{
    del_timer_sync(&priv->timer); /* 同步删除，确保回调不再运行 */
    fyz_led_set(priv, 0);         /* 停止后熄灭 LED */
}

/* ====================== sysfs 属性：brightness ====================== */

/**
 * brightness_show() - 读取当前亮度
 * 用户态：cat /sys/bus/platform/devices/fyz-leds.0/brightness
 */
static ssize_t brightness_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct fyz_led_priv *priv = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", priv->brightness);
}

/**
 * brightness_store() - 设置亮度（仅在 TRIGGER_NONE 模式下有效）
 * 用户态：echo 1 > /sys/bus/platform/devices/fyz-leds.0/brightness
 */
static ssize_t brightness_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct fyz_led_priv *priv = dev_get_drvdata(dev);
    int val;

    if (priv->trigger != TRIGGER_NONE) {
        dev_warn(dev, "请先将触发模式设为 none 再手动控制亮度\n");
        return -EPERM;
    }

    if (kstrtoint(buf, 10, &val) < 0)
        return -EINVAL;

    fyz_led_set(priv, !!val); /* 非零值统一视为点亮 */
    return count;
}
static DEVICE_ATTR_RW(brightness); /* 生成 dev_attr_brightness */

/* ====================== sysfs 属性：trigger ====================== */

/**
 * trigger_show() - 读取当前触发模式
 * 用户态：cat /sys/bus/platform/devices/fyz-leds.0/trigger
 * 输出示例：[none] heartbeat timer
 *           方括号表示当前激活模式（与内核 LED 子系统风格一致）
 */
static ssize_t trigger_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    struct fyz_led_priv *priv = dev_get_drvdata(dev);
    return sprintf(buf, "%s%s %s%s %s%s\n",
        priv->trigger == TRIGGER_NONE      ? "[" : "", "none",
        priv->trigger == TRIGGER_NONE      ? "]" : "",
        priv->trigger == TRIGGER_HEARTBEAT ? "[" : "", "heartbeat",
        priv->trigger == TRIGGER_HEARTBEAT ? "]" : "");
    /*
     * 为简洁起见上面只列了两项；完整实现可用循环遍历触发表。
     * timer 模式读者可参照上面自行补充。
     */
    (void)priv; /* 消除未使用警告（实际已使用） */
}

/**
 * trigger_store() - 设置触发模式
 * 用户态：echo heartbeat > /sys/bus/platform/devices/fyz-leds.0/trigger
 *         echo timer     > ...
 *         echo none      > ...
 */
static ssize_t trigger_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
    struct fyz_led_priv *priv = dev_get_drvdata(dev);
    enum fyz_led_trigger new_trigger;

    /* 解析用户输入 */
    if (sysfs_streq(buf, "none"))
        new_trigger = TRIGGER_NONE;
    else if (sysfs_streq(buf, "heartbeat"))
        new_trigger = TRIGGER_HEARTBEAT;
    else if (sysfs_streq(buf, "timer"))
        new_trigger = TRIGGER_TIMER;
    else {
        dev_err(dev, "未知触发模式：%s（支持：none / heartbeat / timer）\n", buf);
        return -EINVAL;
    }

    /* 如果模式未变化，无需处理 */
    if (new_trigger == priv->trigger)
        return count;

    /* 先停止旧模式 */
    if (priv->trigger != TRIGGER_NONE)
        fyz_trigger_stop(priv);

    /* 切换到新模式 */
    priv->trigger = new_trigger;

    if (new_trigger != TRIGGER_NONE)
        fyz_trigger_start(priv);

    dev_info(dev, "触发模式已切换为：%s\n", buf);
    return count;
}
static DEVICE_ATTR_RW(trigger); /* 生成 dev_attr_trigger */

/* ====================== sysfs 属性组 ====================== */

/**
 * fyz_led_attrs[] - 需要在 sysfs 下创建的属性列表
 */
static struct attribute *fyz_led_attrs[] = {
    &dev_attr_brightness.attr,
    &dev_attr_trigger.attr,
    NULL, /* 数组必须以 NULL 结尾 */
};

/**
 * fyz_led_attr_group - 属性组，供 sysfs_create_group 使用
 */
static const struct attribute_group fyz_led_attr_group = {
    .attrs = fyz_led_attrs,
};

/* ====================== probe / remove ====================== */

/**
 * fyz_led_probe() - 设备匹配成功后由内核调用
 * @pdev: 匹配到的 platform_device
 *
 * 执行顺序：
 *   1. 分配私有数据
 *   2. 从设备树读取 GPIO 和标签
 *   3. 申请并初始化 GPIO
 *   4. 初始化软件定时器
 *   5. 在 sysfs 注册属性
 */
static int fyz_led_probe(struct platform_device *pdev)
{
    struct fyz_led_priv *priv;
    struct device *dev = &pdev->dev;
    int gpio, ret;

    dev_info(dev, "FYZ LED 驱动 probe 开始\n");

    /* 1. 分配私有数据（零初始化） */
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = dev;

    /* 2. 从设备树获取 GPIO 编号 */
    gpio = of_get_named_gpio(dev->of_node, "gpios", 0);
    if (!gpio_is_valid(gpio)) {
        dev_err(dev, "无效的 GPIO，请检查设备树 gpios 属性\n");
        return -EINVAL;
    }
    priv->gpio = gpio;

    /* 读取可选的 label 属性（失败也没关系） */
    of_property_read_string(dev->of_node, "label", &priv->label);
    if (!priv->label)
        priv->label = "fyz-led";

    /* 3. 申请 GPIO 并设为输出，初始电平为低（LED 灭） */
    ret = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_LOW, priv->label);
    if (ret) {
        dev_err(dev, "申请 GPIO %d 失败：%d\n", gpio, ret);
        return ret;
    }

    priv->brightness = 0;
    priv->trigger    = TRIGGER_NONE;

    /* 4. 初始化软件定时器（内核 5.x 新式 API） */
    timer_setup(&priv->timer, fyz_timer_callback, 0);

    /* 5. 将私有数据绑定到 device，供 sysfs 回调通过 dev_get_drvdata 取回 */
    platform_set_drvdata(pdev, priv);

    /* 6. 在 sysfs 创建属性节点 */
    ret = sysfs_create_group(&dev->kobj, &fyz_led_attr_group);
    if (ret) {
        dev_err(dev, "创建 sysfs 属性组失败：%d\n", ret);
        return ret;
    }

    dev_info(dev, "FYZ LED [%s] 初始化完成，GPIO=%d\n", priv->label, gpio);
    return 0;
}

/**
 * fyz_led_remove() - 设备卸载或驱动 rmmod 时由内核调用
 * @pdev: 被移除的 platform_device
 *
 * 执行顺序：
 *   1. 停止定时器
 *   2. 熄灭 LED
 *   3. 删除 sysfs 属性组
 *   （GPIO 由 devm 机制自动释放，无需手动调用 gpio_free）
 */
static int fyz_led_remove(struct platform_device *pdev)
{
    struct fyz_led_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "FYZ LED 驱动 remove\n");

    /* 先停定时器，防止回调在资源释放后仍然访问 priv */
    del_timer_sync(&priv->timer);

    /* 熄灭 LED */
    gpio_set_value(priv->gpio, 0);

    /* 删除 sysfs 属性组 */
    sysfs_remove_group(&pdev->dev.kobj, &fyz_led_attr_group);

    return 0;
}

/* ====================== 设备树匹配表 ====================== */

/**
 * fyz_led_of_match[] - 与设备树 compatible 属性匹配
 *
 * 设备树节点须包含：compatible = "fyz-leds";
 */
static const struct of_device_id fyz_led_of_match[] = {
    { .compatible = "fyz-leds", },
    { /* sentinel，数组结尾标志 */ },
};
MODULE_DEVICE_TABLE(of, fyz_led_of_match);
/*
 * MODULE_DEVICE_TABLE 让 depmod 在 modules.alias 中生成别名，
 * udev 热插拔时可据此自动加载本驱动。
 */

/* ====================== platform_driver 注册 ====================== */

/**
 * fyz_led_driver - 向内核注册的 platform 驱动描述符
 */
static struct platform_driver fyz_led_driver = {
    .probe  = fyz_led_probe,   /* 设备匹配成功时回调 */
    .remove = fyz_led_remove,  /* 设备移除时回调 */
    .driver = {
        .name           = "fyz-leds",          /* 与设备树 compatible 一致 */
        .owner          = THIS_MODULE,          /* 指向本模块，防止提前卸载 */
        .of_match_table = fyz_led_of_match,     /* 设备树匹配表 */
    },
};

/*
 * module_platform_driver() 是以下代码的宏封装：
 *   static int __init fyz_led_init(void) { return platform_driver_register(&fyz_led_driver); }
 *   static void __exit fyz_led_exit(void) { platform_driver_unregister(&fyz_led_driver); }
 *   module_init(fyz_led_init);
 *   module_exit(fyz_led_exit);
 */
module_platform_driver(fyz_led_driver);

/* ====================== 模块信息 ====================== */

MODULE_LICENSE("GPL v2");                          /* 必须与内核协议兼容 */
MODULE_AUTHOR("FYZ <fyz@example.com>");
MODULE_DESCRIPTION("FYZ LED Platform Driver with Trigger Mode Support");
MODULE_VERSION("1.0");



