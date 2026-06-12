#ifndef __OLED_DISPLAY_H
#define __OLED_DISPLAY_H

#include <stdint.h>

/**
 * @brief OLED 界面显示模块
 *
 * 提供系统状态显示功能，包括：
 * - 开机欢迎界面（版本号）
 * - 主状态界面（IP、端口、客户端、温度、LED状态）
 * - 命令帮助界面
 * - 消息提示界面
 */

#define OLED_DISPLAY_VERSION "GATE-V1.0.0"

/**
 * @brief 初始化 OLED 显示模块
 * @param i2c_bus I2C总线编号
 * @param i2c_addr I2C设备地址
 * @return 0: 成功, -1: 失败
 */
int oled_display_init(int i2c_bus, uint8_t i2c_addr);

/**
 * @brief 异步显示开机欢迎界面
 * @param version 版本号字符串
 * @return 0: 成功, -1: 失败
 *
 * @note 在后台线程中显示欢迎界面，不会阻塞主程序执行
 */
int oled_display_welcome_async(const char *version);

/**
 * @brief 检查欢迎界面是否正在显示
 * @return 1: 正在显示, 0: 已完成
 */
int oled_display_is_welcome_running(void);

/**
 * @brief 显示系统主界面
 * @param ip_address IP地址字符串
 * @param port 端口号
 * @param client_count 客户端数量
 * @param temperature 温度（摄氏度 * 10）
 * @param led_status LED状态（0: 关闭, 1: 打开）
 */
void oled_display_main(const char *ip_address, int port,
                       int client_count, int temperature, int led_status);

/**
 * @brief 显示命令帮助界面
 */
void oled_display_help(void);

/**
 * @brief 显示消息提示界面
 * @param title 标题
 * @param message 消息内容
 * @param timeout 显示时长（秒），0表示不自动消失
 */
void oled_display_message(const char *title, const char *message, int timeout);

/**
 * @brief 显示接收到的命令
 * @param cmd 命令字符串
 */
void oled_display_command(const char *cmd);

/**
 * @brief 更新显示缓存到屏幕
 */
void oled_display_update(void);

/**
 * @brief 清屏
 */
void oled_display_clear(void);

/**
 * @brief 关闭 OLED 显示
 */
void oled_display_deinit(void);

#endif
