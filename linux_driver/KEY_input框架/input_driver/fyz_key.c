// SPDX-License-Identifier: GPL-2.0
/*
 * fyz_lradc_keys.c - 基于 input 子系统的 LRADC 按键驱动
 *
 * ─────────────────────────────────────────────────────────
 * 硬件原理：
 *
 *   3.3V
 *    │
 *   [R1]
 *    │──── ADC_IN0
 *   [R2]  (按键1按下时电压 = 3.3V * R2/(R1+R2) ≈ 1.1V)
 *    │
 *   [R3]  (按键2按下时电压 = 3.3V * R3/(R1+R2+R3) ≈ 1.65V)
 *    │
 *   GND
 *
 *   H618 内置 LRADC（Low Resolution ADC，低速ADC），6位精度，
 *   参考电压 1.8V，最多支持 64 个采样级别。
 *   按键按下时，ADC 采到对应电压，驱动通过查表匹配到对应按键。
 *
 * ─────────────────────────────────────────────────────────
 * input 子系统调用链：
 *
 *   硬件中断
 *      │
 *   fyz_lradc_irq()          ← 读寄存器，判断按下/抬起
 *      │
 *   input_report_key()        ← 向 input 核心上报按键事件
 *      │
 *   input_sync()              ← 通知 input 核心事件上报完毕
 *      │
 *   input 核心层              ← 分发给所有监听该设备的进程
 *      │
 *   /dev/input/eventX         ← 应用层通过 read/poll 读取
 *      │
 *   evtest / Qt / Android     ← 用户程序处理按键
 *
 * ─────────────────────────────────────────────────────────
 * 对应设备树节点：
 *
 *   lradc@5070800 {
 *       compatible = "allwinner,sun50i-h616-lradc";
 *       button-key1 {
 *           linux,code = <3>;       // KEY_2
 *           channel    = <0>;       // ADC 通道0
 *           voltage    = <1100000>; // 1.1V（单位 uV）
 *       };
 *       button-key2 {
 *           linux,code = <29>;      // KEY_LEFTCTRL
 *           channel    = <0>;
 *           voltage    = <1650000>; // 1.65V
 *       };
 *   };
 *
 * ─────────────────────────────────────────────────────────
 * 编译方法：
 *   obj-m += fyz_lradc_keys.o
 *   make -C /lib/modules/$(uname -r)/build M=$(PWD) modules
 */

#include <linux/module.h>           /* module_init/exit、MODULE_* 宏 */
#include <linux/platform_device.h>  /* platform_driver、platform_device */
#include <linux/input.h>            /* input_dev、input_report_key、input_sync */
#include <linux/interrupt.h>        /* request_irq、IRQ_HANDLED、irqreturn_t */
#include <linux/io.h>               /* ioremap、readl、writel */
#include <linux/of.h>               /* of_property_read_u32、for_each_child_of_node */
#include <linux/clk.h>              /* clk_get、clk_prepare_enable */
#include <linux/reset.h>            /* reset_control_deassert */
#include <linux/slab.h>             /* devm_kzalloc、devm_kcalloc */
#include <linux/delay.h>      /* 添加 udelay、mdelay 等延迟函数 */
#include <linux/kernel.h>     /* 添加 max、min 等宏定义 */

/* ================================================================
 * 一、LRADC 寄存器地址偏移（相对于 ioremap 基址）
 *
 * 参考：全志 H616/H618 用户手册 LRADC 章节
 * ================================================================ */

#define LRADC_CTRL    0x00  /* 控制寄存器：配置采样率、使能 LRADC */
#define LRADC_INTC    0x04  /* 中断控制寄存器：使能各通道中断 */
#define LRADC_INTS    0x08  /* 中断状态寄存器：读取并清除中断标志 */
#define LRADC_DATA0   0x0C  /* 通道0 ADC 采样数据寄存器（低6位有效） */

/* ----------------------------------------------------------------
 * LRADC_CTRL 寄存器位域
 * ---------------------------------------------------------------- */
#define LRADC_EN           BIT(0)   /* bit0: 1=使能 LRADC，0=关闭 */
#define LRADC_SAMPLE_250HZ (3 << 2) /* bit[3:2]=11: 采样率 250Hz
                                     * 其他值: 32Hz/62Hz/125Hz */
#define LRADC_LEVELA_B_CNT (1 << 8) /* bit[9:8]: 电平稳定计数
                                     * 用于去抖，值越大延迟越长 */

