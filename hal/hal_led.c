#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "hal_led.h"

#define LED_PATH "/sys/class/leds/green_led"
#define TRIGGER_FILE "trigger"
#define BRIGHTNESS_FILE "brightness"

/**
 * @brief 内部函数：写入文件内容
 * @param path: 文件路径
 * @param value: 要写入的值
 * @return 0: 成功, -1: 失败
 */
static int write_to_file(const char *path, const char *value)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("fopen");
        fprintf(stderr, "无法打开文件: %s\n", path);
        return -1;
    }

    if (fprintf(fp, "%s", value) < 0) {
        perror("fprintf");
        fprintf(stderr, "写入文件失败: %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/**
 * @brief 内部函数：读取文件内容
 * @param path: 文件路径
 * @param buffer: 缓冲区
 * @param size: 缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int read_from_file(const char *path, char *buffer, int size)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen");
        fprintf(stderr, "无法打开文件: %s\n", path);
        return -1;
    }

    if (fgets(buffer, size, fp) == NULL) {
        perror("fgets");
        fprintf(stderr, "读取文件失败: %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/**
 * @brief 初始化 LED
 * @param gpio_pin: LED 对应的 GPIO 引脚编号（sysfs 模式忽略此参数）
 * @return 0: 成功, -1: 失败
 */
int hal_led_init(int gpio_pin)
{
    // 检查 LED 设备是否存在
    if (access(LED_PATH, F_OK) != 0) {
        perror("access");
        fprintf(stderr, "LED 设备不存在: %s\n", LED_PATH);
        return -1;
    }

    // 默认关闭 LED
    return hal_led_off(gpio_pin);
}

/**
 * @brief 点亮 LED (常亮)
 * @param gpio_pin: LED 对应的 GPIO 引脚编号（sysfs 模式忽略此参数）
 * @return 0: 成功, -1: 失败
 */
int hal_led_on(int gpio_pin)
{
    char path[256];

    // 先设置 trigger 为 none（停止闪烁）
    snprintf(path, sizeof(path), "%s/%s", LED_PATH, TRIGGER_FILE);
    if (write_to_file(path, "none") < 0) {
        return -1;
    }

    // 设置亮度为 255（最亮）
    snprintf(path, sizeof(path), "%s/%s", LED_PATH, BRIGHTNESS_FILE);
    if (write_to_file(path, "255") < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief 熄灭 LED
 * @param gpio_pin: LED 对应的 GPIO 引脚编号（sysfs 模式忽略此参数）
 * @return 0: 成功, -1: 失败
 */
int hal_led_off(int gpio_pin)
{
    char path[256];

    // 先设置 trigger 为 none（停止闪烁）
    snprintf(path, sizeof(path), "%s/%s", LED_PATH, TRIGGER_FILE);
    if (write_to_file(path, "none") < 0) {
        return -1;
    }

    // 设置亮度为 0（熄灭）
    snprintf(path, sizeof(path), "%s/%s", LED_PATH, BRIGHTNESS_FILE);
    if (write_to_file(path, "0") < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief 切换 LED 状态 (亮→灭 或 灭→亮)
 * @param gpio_pin: LED 对应的 GPIO 引脚编号（sysfs 模式忽略此参数）
 * @return 0: 成功, -1: 失败
 */
int hal_led_toggle(int gpio_pin)
{
    char path[256];
    char buffer[16];
    int brightness;

    // 读取当前亮度
    snprintf(path, sizeof(path), "%s/%s", LED_PATH, BRIGHTNESS_FILE);
    if (read_from_file(path, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    // 转换并切换状态
    brightness = atoi(buffer);
    if (brightness > 0) {
        return hal_led_off(gpio_pin);
    } else {
        return hal_led_on(gpio_pin);
    }
}

