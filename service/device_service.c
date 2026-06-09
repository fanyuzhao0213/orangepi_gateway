#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "device_service.h"
#include "hal_led.h"
#include "hal_file.h"


// 全局变量定义
device_status_t g_device_status = {
    .led_status = 0,
    .client_count = 0,
    .temperature = 250  // 默认 25.0°C
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

    pthread_mutex_unlock(&g_status_mutex);

    // 初始化 HAL LED
    hal_led_init(0);
    hal_led_off(0);
}

/**
 * @brief 设置 LED 状态
 * @param on 0: 关闭, 1: 打开
 */
void device_status_set_led(int on)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.led_status = (on != 0) ? 1 : 0;
    pthread_mutex_unlock(&g_status_mutex);

    // 调用 HAL 层控制 LED
    if (on) {
        hal_led_on(0);
    } else {
        hal_led_off(0);
    }
}

/**
 * @brief 获取 LED 状态
 * @return 0: 关闭, 1: 打开
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
 * @brief 设置客户端数量
 * @param count 客户端数量
 */
void device_status_set_client_count(int count)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.client_count = (count >= 0) ? count : 0;
    pthread_mutex_unlock(&g_status_mutex);
}

/**
 * @brief 获取客户端数量
 * @return 客户端数量
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
 * @param temp 温度 (摄氏度 * 10)
 */
void device_status_set_temperature(int temp)
{
    pthread_mutex_lock(&g_status_mutex);
    g_device_status.temperature = temp;
    pthread_mutex_unlock(&g_status_mutex);
}

/**
 * @brief 获取温度
 * @return 温度 (摄氏度 * 10)
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
 * @param status 输出参数，指向 device_status_t 结构体
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