/* ----------------------------------------------------------------
 * LRADC_INTC 中断控制寄存器位域
 * ---------------------------------------------------------------- */
#define LRADC_CHAN0_KEYUP_IRQ   BIT(4) /* bit4: 使能通道0按键抬起中断 */
#define LRADC_CHAN0_KEYDOWN_IRQ BIT(0) /* bit0: 使能通道0按键按下中断 */

/* ----------------------------------------------------------------
 * LRADC_INTS 中断状态寄存器位域
 * 读到1表示该中断发生，写1清除（写1清零，Write-1-Clear）
 * ---------------------------------------------------------------- */
#define LRADC_CHAN0_KEYUP_PEND   BIT(4) /* bit4: 通道0按键抬起中断待处理 */
#define LRADC_CHAN0_KEYDOWN_PEND BIT(0) /* bit0: 通道0按键按下中断待处理 */

/* ----------------------------------------------------------------
 * ADC 参数常量
 * ---------------------------------------------------------------- */
#define LRADC_VREF           1800000 /* 参考电压 1.8V，单位 uV */
#define LRADC_RESOLUTION     64      /* 6位ADC：2^6 = 64 个采样级别（0~63） */
#define LRADC_VOLTAGE_MARGIN 50000     /* 电压匹配容差 ±50mV（单位 uV）
                                      * ADC 误差 + 电阻精度误差，留余量 */

/* ================================================================
 * 二、驱动数据结构
 * ================================================================ */

/**
 * struct fyz_lradc_button - 描述一个物理按键
 *
 * 从设备树解析得到，每个按键对应设备树中的一个子节点：
 *   button-key1 { linux,code=<3>; channel=<0>; voltage=<1100000>; };
 *
 * @linux_code: 上报给 input 子系统的键值（如 KEY_2=3）
 *              完整键值定义见 include/uapi/linux/input-event-codes.h
 * @channel:    使用的 ADC 通道号（当前硬件只有 channel 0）
 * @voltage:    该按键按下时 ADC 引脚的期望电压，单位 uV
 * @active:     当前是否处于按下状态（防止重复上报按下事件）
 */
struct fyz_lradc_button {
    int  linux_code; /* 键值，对应 KEY_XXX 宏 */
    int  channel;    /* ADC 通道号 */
    int  voltage;    /* 期望电压（uV） */
    bool active;     /* true=当前处于按下状态 */
};

/**
 * struct fyz_lradc_priv - 驱动全局私有数据
 *
 * probe 时分配，通过 platform_set_drvdata 绑定到设备，
 * 后续所有函数通过 platform_get_drvdata 或 dev_id 取回。
 *
 * @dev:         指向 &platform_device.dev，用于 dev_err/devm_* 等
 * @base:        ioremap 后的寄存器虚拟基地址
 * @clk:         LRADC 工作时钟句柄
 * @reset:       复位控制句柄（H618 需要先解除复位才能访问寄存器）
 * @input:       向 input 子系统注册的 input_dev 指针
 * @buttons:     按键数组首地址（从设备树解析得到）
 * @num_buttons: 按键数量
 * @irq:         从设备树/平台资源获取的硬件中断号
 */
struct fyz_lradc_priv {
    struct device           *dev;
    void __iomem            *base;        /* __iomem 提示编译器这是IO内存 */
    struct clk              *clk;
    struct reset_control    *reset;
    struct input_dev        *input;
    struct fyz_lradc_button *buttons;
    int                      num_buttons;
    int                      irq;

    /* 新增：连续采样相关字段 */
    int                      sample_buffer[3];   /* 采样缓冲区 */
    int                      sample_index;       /* 当前采样索引 */
    struct timer_list       sample_timer;       /* 采样定时器 */
    int                     pending_voltage;    /* 待处理的电压值 */
    bool                    sampling;           /* 是否正在采样中 */
};

/* ================================================================
 * 三、寄存器读写封装
 *
 * 封装为内联函数而非直接调用 readl/writel，好处：
 *   1. 代码更简洁，不需要每次写 priv->base + offset
 *   2. 未来如果需要加调试打印，只改这里即可
 * ================================================================ */

/**
 * lradc_readl() - 读取一个 32 位寄存器
 * @priv:   驱动私有数据（含寄存器基址）
 * @offset: 寄存器相对基址的字节偏移
 *
 * readl() 会插入必要的内存屏障，保证 IO 访问顺序。
 */
