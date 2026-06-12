#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "cmd_parser.h"

/**
 * @brief 处理命令事件
 * @param cmd 命令枚举值
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
int event_process(cmd_t cmd, char *response, int response_size);

/**
 * @brief 处理带原始命令字符串的事件（用于解析参数）
 * @param raw_cmd 原始命令字符串
 * @param response 输出响应缓冲区
 * @param response_size 响应缓冲区大小
 * @return 0: 成功, -1: 失败
 */
int event_process_raw(const char *raw_cmd, char *response, int response_size);

#endif /* EVENT_MANAGER_H */