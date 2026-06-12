#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stddef.h>

/**
 * @brief 初始化客户端管理器
 * @return 0: 成功, -1: 失败
 */
int client_manager_init(void);

/**
 * @brief 销毁客户端管理器
 */
void client_manager_destroy(void);

/**
 * @brief 添加客户端
 * @param fd 客户端文件描述符
 * @return 0: 成功, -1: 失败
 */
int client_add(int fd);

/**
 * @brief 移除客户端
 * @param fd 客户端文件描述符
 * @return 0: 成功, -1: 失败
 */
int client_remove(int fd);

/**
 * @brief 获取客户端数量
 * @return 客户端数量
 */
int client_count(void);

/**
 * @brief 向所有客户端广播消息
 * @param data 消息数据
 * @param len 数据长度
 * @return 发送的字节数
 */
int client_broadcast(const char *data, size_t len);

#endif /* CLIENT_MANAGER_H */


