#include <stdio.h>
#include <string.h>
#include "event_manager.h"
#include "../hal/hal_led.h"
#include "../service/device_service.h"
#include "../service/config_service.h"
#include "../service/log_service.h"

/**
 * @brief 处理 LED ON 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_led_on(char *response, int response_size)
{
    // 更新设备状态
    device_status_set_led(1);

    // 调用 HAL 层控制 LED
    hal_led_on(0);

    // 记录日志
    log_info("CMD: LED ON - LED已打开");

    // 返回响应
    snprintf(response, response_size, "OK\r\n");
    return 0;
}

/**
 * @brief 处理 LED OFF 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_led_off(char *response, int response_size)
{
    // 更新设备状态
    device_status_set_led(0);

    // 调用 HAL 层控制 LED
    hal_led_off(0);

    // 记录日志
    log_info("CMD: LED OFF - LED已关闭");

    // 返回响应
    snprintf(response, response_size, "OK\r\n");
    return 0;
}

/**
 * @brief 处理 GET STATUS 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_get_status(char *response, int response_size)
{
    device_status_t status;

    // 获取设备状态
    device_status_get_all(&status);

    // 记录日志
    log_info("CMD: GET STATUS - LED=%d, CLIENT=%d, TEMP=%d",
             status.led_status, status.client_count, status.temperature);

    // 返回响应
    snprintf(response, response_size,
             "LED=%d CLIENT=%d TEMP=%d\r\n",
             status.led_status, status.client_count, status.temperature);
    return 0;
}

/**
 * @brief 处理 GET TEMP 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_get_temp(char *response, int response_size)
{
    int temp = device_status_get_temperature();

    // 记录日志
    log_info("CMD: GET TEMP - TEMP=%d", temp);

    // 返回响应
    snprintf(response, response_size, "TEMP=%d\r\n", temp);
    return 0;
}

/**
 * @brief 处理 GET CLIENT 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_get_client(char *response, int response_size)
{
    int clients = device_status_get_client_count();

    // 记录日志
    log_info("CMD: GET CLIENT - CLIENT=%d", clients);

    // 返回响应
    snprintf(response, response_size, "CLIENT=%d\r\n", clients);
    return 0;
}

/**
 * @brief 处理 RELOAD CONFIG 命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_reload_config(char *response, int response_size)
{
    // 重新加载配置
    config_load("/home/orangepi/fyz_test/pi_ctrl.conf");

    // 记录日志
    log_info("CMD: RELOAD CONFIG - 配置已重新加载");

    // 返回响应
    snprintf(response, response_size, "CONFIG RELOADED\r\n");
    return 0;
}

/**
 * @brief 处理未知命令
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
static int handle_unknown(char *response, int response_size)
{
    // 记录日志
    log_warn("CMD: UNKNOWN - 收到未知命令");

    // 返回响应
    snprintf(response, response_size, "ERROR: Unknown command\r\n");
    return 0;
}

/**
 * @brief 处理命令事件
 * @param cmd 命令枚举值
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
int event_process(cmd_t cmd, char *response, int response_size)
{
    if (response == NULL || response_size <= 0) {
        return -1;
    }

    switch (cmd) {
        case CMD_LED_ON:
            return handle_led_on(response, response_size);
        case CMD_LED_OFF:
            return handle_led_off(response, response_size);
        case CMD_GET_STATUS:
            return handle_get_status(response, response_size);
        case CMD_GET_TEMP:
            return handle_get_temp(response, response_size);
        case CMD_GET_CLIENT:
            return handle_get_client(response, response_size);
        case CMD_RELOAD_CONFIG:
            return handle_reload_config(response, response_size);
        case CMD_UNKNOWN:
        default:
            return handle_unknown(response, response_size);
    }
}