static inline u32 lradc_readl(struct fyz_lradc_priv *priv, u32 offset)
{
    return readl(priv->base + offset);
}

/**
 * lradc_writel() - 写入一个 32 位寄存器
 * @priv:   驱动私有数据
 * @val:    要写入的值
 * @offset: 寄存器偏移
 *
 * writel() 同样保证写操作到达硬件后才返回。
 */
static inline void lradc_writel(struct fyz_lradc_priv *priv,
                                 u32 val, u32 offset)
{
    writel(val, priv->base + offset);
}

/* ================================================================
 * 四、ADC 数值处理
 * ================================================================ */

/**
 * lradc_adc_to_voltage() - ADC 原始值 → 电压（uV）
 * @adc_val: ADC 采样原始值，范围 0~63（6位）
 *
 * 转换公式：
 *   voltage(uV) = adc_val * VREF(uV) / RESOLUTION
 *               = adc_val * 1800000 / 64
 *               = adc_val * 28125
 *
 * 例：按键1电压 1.1V → adc_val ≈ 1100000/28125 ≈ 39
 *     按键2电压 1.65V → adc_val ≈ 1650000/28125 ≈ 58
 *
 * 返回：电压值，单位 uV
 */
static int lradc_adc_to_voltage(int adc_val)
{
    return adc_val * LRADC_VREF / LRADC_RESOLUTION;
}

/**
 * lradc_find_button() - 根据当前电压查找匹配的按键
 * @priv:    驱动私有数据
 * @voltage: 当前 ADC 测量到的电压（uV）
 *
 * 遍历所有已注册按键，找到电压差在容差范围内的第一个按键。
 * 容差 LRADC_VOLTAGE_MARGIN = ±100mV，用于吸收：
 *   - ADC 量化误差（6位精度约 ±14mV）
 *   - 电阻精度误差（1% 电阻约 ±10mV）
 *   - 温度漂移等
 *
 * 返回：匹配的按键指针；未找到返回 NULL
 */
static struct fyz_lradc_button *lradc_find_button(struct fyz_lradc_priv *priv,
                                                   int voltage)
{
    int i;

    for (i = 0; i < priv->num_buttons; i++) {
        /* 计算当前电压与期望电压的差值绝对值 */
        int diff = priv->buttons[i].voltage - voltage;
        if (diff < 0)
            diff = -diff; /* abs()，避免引入 math 库 */

        if (diff <= LRADC_VOLTAGE_MARGIN)
            return &priv->buttons[i]; /* 找到匹配按键，立即返回 */
    }

    return NULL; /* 没有按键匹配此电压（可能是干扰或噪声） */
}

/**
 * lradc_read_stable_voltage() - 连续采样3次，返回稳定的电压值
 * @priv: 驱动私有数据
 *
 * 返回：稳定的电压值（uV），如果采样不稳定返回 -1
 */
static int lradc_read_stable_voltage(struct fyz_lradc_priv *priv)
{
    int samples[3];
    int i;
    int max_val, min_val;
    int stable_adc;

    /* 连续采样3次 */
    for (i = 0; i < 3; i++) {
        samples[i] = lradc_readl(priv, LRADC_DATA0) & 0x3F;

        /* 采样间隔：2ms，让ADC有足够时间稳定 */
        if (i < 2)
            udelay(2000);

        dev_dbg(priv->dev, "采样[%d]: ADC=%d\n", i, samples[i]);
    }

    /* 检查采样稳定性：最大值与最小值的差不能太大 */
    max_val = max(samples[0], max(samples[1], samples[2]));
    min_val = min(samples[0], min(samples[1], samples[2]));

    if (max_val - min_val > 3) {  /* ADC值跳动超过3个等级 */
        dev_dbg(priv->dev, "采样不稳定: 波动范围 %d-%d\n",
                min_val, max_val);
        return -1;  /* 采样不稳定，返回错误 */
    }

    /* 对3个值排序，取中间值（中位数更抗干扰）*/
    if (samples[0] > samples[1]) {
        int temp = samples[0];
        samples[0] = samples[1];
        samples[1] = temp;
    }
    if (samples[1] > samples[2]) {
        int temp = samples[1];
        samples[1] = samples[2];
        samples[2] = temp;
    }
    if (samples[0] > samples[1]) {
        int temp = samples[0];
        samples[0] = samples[1];
        samples[1] = temp;
    }

    stable_adc = samples[1];  /* 中位数 */

    dev_dbg(priv->dev, "稳定采样: ADC=%d, 电压=%d uV\n",
            stable_adc, lradc_adc_to_voltage(stable_adc));

    return lradc_adc_to_voltage(stable_adc);
}


