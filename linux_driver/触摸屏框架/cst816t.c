#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>

#define DRV_NAME        "cst816t"
#define CST816T_ADDR    0x15

/* 寄存器 */
#define REG_GESTURE     0x01
#define REG_FINGER_NUM  0x02
#define REG_X_H         0x03
#define REG_X_L         0x04
#define REG_Y_H         0x05
#define REG_Y_L         0x06
#define REG_CHIP_ID     0xA7
#define REG_VERSION     0xA9

/* 手势 */
#define GESTURE_NONE    0x00
#define GESTURE_UP      0x01
#define GESTURE_DOWN    0x02
#define GESTURE_LEFT    0x03
#define GESTURE_RIGHT   0x04
#define GESTURE_CLICK   0x05
#define GESTURE_DCLICK  0x0B
#define GESTURE_LPRESS  0x0C

struct cst816t_priv {
    struct i2c_client   *client;
    struct input_dev    *input;
    struct gpio_desc    *reset;
    int                  irq;
};

/* ────────────── I2C 读写 ────────────── */

static int cst816t_read_reg(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;

    ret = i2c_master_send(client, &reg, 1);
    if (ret < 0) {
        dev_err(&client->dev, "i2c send reg 0x%02X failed: %d\n", reg, ret);
        return ret;
    }

    msleep(5);  /* 发完地址稍等一下再读 */

    ret = i2c_master_recv(client, buf, len);
    if (ret < 0) {
        dev_err(&client->dev, "i2c recv failed: %d\n", ret);
        return ret;
    }
    return 0;
}

/* ────────────── 硬件复位 ────────────── */

static void cst816t_reset(struct cst816t_priv *priv)
{
    dev_info(&priv->client->dev, "reset: pull low\n");
    gpiod_set_value(priv->reset, 0);   /* 拉低 */
    msleep(20);

    dev_info(&priv->client->dev, "reset: release high\n");
    gpiod_set_value(priv->reset, 1);   /* 拉高，芯片启动 */
    msleep(50);

    dev_info(&priv->client->dev, "reset done\n");
}

/* ────────────── 中断处理 ────────────── */

static irqreturn_t cst816t_irq_handler(int irq, void *dev_id)
{
    struct cst816t_priv *priv = dev_id;
    struct i2c_client   *client = priv->client;
    u8  buf[6];
    int ret;
    u8  finger_num, gesture;
    u16 x, y;

    /* 从 REG_GESTURE 开始连续读 6 字节 */
    ret = cst816t_read_reg(client, REG_GESTURE, buf, 6);
    if (ret < 0)
        return IRQ_HANDLED;

    gesture    = buf[0];
    finger_num = buf[1];
    x = ((buf[2] & 0x0F) << 8) | buf[3];
    y = ((buf[4] & 0x0F) << 8) | buf[5];

    dev_dbg(&client->dev,
            "gesture=0x%02X fingers=%d x=%d y=%d\n",
            gesture, finger_num, x, y);

    if (finger_num == 0) {
        /* 手指抬起 */
        input_mt_slot(priv->input, 0);
        input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, false);
        input_report_key(priv->input, BTN_TOUCH, 0);
    } else {
        /* 手指按下 */
        input_mt_slot(priv->input, 0);
        input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
        input_report_abs(priv->input, ABS_MT_POSITION_X, x);
        input_report_abs(priv->input, ABS_MT_POSITION_Y, y);
        input_report_key(priv->input, BTN_TOUCH, 1);
        input_report_abs(priv->input, ABS_X, x);
        input_report_abs(priv->input, ABS_Y, y);
    }

    input_sync(priv->input);
    return IRQ_HANDLED;
}

/* ────────────── probe / remove ────────────── */

static int cst816t_probe(struct i2c_client *client,
                          const struct i2c_device_id *id)
{
    struct cst816t_priv *priv;
    struct input_dev    *input;
    u8  chip_id = 0, version = 0;
    int ret;

    dev_info(&client->dev, "========== probe start ==========\n");
    dev_info(&client->dev, "I2C addr=0x%02X irq=%d\n",
             client->addr, client->irq);

    /* 分配私有数据 */
    priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;

    priv->client = client;
    i2c_set_clientdata(client, priv);

    /* 获取 RST GPIO */
    priv->reset = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->reset)) {
        ret = PTR_ERR(priv->reset);
        dev_err(&client->dev, "failed to get reset gpio: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "reset gpio ok\n");

    // /* 硬件复位 */
    // cst816t_reset(priv);

    // /* 复位后额外等待，确保芯片 I2C 就绪 */
    // msleep(100);
    // dev_info(&client->dev, "ready to read chip id\n");

    /* 读取芯片 ID */
    ret = cst816t_read_reg(client, REG_CHIP_ID, &chip_id, 1);
    if (ret < 0) {
        dev_err(&client->dev, "failed to read chip id: %d\n", ret);
        return ret;
    }

    cst816t_read_reg(client, REG_VERSION, &version, 1);
    dev_info(&client->dev, "chip_id=0x%02X version=0x%02X\n",
             chip_id, version);

    /* 注册 input 设备 */
    input = devm_input_allocate_device(&client->dev);
    if (!input) {
        dev_err(&client->dev, "failed to alloc input device\n");
        return -ENOMEM;
    }

    input->name       = "cst816t";
    input->id.bustype = BUS_I2C;
    input->dev.parent = &client->dev;

    /* 触摸范围和屏幕分辨率一致 */
    input_set_abs_params(input, ABS_X,              0, 240, 0, 0);
    input_set_abs_params(input, ABS_Y,              0, 280, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_X,  0, 240, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,  0, 280, 0, 0);

    input_mt_init_slots(input, 1, INPUT_MT_DIRECT);
    input_set_capability(input, EV_KEY, BTN_TOUCH);

    priv->input = input;

    ret = input_register_device(input);
    if (ret) {
        dev_err(&client->dev, "input_register_device failed: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "input device registered\n");

    /* 申请中断 */
    priv->irq = client->irq;
    if (priv->irq <= 0) {
        dev_err(&client->dev, "no irq configured\n");
        return -EINVAL;
    }

    ret = devm_request_threaded_irq(&client->dev, priv->irq,
                                    NULL, cst816t_irq_handler,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    DRV_NAME, priv);
    if (ret) {
        dev_err(&client->dev, "failed to request irq %d: %d\n",
                priv->irq, ret);
        return ret;
    }
    dev_info(&client->dev, "irq %d registered\n", priv->irq);
    dev_info(&client->dev, "========== probe success ==========\n");

    return 0;
}

static void cst816t_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "remove\n");
}

/* ────────────── 模块注册 ────────────── */

static const struct of_device_id cst816t_of_match[] = {
    { .compatible = "hynitron,cst816t" },
    { }
};
MODULE_DEVICE_TABLE(of, cst816t_of_match);

static const struct i2c_device_id cst816t_id[] = {
    { "cst816t", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, cst816t_id);

static struct i2c_driver cst816t_driver = {
    .driver = {
        .name           = DRV_NAME,
        .of_match_table = cst816t_of_match,
    },
    .probe    = cst816t_probe,
    .remove   = cst816t_remove,
    .id_table = cst816t_id,
};

module_i2c_driver(cst816t_driver);

MODULE_AUTHOR("FYZ");
MODULE_DESCRIPTION("CST816T I2C Touchscreen Driver");
MODULE_LICENSE("GPL");