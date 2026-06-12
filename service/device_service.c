#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "device_service.h"
#include "../hal/hal_led.h"

/* ===== 终端颜色 ===== */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ===== 全局变量定义 ===== */
device_status_t g_device_status = {
    .led_status = 0,
    .client_count = 0,
    .temperature = 250,
    .led_trigger = LED_TRIGGER_NONE
};

pthread_mutex_t g_status_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 初始化设备状态
 */
void device_status_init(void)
{
    pthread_mutex_lock(&g_status_mutex);

    g_device_status.led_status = 0;
    g_device_status.client_count = 0;
    g_device_status.temperature = 250;
    g_device_status.led_trigger = LED_TRIGGER_NONE;

    pthread_mutex_unlock(&g_status_mutex);

    /* HAL LED 已在 system_init 中通过 hal_led_init() 初始化，此处仅设置默认状态 */
}

/**
 * @brief 设置 LED 状态
 * @note 心跳/定时器模式下不允许手动控制 LED
 */
void device_status_set_led(int on)
{
    pthread_mutex_lock(&g_status_mutex);
    led_trigger_mode_t trigger = g_device_status.led_trigger;
    if (trigger != LED_TRIGGER_NONE) {
        pthread_mutex_unlock(&g_status_mutex);
        printf(C_BOLD C_YELLOW "[设备] ⚠ 当前为 %s 模式，不允许手动控制\n" C_RESET,
               trigger == LED_TRIGGER_HEARTBEAT ? "heartbeat" : "timer");
        return;
    }
    g_device_status.led_status = (on != 0) ? 1 : 0;
    pthread_mutex_unlock(&g_status_mutex);

    if (on) {
        hal_led_on(0);
        printf(C_BOLD C_GREEN "[设备] ● LED 已点亮\n" C_RESET);
    } else {
        hal_led_off(0);
        printf(C_BOLD C_YELLOW "[设备] ○ LED 已熄灭\n" C_RESET);
    }
}

/**
 * @brief 获取 LED 状态
 */
int device_status_get_led(void)
{
    int status;
    pthread_mutex_lock(&g_status_mutex);
    status = g_device_status.led_status;
    pthread_mutex_unlock(&g_status_mutex);
    return status;
}

/**
 * @brief 设置 LED 触发模式
 */
void device_status_set_led_trigger(led_trigger_mode_t mode)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.led_trigger = mode;
    pthread_mutex_unlock(&g_status_mutex);

    hal_led_set_trigger(mode);

    const char *mode_name;
    switch (mode) {
        case LED_TRIGGER_HEARTBEAT:
            mode_name = "heartbeat";
            printf(C_BOLD C_CYAN "[设备] ♥ LED 触发模式: %s\n" C_RESET, mode_name);
            break;
        case LED_TRIGGER_TIMER:
            mode_name = "timer";
            printf(C_BOLD C_CYAN "[设备] ⏱ LED 触发模式: %s\n" C_RESET, mode_name);
            break;
        default:
            mode_name = "none";
            printf(C_BOLD C_CYAN "[设备] ● LED 触发模式: %s\n" C_RESET, mode_name);
            break;
    }
}

/**
 * @brief 获取 LED 触发模式
 */
led_trigger_mode_t device_status_get_led_trigger(void)
{
    led_trigger_mode_t mode;
    pthread_mutex_lock(&g_status_mutex);
    mode = g_device_status.led_trigger;
    pthread_mutex_unlock(&g_status_mutex);
    return mode;
}

/**
 * @brief 设置客户端数量
 */
void device_status_set_client_count(int count)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.client_count = (count >= 0) ? count : 0;
    pthread_mutex_unlock(&g_status_mutex);
}

/**
 * @brief 获取客户端数量
 */
int device_status_get_client_count(void)
{
    int count;
    pthread_mutex_lock(&g_status_mutex);
    count = g_device_status.client_count;
    pthread_mutex_unlock(&g_status_mutex);
    return count;
}

/**
 * @brief 设置温度
 */
void device_status_set_temperature(int temp)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.temperature = temp;
    pthread_mutex_unlock(&g_status_mutex);
}

/**
 * @brief 获取温度
 */
int device_status_get_temperature(void)
{
    int temp;
    pthread_mutex_lock(&g_status_mutex);
    temp = g_device_status.temperature;
    pthread_mutex_unlock(&g_status_mutex);
    return temp;
}

/**
 * @brief 获取完整的设备状态
 */
void device_status_get_all(device_status_t *status)
{
    if (status == NULL) {
        return;
    }

    pthread_mutex_lock(&g_status_mutex);
    memcpy(status, &g_device_status, sizeof(device_status_t));
    pthread_mutex_unlock(&g_status_mutex);
}