/* ================================================================
 * 五、中断处理函数
 *
 * LRADC 在以下情况触发中断：
 *   - 按键按下（ADC 电压从高变低，稳定后触发 KEYDOWN）
 *   - 按键抬起（ADC 电压恢复到 VREF，触发 KEYUP）
 * ================================================================ */
/**
 * fyz_lradc_irq() - LRADC 硬件中断处理函数
 * @irq:    中断号（内核传入，一般不直接使用）
 * @dev_id: 注册中断时传入的私有指针（即 priv）
 *
 * 执行上下文：硬中断上下文，不能睡眠，不能调用可能阻塞的函数。
 *
 * 处理流程：
 *   1. 读 LRADC_INTS 获取中断原因
 *   2. 立即写回清除中断标志（Write-1-Clear），避免中断重入
 *   3. 若是 KEYDOWN：读 ADC 值 → 转电压 → 查表 → 上报按下
 *   4. 若是 KEYUP：遍历所有 active 按键 → 上报抬起
 *   5. 返回 IRQ_HANDLED 告知内核中断已处理
 */

static irqreturn_t fyz_lradc_irq(int irq, void *dev_id)
{
    struct fyz_lradc_priv   *priv = dev_id;
    struct fyz_lradc_button *btn;
    u32  ints;
    int  voltage;
    int  i;
    static unsigned long last_irq_time = 0;

    ints = lradc_readl(priv, LRADC_INTS);
    lradc_writel(priv, ints, LRADC_INTS);

    /* 简单的防抖：50ms内不处理新中断 */
    if (time_before(jiffies, last_irq_time + msecs_to_jiffies(50))) {
        dev_dbg(priv->dev, "中断防抖中，忽略\n");
        return IRQ_HANDLED;
    }

    /* ──────────────── 处理按键按下 ──────────────── */
    if (ints & LRADC_CHAN0_KEYDOWN_PEND) {

        /* 连续采样3次，验证电压稳定性 */
        voltage = lradc_read_stable_voltage(priv);

        if (voltage < 0) {
            dev_dbg(priv->dev, "采样不稳定，忽略本次按键\n");
            return IRQ_HANDLED;
        }

        /* 查找匹配的按键 */
        btn = lradc_find_button(priv, voltage);

        if (btn && !btn->active) {
            /* 可选：检查电压是否在合理范围 */
            if (abs(voltage - btn->voltage) > LRADC_VOLTAGE_MARGIN) {
                dev_dbg(priv->dev, "电压 %d 超出按键 %d 容差范围，忽略\n",
                        voltage, btn->linux_code);
                return IRQ_HANDLED;
            }

            btn->active = true;
            input_report_key(priv->input, btn->linux_code, 1);
            input_sync(priv->input);
            last_irq_time = jiffies;
            dev_info(priv->dev, "上报按下: keycode=%d (电压=%d uV)\n",
                     btn->linux_code, voltage);
        } else if (!btn) {
            dev_dbg(priv->dev, "未找到匹配按键，电压=%d uV（忽略）\n", voltage);
        }
    }

    /* ──────────────── 处理按键抬起 ──────────────── */
    if (ints & LRADC_CHAN0_KEYUP_PEND) {
        dev_dbg(priv->dev, "KEYUP 事件\n");

        for (i = 0; i < priv->num_buttons; i++) {
            if (priv->buttons[i].active) {
                priv->buttons[i].active = false;
                input_report_key(priv->input,
                                 priv->buttons[i].linux_code,
                                 0);
                input_sync(priv->input);
                dev_info(priv->dev, "上报抬起: keycode=%d\n",
                        priv->buttons[i].linux_code);
            }
        }
        last_irq_time = jiffies;
    }

    return IRQ_HANDLED;
}


/* ================================================================
 * 六、硬件初始化与反初始化
 * ================================================================ */

/**
 * fyz_lradc_hw_init() - 初始化 LRADC 硬件寄存器
 * @priv: 驱动私有数据
 *
 * 必须在时钟使能、复位解除之后调用，否则寄存器访问无效。
 *
 * 配置项：
 *   - 采样率 250Hz（每秒采样250次，按键响应约 4ms）
 *   - 使能 LRADC
 *   - 使能通道0的按下/抬起中断
 */
