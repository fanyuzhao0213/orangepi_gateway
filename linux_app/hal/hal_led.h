#ifndef HAL_LED_H
#define HAL_LED_H

/**
 * LED 控制函数接口
 * 基于 Orange Pi sysfs LED 框架
 *
 * 支持两种 LED 路径:
 *   - /sys/devices/platform/fyz_led  (FYZ 自定义驱动)
 *   - /sys/class/leds/green_led     (标准路径)
 */

/* LED 触发模式 */
typedef enum {
    LED_TRIGGER_NONE      = 0,  /* 手动控制 */
    LED_TRIGGER_HEARTBEAT = 1,  /* 心跳模式 */
    LED_TRIGGER_TIMER     = 2   /* 定时器模式 */
} led_trigger_mode_t;

/**
 * @brief 初始化 LED
 * @param gpio_pin: LED 对应的 GPIO 引脚编号（用于兼容 GPIO 控制）
 * @return 0: 成功, -1: 失败
 */
int hal_led_init(int gpio_pin);

/**
 * @brief 点亮 LED
 * @param gpio_pin: LED 对应的 GPIO 引脚编号
 * @return 0: 成功, -1: 失败
 */
int hal_led_on(int gpio_pin);

/**
 * @brief 熄灭 LED
 * @param gpio_pin: LED 对应的 GPIO 引脚编号
 * @return 0: 成功, -1: 失败
 */
int hal_led_off(int gpio_pin);

/**
 * @brief 切换 LED 状态 (亮→灭 或 灭→亮)
 * @param gpio_pin: LED 对应的 GPIO 引脚编号
 * @return 0: 成功, -1: 失败
 */
int hal_led_toggle(int gpio_pin);

/**
 * @brief 设置 LED 触发模式
 * @param mode: 触发模式 (LED_TRIGGER_NONE/HEARTBEAT/TIMER)
 * @return 0: 成功, -1: 失败
 */
int hal_led_set_trigger(led_trigger_mode_t mode);

/**
 * @brief 设置 LED 触发模式（带时间参数）
 * @param mode: 触发模式
 * @param delay_on_ms: 亮起持续时间(ms)，仅 TIMER 模式有效
 * @param delay_off_ms: 熄灭持续时间(ms)，仅 TIMER 模式有效
 * @return 0: 成功, -1: 失败
 */
int hal_led_set_trigger_with_delay(led_trigger_mode_t mode, int delay_on_ms, int delay_off_ms);

/**
 * @brief 获取当前 LED 触发模式
 * @param mode: 输出参数，存储当前模式
 * @return 0: 成功, -1: 失败
 */
int hal_led_get_trigger(led_trigger_mode_t *mode);

/**
 * @brief 获取当前 LED 亮度状态
 * @return 0: 熄灭, 1: 亮起, -1: 失败
 */
int hal_led_get_brightness(void);

#endif /* HAL_LED_H */