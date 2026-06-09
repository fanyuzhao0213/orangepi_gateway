#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include "tcp_server.h"
#include "client_manager.h"
#include "message_queue.h"
#include "../service/log_service.h"
#include "../service/config_service.h"

#define MAX_EVENTS 32
#define DEFAULT_MAX_CLIENTS 20
#define BUFFER_SIZE 256

static int g_server_fd = -1;
static int g_epoll_fd = -1;
static int g_running = 0;
static pthread_t g_server_thread;
static int g_listen_port = 0;

/**
 * @brief 设置非阻塞模式
 * @param fd 文件描述符
 * @return 0: 成功, -1: 失败
 */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief 处理新连接
 * @param server_fd 服务器文件描述符
 * @return 0: 成功, -1: 失败
 */
static int handle_new_connection(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    // 设置非阻塞
    if (set_nonblocking(client_fd) < 0) {
        close(client_fd);
        return -1;
    }

    // 添加到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("epoll_ctl add client");
        close(client_fd);
        return -1;
    }

    // 添加到客户端管理器
    client_add(client_fd);

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    log_info("新客户端连接: %s:%d (fd=%d)",
             ip_str, ntohs(client_addr.sin_port), client_fd);

    return 0;
}

/**
 * @brief 处理客户端数据
 * @param client_fd 客户端文件描述符
 * @return 0: 成功, -1: 失败
 */
static int handle_client_data(int client_fd)
{
    char buffer[BUFFER_SIZE];
    ssize_t nread;
    message_t msg;

    nread = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (nread <= 0) {
        // 连接关闭或错误
        if (nread == 0) {
            log_info("客户端断开连接: fd=%d", client_fd);
        } else {
            perror("recv");
            log_error("recv 错误: fd=%d", client_fd);
        }

        // 从 epoll 移除
        epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        client_remove(client_fd);
        return -1;
    }

    buffer[nread] = '\0';
    log_info("recv: %s (fd=%d)", buffer, client_fd);

    // 封装消息
    msg.client_fd = client_fd;
    msg.data_len = nread;
    strncpy(msg.data, buffer, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data) - 1] = '\0';

    // 放入消息队列
    if (mq_push(&msg) < 0) {
        log_error("消息队列已满，丢弃消息: fd=%d", client_fd);
    }

    return 0;
}

/**
 * @brief 服务器线程函数
 * @param arg 参数
 * @return NULL
 */
static void* server_thread_func(void *arg)
{
    struct epoll_event events[MAX_EVENTS];

    log_info("TCP 服务器启动，监听端口: %d", g_listen_port);

    while (g_running) {
        int nfds = epoll_wait(g_epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == g_server_fd) {
                // 新连接
                handle_new_connection(g_server_fd);
            } else {
                // 客户端数据
                handle_client_data(events[i].data.fd);
            }
        }
    }

    log_info("TCP 服务器停止");
    return NULL;
}

/**
 * @brief 启动 TCP 服务器
 * @param port 监听端口
 * @return 0: 成功, -1: 失败
 */
int tcp_server_start(int port)
{
    struct sockaddr_in server_addr;
    struct epoll_event ev;
    int reuse = 1;

    if (g_running) {
        return 0;
    }

    // 保存端口号（用于日志显示）
    g_listen_port = port;

    // 创建 socket
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return -1;
    }

    // 设置 SO_REUSEADDR
    if (setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(g_server_fd);
        return -1;
    }

    // 绑定
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(g_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        return -1;
    }

    // 读取配置的最大客户端数
    int max_clients = atoi(config_get("max_clients", "20"));
    if (max_clients <= 0) {
        max_clients = DEFAULT_MAX_CLIENTS;
    }

    // 监听
    if (listen(g_server_fd, max_clients) < 0) {
        perror("listen");
        close(g_server_fd);
        return -1;
    }

    // 设置非阻塞
    if (set_nonblocking(g_server_fd) < 0) {
        close(g_server_fd);
        return -1;
    }

    // 创建 epoll
    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd < 0) {
        perror("epoll_create1");
        close(g_server_fd);
        return -1;
    }

    // 添加服务器 socket 到 epoll
    ev.events = EPOLLIN;
    ev.data.fd = g_server_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_server_fd, &ev) < 0) {
        perror("epoll_ctl add server");
        close(g_epoll_fd);
        close(g_server_fd);
        return -1;
    }

    g_running = 1;

    // 创建服务器线程（传递 NULL，线程内部使用全局变量 g_listen_port）
    if (pthread_create(&g_server_thread, NULL, server_thread_func, NULL) != 0) {
        perror("pthread_create");
        g_running = 0;
        close(g_epoll_fd);
        close(g_server_fd);
        return -1;
    }

    return 0;
}

/**
 * @brief 停止 TCP 服务器（不等待线程，由调用方负责等待）
 */
void tcp_server_stop(void)
{
    if (!g_running) {
        return;
    }

    g_running = 0;

    // 中断 epoll_wait
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }

    if (g_epoll_fd >= 0) {
        close(g_epoll_fd);
        g_epoll_fd = -1;
    }

    // 线程等待由调用方（main函数）负责
}

/**
 * @brief 获取服务器状态
 * @return 1: 运行中, 0: 已停止
 */
int tcp_server_is_running(void)
{
    return g_running;
}