static void fyz_lradc_hw_init(struct fyz_lradc_priv *priv)
{
    /*
     * 写 LRADC_CTRL 控制寄存器：
     *   LRADC_EN          = bit0  = 1  → 使能 LRADC
     *   LRADC_SAMPLE_250HZ= bit[3:2]=11→ 采样率 250Hz
     *   LRADC_LEVELA_B_CNT= bit[9:8]=01→ 电平稳定计数，硬件去抖
     */
    lradc_writel(priv,
                 LRADC_EN | LRADC_SAMPLE_250HZ | LRADC_LEVELA_B_CNT,
                 LRADC_CTRL);

    /*
     * 写 LRADC_INTC 中断控制寄存器：
     *   同时使能按下和抬起两种中断，
     *   这样按键的完整生命周期（按下→抬起）都能被捕获。
     */
    lradc_writel(priv,
                 LRADC_CHAN0_KEYDOWN_IRQ | LRADC_CHAN0_KEYUP_IRQ,
                 LRADC_INTC);
}

/**
 * fyz_lradc_hw_deinit() - 关闭 LRADC 硬件
 * @priv: 驱动私有数据
 *
 * remove 时调用，确保硬件停止工作，不再产生中断。
 * 必须在 free_irq 之前调用，否则可能在 free_irq 期间产生中断。
 */
static void fyz_lradc_hw_deinit(struct fyz_lradc_priv *priv)
{
    lradc_writel(priv, 0, LRADC_INTC); /* 关闭所有中断使能 */
    lradc_writel(priv, 0, LRADC_CTRL); /* 禁用 LRADC，停止采样 */
}

/* ================================================================
 * 七、设备树解析
 * ================================================================ */

/**
 * fyz_lradc_parse_dt() - 从设备树解析所有按键配置
 * @priv: 驱动私有数据
 *
 * 遍历 lradc@5070800 的所有可用（status != "disabled"）子节点，
 * 读取每个子节点的三个必要属性：
 *   linux,code : 按键键值
 *   channel    : ADC 通道号
 *   voltage    : 按键电压（uV）
 *
 * 解析结果写入 priv->buttons 数组。
 *
 * 返回：0 成功；负数错误码
 */
static int fyz_lradc_parse_dt(struct fyz_lradc_priv *priv)
{
    struct device_node *np    = priv->dev->of_node; /* 本设备的DT节点 */
    struct device_node *child;                       /* 子节点迭代变量 */
    int count = 0;  /* 子节点总数 */
    int i     = 0;  /* 成功解析的按键计数 */

    /*
     * 第一遍遍历：只统计数量，用于后续内存分配。
     * for_each_available_child_of_node 会自动跳过
     * status = "disabled" 的子节点。
     */
    for_each_available_child_of_node(np, child)
        count++;

    if (count == 0) {
        dev_err(priv->dev, "设备树中没有找到可用的按键子节点\n");
        return -ENODEV; /* 没有设备 */
    }

    /*
     * 分配按键数组：
     * devm_kcalloc 分配 count 个 fyz_lradc_button 结构体，
     * 自动零初始化（所有 active 初始为 false）。
     * devm 管理的内存在设备卸载时自动释放，无需手动 kfree。
     */
    priv->buttons = devm_kcalloc(priv->dev, count,
                                  sizeof(*priv->buttons), GFP_KERNEL);
    if (!priv->buttons)
        return -ENOMEM; /* 内存分配失败 */

    priv->num_buttons = count;

    /*
     * 第二遍遍历：逐个解析子节点属性。
     * of_property_read_u32() 从设备树节点读取一个 u32 属性，
     * 失败返回负数错误码。
     */
    for_each_available_child_of_node(np, child) {
        u32 code, channel, voltage;

        /* 读取 linux,code 属性（按键键值） */
        if (of_property_read_u32(child, "linux,code", &code)) {
            dev_err(priv->dev,
                    "节点 %s 缺少 linux,code 属性，跳过\n",
                    child->name);
            continue; /* 跳过此节点，继续下一个 */
        }

        /* 读取 channel 属性（ADC 通道号） */
        if (of_property_read_u32(child, "channel", &channel)) {
            dev_err(priv->dev,
                    "节点 %s 缺少 channel 属性，跳过\n",
                    child->name);
            continue;
        }

        /* 读取 voltage 属性（期望电压，单位 uV） */
        if (of_property_read_u32(child, "voltage", &voltage)) {
            dev_err(priv->dev,
                    "节点 %s 缺少 voltage 属性，跳过\n",
                    child->name);
            continue;
        }

        /* 将解析结果存入按键数组 */
        priv->buttons[i].linux_code = code;
        priv->buttons[i].channel    = channel;
        priv->buttons[i].voltage    = voltage;
        priv->buttons[i].active     = false; /* 初始为未按下 */

        dev_info(priv->dev,
                 "注册按键[%d]: keycode=%u channel=%u voltage=%u uV\n",
                 i, code, channel, voltage);
        i++;
    }

    return 0;
}

