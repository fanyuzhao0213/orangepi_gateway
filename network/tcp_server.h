#ifndef TCP_SERVER_H
#define TCP_SERVER_H

/**
 * @brief 启动 TCP 服务器
 * @param port 监听端口
 * @return 0: 成功, -1: 失败
 */
int tcp_server_start(int port);

/**
 * @brief 停止 TCP 服务器
 */
void tcp_server_stop(void);

/**
 * @brief 获取服务器状态
 * @return 1: 运行中, 0: 已停止
 */
int tcp_server_is_running(void);

#endif /* TCP_SERVER_H */


