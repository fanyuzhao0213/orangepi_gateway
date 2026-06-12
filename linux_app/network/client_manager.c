#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "client_manager.h"
#include "../service/device_service.h"
#include "../service/config_service.h"

#define DEFAULT_MAX_CLIENTS 20

typedef struct {
    int *fds;
    int count;
    int max_count;
    pthread_mutex_t mutex;
} client_manager_t;

static client_manager_t g_client_manager;

/**
 * @brief 初始化客户端管理器
 * @return 0: 成功, -1: 失败
 */
int client_manager_init(void)
{
    // 从配置文件读取 max_clients
    g_client_manager.max_count = atoi(config_get("max_clients", "20"));
    if (g_client_manager.max_count <= 0) {
        g_client_manager.max_count = DEFAULT_MAX_CLIENTS;
    }

    // 动态分配客户端数组
    g_client_manager.fds = (int *)malloc(g_client_manager.max_count * sizeof(int));
    if (g_client_manager.fds == NULL) {
        return -1;
    }

    g_client_manager.count = 0;
    memset(g_client_manager.fds, -1, g_client_manager.max_count * sizeof(int));

    if (pthread_mutex_init(&g_client_manager.mutex, NULL) != 0) {
        free(g_client_manager.fds);
        return -1;
    }

    return 0;
}

/**
 * @brief 销毁客户端管理器
 */
void client_manager_destroy(void)
{
    pthread_mutex_destroy(&g_client_manager.mutex);
    if (g_client_manager.fds != NULL) {
        free(g_client_manager.fds);
        g_client_manager.fds = NULL;
    }
}

/**
 * @brief 添加客户端
 * @param fd 客户端文件描述符
 * @return 0: 成功, -1: 失败
 */
int client_add(int fd)
{
    if (fd < 0) {
        return -1;
    }

    pthread_mutex_lock(&g_client_manager.mutex);

    if (g_client_manager.count >= g_client_manager.max_count) {
        pthread_mutex_unlock(&g_client_manager.mutex);
        return -1;
    }

    // 添加到数组
    g_client_manager.fds[g_client_manager.count] = fd;
    g_client_manager.count++;

    // 更新设备状态中的客户端数量
    device_status_set_client_count(g_client_manager.count);

    pthread_mutex_unlock(&g_client_manager.mutex);
    return 0;
}

/**
 * @brief 移除客户端
 * @param fd 客户端文件描述符
 * @return 0: 成功, -1: 失败
 */
int client_remove(int fd)
{
    int i;
    int found = 0;

    if (fd < 0) {
        return -1;
    }

    pthread_mutex_lock(&g_client_manager.mutex);

    // 查找并移除
    for (i = 0; i < g_client_manager.count; i++) {
        if (g_client_manager.fds[i] == fd) {
            // 用最后一个元素覆盖
            g_client_manager.fds[i] = g_client_manager.fds[g_client_manager.count - 1];
            g_client_manager.fds[g_client_manager.count - 1] = -1;
            g_client_manager.count--;
            found = 1;
            break;
        }
    }

    if (found) {
        // 更新设备状态中的客户端数量
        device_status_set_client_count(g_client_manager.count);
    }

    pthread_mutex_unlock(&g_client_manager.mutex);
    return found ? 0 : -1;
}

/**
 * @brief 获取客户端数量
 * @return 客户端数量
 */
int client_count(void)
{
    int count;
    pthread_mutex_lock(&g_client_manager.mutex);
    count = g_client_manager.count;
    pthread_mutex_unlock(&g_client_manager.mutex);
    return count;
}

/**
 * @brief 向所有客户端广播消息
 * @param data 消息数据
 * @param len 数据长度
 * @return 发送的字节数
 */
int client_broadcast(const char *data, size_t len)
{
    int i;
    int total_sent = 0;

    if (data == NULL || len == 0) {
        return 0;
    }

    pthread_mutex_lock(&g_client_manager.mutex);

    for (i = 0; i < g_client_manager.count; i++) {
        int fd = g_client_manager.fds[i];
        ssize_t sent = write(fd, data, len);
        if (sent > 0) {
            total_sent += sent;
        }
        // 如果发送失败，关闭连接会在 epoll 事件中处理
    }

    pthread_mutex_unlock(&g_client_manager.mutex);
    return total_sent;
}