/* ================================================================
 * 八、probe / remove
 *
 * probe  → 设备与驱动匹配成功时由内核调用（模块加载时）
 * remove → 设备移除或模块卸载时由内核调用
 * ================================================================ */

/**
 * fyz_lradc_probe() - 驱动探测函数
 * @pdev: 匹配到的 platform_device
 *
 * 初始化顺序（严格按依赖关系）：
 *   1. 分配私有数据
 *   2. ioremap 寄存器地址
 *   3. 获取并使能时钟（时钟使能后寄存器才可访问）
 *   4. 解除复位（复位解除后硬件进入工作状态）
 *   5. 解析设备树按键配置
 *   6. 获取中断号
 *   7. 分配并配置 input_dev
 *   8. 注册中断处理函数
 *   9. 初始化 LRADC 硬件寄存器
 *  10. 向 input 子系统注册设备
 *
 * 返回：0 成功；负数错误码（内核会打印并中止驱动加载）
 */
static int fyz_lradc_probe(struct platform_device *pdev)
{
    struct fyz_lradc_priv *priv;  /* 驱动私有数据 */
    struct input_dev      *input; /* input 设备 */
    struct resource       *res;   /* 平台资源（内存地址范围） */
    int ret; /* 错误返回值 */
    int i;   /* 循环变量 */

    dev_info(&pdev->dev, "FYZ LRADC Keys 驱动 probe 开始\n");

    /* ── 步骤1：分配私有数据结构 ── */
    /*
     * devm_kzalloc：设备管理的内存分配（零初始化）。
     * devm 系列函数在设备 detach 时自动释放，无需手动 kfree，
     * 大大简化错误路径处理。
     */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = &pdev->dev; /* 保存 device 指针，供后续 dev_err 使用 */

    /* ── 步骤2：映射寄存器地址 ── */
    /*
     * platform_get_resource：从设备树的 reg 属性获取内存资源。
     * devm_ioremap_resource：将物理地址映射为内核可访问的虚拟地址，
     * 并自动申请/注册该地址范围（防止其他驱动重复映射）。
     */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base)) {
        dev_err(&pdev->dev, "ioremap 失败\n");
        return PTR_ERR(priv->base); /* 将错误指针转为错误码 */
    }

    /* ── 步骤3：获取并使能时钟 ── */
    /*
     * LRADC 需要时钟才能工作。
     * devm_clk_get：从设备树 clocks 属性获取时钟句柄。
     * clk_prepare_enable：先 prepare（软件配置），再 enable（实际开启）。
     * 注意：clk_prepare_enable 失败时需要手动 disable，
     * 因为不是 devm 系列，所以错误路径要跳到 err_clk 手动清理。
     */
    priv->clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(priv->clk)) {
        dev_err(&pdev->dev, "获取时钟失败\n");
        return PTR_ERR(priv->clk);
    }
    ret = clk_prepare_enable(priv->clk);
    if (ret) {
        dev_err(&pdev->dev, "使能时钟失败: %d\n", ret);
        return ret;
    }

    /* ── 步骤4：解除硬件复位 ── */
    /*
     * H618 的外设上电后默认处于复位状态，需要软件解除。
     * devm_reset_control_get_optional_exclusive：
     *   - optional：设备树没有 resets 属性时不报错，返回 NULL
     *   - exclusive：独占此复位控制器（不共享）
     * IS_ERR_OR_NULL：同时检查指针是否为 NULL 或错误指针
     */
    priv->reset = devm_reset_control_get_optional_exclusive(&pdev->dev, NULL);
    if (!IS_ERR_OR_NULL(priv->reset)) {
        ret = reset_control_deassert(priv->reset); /* 1=释放复位，0=保持复位 */
        if (ret) {
            dev_err(&pdev->dev, "解除复位失败: %d\n", ret);
            goto err_clk; /* 跳到时钟关闭的错误处理 */
        }
    }

    /* ── 步骤5：解析设备树按键配置 ── */
    ret = fyz_lradc_parse_dt(priv);
    if (ret) {
        dev_err(&pdev->dev, "设备树解析失败: %d\n", ret);
        goto err_clk;
    }

    /* ── 步骤6：获取中断号 ── */
    /*
     * platform_get_irq：从设备树 interrupts 属性获取中断号。
     * 返回值 < 0 表示失败（没有配置中断）。
     */
    priv->irq = platform_get_irq(pdev, 0); /* 第0个中断 */
    if (priv->irq < 0) {
        dev_err(&pdev->dev, "获取中断号失败: %d\n", priv->irq);
        ret = priv->irq;
        goto err_clk;
    }
    dev_info(&pdev->dev, "使用中断号: %d\n", priv->irq);

    /* ── 步骤7：分配并配置 input_dev ── */
    /*
     * devm_input_allocate_device：分配一个 input_dev 结构体，
     * 设备卸载时自动调用 input_free_device 释放。
     * 必须在 input_register_device 之前设置好所有属性。
     */
    input = devm_input_allocate_device(&pdev->dev);
    if (!input) {
        dev_err(&pdev->dev, "分配 input_dev 失败\n");
        ret = -ENOMEM;
        goto err_clk;
    }
    priv->input = input;

    /* 设备名称：显示在 /proc/bus/input/devices 和 evtest 中 */
    input->name = "fyz-lradc-keys";

    /* 物理路径：描述设备在系统中的位置，格式自定义 */
    input->phys = "fyz-lradc/input0";

    /* 设备ID：总线类型、厂商ID、产品ID、版本 */
    input->id.bustype = BUS_HOST;   /* 片内外设，使用 HOST 总线类型 */
    input->id.vendor  = 0x0001;     /* 自定义厂商ID */
    input->id.product = 0x0001;     /* 自定义产品ID */
    input->id.version = 0x0100;     /* 版本 1.0 */

    /*
     * input_set_capability：向 input 核心声明本设备支持哪些事件。
     * 这里声明支持 EV_KEY 类型的各个键值。
     * 必须提前声明，否则 input 核心会丢弃未声明的事件。
     * evtest 显示的 "Supported events" 就来自这里的声明。
     */
    for (i = 0; i < priv->num_buttons; i++) {
        input_set_capability(input,              /* input_dev */
                             EV_KEY,             /* 事件类型：按键 */
                             priv->buttons[i].linux_code); /* 键值 */
    }

    /* ── 步骤8：注册中断处理函数 ── */
    /*
     * devm_request_irq：申请中断，devm 管理（卸载时自动 free_irq）。
     *   参数3: irqflags，0 表示使用设备树中指定的触发方式
     *   参数4: 中断名称，显示在 /proc/interrupts 中
     *   参数5: 传递给中断处理函数的私有数据（dev_id）
     */
    ret = devm_request_irq(&pdev->dev,
                            priv->irq,
                            fyz_lradc_irq,      /* 中断处理函数 */
                            0,
                            "fyz-lradc-keys",
                            priv);              /* 作为 dev_id 传入 */
    if (ret) {
        dev_err(&pdev->dev, "注册中断失败: %d\n", ret);
        goto err_clk;
    }

    /* ── 步骤9：初始化 LRADC 硬件寄存器 ── */
    fyz_lradc_hw_init(priv); /* 配置采样率，使能中断 */

    /* ── 步骤10：向 input 子系统注册设备 ── */
    /*
     * input_register_device：完成注册后：
     *   - /dev/input/eventX 节点出现
     *   - udev 触发热插拔事件
     *   - evtest 可以看到此设备
     * 注册失败需要先关闭硬件再返回错误。
     */
    ret = input_register_device(input);
    if (ret) {
        dev_err(&pdev->dev, "注册 input 设备失败: %d\n", ret);
        goto err_hw; /* 需要先关闭硬件 */
    }

    /*
     * 将私有数据绑定到 platform_device，
     * remove 时通过 platform_get_drvdata 取回。
     */
    platform_set_drvdata(pdev, priv);

    dev_info(&pdev->dev,
             "FYZ LRADC Keys 初始化完成，共注册 %d 个按键\n",
             priv->num_buttons);
    return 0; /* probe 成功 */

    /* ── 错误处理路径 ── */
