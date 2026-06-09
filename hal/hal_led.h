#ifndef HAL_LED_H
#define HAL_LED_H

/**
 * LED 控制函数接口
 * 基于 Orange Pi sysfs LED 框架
 */

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

#endif /* HAL_LED_H */


