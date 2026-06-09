#ifndef DEVICE_SERVICE_H
#define DEVICE_SERVICE_H

#include <pthread.h>

typedef struct {
    int led_status;       // 0: off, 1: on
    int client_count;     // 当前客户端数量
    int temperature;      // 温度 (摄氏度 * 10)
} device_status_t;

extern device_status_t g_device_status;
extern pthread_mutex_t g_status_mutex;

/**
 * @brief 初始化设备状态
 */
void device_status_init(void);

/**
 * @brief 设置 LED 状态
 * @param on 0: 关闭, 1: 打开
 */
void device_status_set_led(int on);

/**
 * @brief 获取 LED 状态
 * @return 0: 关闭, 1: 打开
 */
int device_status_get_led(void);

/**
 * @brief 设置客户端数量
 * @param count 客户端数量
 */
void device_status_set_client_count(int count);

/**
 * @brief 获取客户端数量
 * @return 客户端数量
 */
int device_status_get_client_count(void);

/**
 * @brief 设置温度
 * @param temp 温度 (摄氏度 * 10)
 */
void device_status_set_temperature(int temp);

/**
 * @brief 获取温度
 * @return 温度 (摄氏度 * 10)
 */
int device_status_get_temperature(void);

/**
 * @brief 获取完整的设备状态
 * @param status 输出参数，指向 device_status_t 结构体
 */
void device_status_get_all(device_status_t *status);

#endif