err_hw:
    fyz_lradc_hw_deinit(priv); /* 关闭硬件，停止中断 */
err_clk:
    clk_disable_unprepare(priv->clk); /* 关闭时钟 */
    return ret;
}

/**
 * fyz_lradc_remove() - 驱动卸载函数
 * @pdev: 被移除的 platform_device
 *
 * 卸载顺序（与 probe 相反）：
 *   1. 关闭硬件（停止产生中断）
 *   2. 关闭时钟
 *   其余资源（irq、ioremap、input_dev、buttons数组）
 *   均由 devm 机制自动释放，无需手动处理。
 *
 * 返回：始终返回 0
 */
static int fyz_lradc_remove(struct platform_device *pdev)
{
    /* 取回 probe 时通过 platform_set_drvdata 存入的私有数据 */
    struct fyz_lradc_priv *priv = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "FYZ LRADC Keys 驱动 remove\n");

    /*
     * 必须先关闭硬件再释放中断。
     * 如果先 free_irq 再关硬件，可能在 free_irq 期间硬件
     * 继续产生中断，导致中断处理函数访问已释放的资源。
     * devm_request_irq 注册的中断会在此之后由 devm 自动释放。
     */
    fyz_lradc_hw_deinit(priv);

    /*
     * 手动关闭时钟（clk_prepare_enable 不是 devm 系列，
     * 需要配对调用 clk_disable_unprepare）。
     */
    clk_disable_unprepare(priv->clk);

    /*
     * 以下资源由 devm 自动释放（无需手动处理）：
     *   devm_request_irq      → free_irq
     *   devm_ioremap_resource → iounmap
     *   devm_input_allocate_device → input_unregister_device + input_free_device
     *   devm_kcalloc（buttons数组）→ kfree
     *   devm_kzalloc（priv本身）  → kfree
     */

    return 0;
}

/* ================================================================
 * 九、设备树匹配表
 *
 * 列出本驱动可以处理的设备树 compatible 字符串。
 * 内核启动时扫描设备树，若节点的 compatible 属性与此表匹配，
 * 则调用本驱动的 probe 函数。
 * ================================================================ */

static const struct of_device_id fyz_lradc_of_match[] = {
    { .compatible = "fyz,fyz-lradc-keys", },
    { /* sentinel */ },
};
/*
 * MODULE_DEVICE_TABLE：让 depmod 在 modules.alias 中生成别名，
 * udev 热插拔时可据此自动加载本驱动，无需手动 insmod。
 */
MODULE_DEVICE_TABLE(of, fyz_lradc_of_match);

/* ================================================================
 * 十、platform_driver 注册
 * ================================================================ */

static struct platform_driver fyz_lradc_driver = {
    .probe  = fyz_lradc_probe,  /* 设备匹配时调用 */
    .remove = fyz_lradc_remove, /* 设备移除时调用 */
    .driver = {
        .name           = "fyz-lradc-keys", /* 驱动名，显示在 /sys/bus/platform/drivers/ */
        .owner          = THIS_MODULE,       /* 防止驱动正在使用时被 rmmod */
        .of_match_table = fyz_lradc_of_match,/* 设备树匹配表 */
    },
};

/*
 * module_platform_driver() 展开为：
 *
 *   static int __init fyz_lradc_init(void) {
 *       return platform_driver_register(&fyz_lradc_driver);
 *   }
 *   static void __exit fyz_lradc_exit(void) {
 *       platform_driver_unregister(&fyz_lradc_driver);
 *   }
 *   module_init(fyz_lradc_init);
 *   module_exit(fyz_lradc_exit);
 *
 * 是标准的 platform 驱动注册宏，省去手动写 init/exit 的样板代码。
 */
module_platform_driver(fyz_lradc_driver);

/* ================================================================
 * 十一、模块元信息
 * ================================================================ */

MODULE_LICENSE("GPL v2");     /* 必须兼容 GPL，否则无法使用内核导出符号 */
MODULE_AUTHOR("FYZ");
MODULE_DESCRIPTION("FYZ LRADC Keys Driver based on Linux Input Subsystem");
MODULE_VERSION("1.0");

