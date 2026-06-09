/**
 * @file main.c
 * @brief Gate_orangepi 主程序
 * @author PiCtrl Team
 * @date 2026-06-09
 *
 * @note 编译时间: __DATE__ __TIME__
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "hal_led.h"
#include "device_service.h"
#include "log_service.h"
#include "config_service.h"
#include "cmd_parser.h"
#include "event_manager.h"
#include "tcp_server.h"
#include "client_manager.h"
#include "message_queue.h"

/* ============================================================
 * 编译时间定义
 * ============================================================ */
#define COMPILE_TIME __DATE__ " " __TIME__
#define COMPILE_TIMESTAMP __DATE__ " " __TIME__

/* ============================================================
 * 全局变量
 * ============================================================ */

static volatile int g_running = 1;          /**< 运行标志 */
static pthread_t g_network_thread;          /**< 网络线程 */
static pthread_t g_business_thread;         /**< 业务线程 */
static pthread_t g_log_thread;              /**< 日志线程 */

/* ============================================================
 * 守护进程相关
 * ============================================================ */

/**
 * @brief 初始化守护进程
 * @return 0: 成功, -1: 失败
 *
 * @note 标准守护进程初始化流程：
 *       1. fork 子进程，父进程退出
 *       2. 创建新会话 (setsid)
 *       3. 再次 fork 防止获取终端
 *       4. 设置文件权限掩码 (umask)
 *       5. 切换到根目录 (chdir)
 *       6. 关闭标准输入输出
 */
static int daemon_init(void)
{
    pid_t pid;
    int i;

    // 1. fork 子进程
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid > 0) {
        // 父进程退出
        exit(0);
    }

    // 2. 创建新会话
    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }

    // 3. 再次 fork 防止获取终端
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid > 0) {
        exit(0);
    }

    // 4. 设置文件权限掩码
    umask(0);

    // 5. 切换到根目录
    if (chdir("/") < 0) {
        perror("chdir");
        return -1;
    }

    // 6. 关闭标准输入输出
    for (i = 0; i < 3; i++) {
        close(i);
        open("/dev/null", O_RDWR);
    }

    return 0;
}

/* ============================================================
 * 信号处理
 * ============================================================ */

/**
 * @brief 信号处理函数
 * @param sig 信号编号
 *
 * @note 支持的信号：
 *       SIGINT/SIGTERM - 优雅退出
 *       SIGUSR1 - 重新加载配置
 *       SIGUSR2 - 切换日志等级
 */
static void signal_handler(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            // 优雅退出 - 只设置标志，不在信号处理函数中写日志（避免可能的递归调用）
            g_running = 0;
            break;

        case SIGUSR1:
            // 重新加载配置
            log_info("收到 SIGUSR1，重新加载配置");
            config_load("/etc/gate_orangepi.conf");
            break;

        case SIGUSR2:
            // 切换日志等级
            log_info("收到 SIGUSR2，切换日志等级");
            // 这里可以添加日志等级切换逻辑
            break;

        default:
            break;
    }
}

/**
 * @brief 注册信号处理
 */
static void signal_setup(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    // 忽略 SIGPIPE，防止 socket 写错误导致进程退出
    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 * 线程函数
 * ============================================================ */

/**
 * @brief 网络线程
 * @param arg 参数
 * @return NULL
 *
 * @note 负责 TCP 服务器运行，接收客户端连接和数据
 *       使用 epoll 实现高并发
 */
static void* network_thread_func(void *arg)
{
    log_info("网络线程启动");

    // 读取配置的端口
    int server_port = atoi(config_get("server_port", "8888"));

    // 启动 TCP 服务器（使用配置文件中的端口）
    if (tcp_server_start(server_port) < 0) {
        log_error("TCP 服务器启动失败");
        return NULL;
    }

    // 等待服务器结束
    while (g_running && tcp_server_is_running()) {
        sleep(1);
    }

    log_info("网络线程退出");
    return NULL;
}

/**
 * @brief 业务线程
 * @param arg 参数
 * @return NULL
 *
 * @note 负责从消息队列获取消息，解析命令，执行事件处理
 *       实现业务逻辑与网络层的解耦
 */
static void* business_thread_func(void *arg)
{
    message_t msg;
    char response[256];
    cmd_t cmd;

    log_info("业务线程启动");

    while (g_running) {
        // 从消息队列获取消息
        if (mq_pop(&msg) < 0) {
            // 队列为空，等待
            usleep(10000); // 10ms
            continue;
        }

        // 解析命令
        cmd = parse_cmd(msg.data);

        // 处理事件
        event_process(cmd, response, sizeof(response));

        // 发送响应给客户端
        if (msg.client_fd >= 0) {
            write(msg.client_fd, response, strlen(response));
        }
    }

    log_info("业务线程退出");
    return NULL;
}

/**
 * @brief 日志线程
 * @param arg 参数
 * @return NULL
 *
 * @note 负责异步日志写入
 *       目前日志是同步写入，此线程作为缓冲刷新线程
 */
static void* log_thread_func(void *arg)
{
    log_info("日志线程启动");

    while (g_running) {
        // 这里可以实现异步日志写入
        // 目前日志是同步写入的，这个线程作为日志缓冲刷新线程
        sleep(5);
    }

    log_info("日志线程退出");
    return NULL;
}

/* ============================================================
 * 系统状态打印
 * ============================================================ */

/**
 * @brief 打印系统状态
 */
static void print_system_status(void)
{
    device_status_t status;
    int clientcount;
    time_t now;
    char time_str[64];

    // 获取当前时间
    now = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 获取设备状态
    device_status_get_all(&status);
    clientcount = client_count();

    // 打印状态
    log_info("系统状态: [%s] LED=%s, 客户端=%d, 温度=%.1f°C",
             time_str,
             status.led_status ? "ON" : "OFF",
             clientcount,
             status.temperature / 10.0);
}

/* ============================================================
 * 初始化与清理
 * ============================================================ */

/**
 * @brief 初始化系统
 * @return 0: 成功, -1: 失败
 *
 * @note 初始化顺序：
 *       1. HAL LED
 *       2. 日志系统（记录编译时间）
 *       3. 配置加载
 *       4. 设备状态
 *       5. 消息队列
 *       6. 客户端管理
 */
static int system_init(void)
{
    // 1. 初始化 HAL
    if (hal_led_init(0) < 0) {
        fprintf(stderr, "HAL LED 初始化失败\n");
        return -1;
    }

    // 2. 加载配置（使用当前目录）
    config_load("./gate_orangepi.conf");
    log_info("配置加载完成");

    // 3. 初始化日志（使用配置文件中的日志路径）
    const char *log_file = config_get("log_file", "./gate_orangepi.log");
    log_init(log_file);
    log_info("Gate_orangepi 服务启动");
    log_info("编译时间: %s", COMPILE_TIME);

    // 4. 初始化设备状态
    device_status_init();
    log_info("设备状态初始化完成");

    // 5. 初始化消息队列
    if (mq_init() < 0) {
        log_error("消息队列初始化失败");
        return -1;
    }
    log_info("消息队列初始化完成");

    // 6. 初始化客户端管理器
    if (client_manager_init() < 0) {
        log_error("客户端管理器初始化失败");
        return -1;
    }
    log_info("客户端管理器初始化完成");

    return 0;
}

/**
 * @brief 清理系统资源
 *
 * @note 清理顺序与初始化相反
 */
static void system_cleanup(void)
{
    log_info("开始清理系统资源...");

    // 停止 TCP 服务器
    tcp_server_stop();
    log_info("TCP 服务器已停止");

    // 销毁客户端管理器
    client_manager_destroy();
    log_info("客户端管理器已销毁");

    // 销毁消息队列
    mq_destroy();
    log_info("消息队列已销毁");

    // 关闭 LED
    hal_led_off(0);
    log_info("LED 已关闭");

    log_info("Gate_orangepi 服务停止");
}

/* ============================================================
 * 主函数
 * ============================================================ */

/**
 * @brief 主函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 0: 成功, 1: 失败
 *
 * @note 程序入口，负责解析命令行参数、初始化系统、创建线程、主循环
 */
int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    int opt;

    // 解析命令行参数
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "用法: %s [-d]\n", argv[0]);
                fprintf(stderr, "  -d  以守护进程模式运行\n");
                return 1;
        }
    }

    // 守护进程模式
    if (daemon_mode) {
        if (daemon_init() < 0) {
            fprintf(stderr, "守护进程初始化失败\n");
            return 1;
        }
    }

    // 设置信号处理
    signal_setup();

    // 初始化系统
    if (system_init() < 0) {
        fprintf(stderr, "系统初始化失败\n");
        return 1;
    }

    // 创建网络线程
    if (pthread_create(&g_network_thread, NULL, network_thread_func, NULL) != 0) {
        log_error("网络线程创建失败");
        return 1;
    }
    log_info("网络线程已创建");

    // 创建业务线程
    if (pthread_create(&g_business_thread, NULL, business_thread_func, NULL) != 0) {
        log_error("业务线程创建失败");
        return 1;
    }
    log_info("业务线程已创建");

    // 创建日志线程
    if (pthread_create(&g_log_thread, NULL, log_thread_func, NULL) != 0) {
        log_error("日志线程创建失败");
        return 1;
    }
    log_info("日志线程已创建");

    // 读取配置参数
    int server_port = atoi(config_get("server_port", "8888"));
    int status_interval = atoi(config_get("status_interval", "10"));

    // 获取本机IP地址（遍历网络接口）
    char ip_address[256] = "127.0.0.1";
    FILE *fp = popen("/sbin/ifconfig | grep -Eo 'inet (addr:)?([0-9]*\\.){3}[0-9]*' | grep -Eo '([0-9]*\\.){3}[0-9]*' | grep -v '127.0.0.1' | head -n 1", "r");
    if (fp != NULL) {
        if (fgets(ip_address, sizeof(ip_address), fp) != NULL) {
            // 去除换行符
            ip_address[strcspn(ip_address, "\n")] = '\0';
        }
        pclose(fp);
    }

    log_info("Gate_orangepi 服务已启动");
    log_info("服务器地址: %s:%d", ip_address, server_port);
    log_info("编译时间: %s", COMPILE_TIME);

    // 打印客户端使用说明
    log_info("========================================");
    log_info("客户端命令说明:");
    log_info("  LED ON          - 打开LED灯");
    log_info("  LED OFF         - 关闭LED灯");
    log_info("  GET STATUS      - 获取系统状态");
    log_info("  GET TEMP        - 获取温度");
    log_info("  GET CLIENT      - 获取客户端数量");
    log_info("  RELOAD CONFIG   - 重新加载配置");
    log_info("========================================");

    // 主循环
    while (g_running) {
        // 打印系统状态
        print_system_status();

        // 使用配置的间隔时间
        sleep(status_interval);
    }

    log_info("收到退出信号，开始关闭...");

    // 停止消息队列（唤醒业务线程）
    log_info("停止消息队列...");
    mq_stop();

    // 等待线程结束
    log_info("等待网络线程退出...");
    pthread_join(g_network_thread, NULL);
    log_info("网络线程已退出");

    log_info("等待业务线程退出...");
    pthread_join(g_business_thread, NULL);
    log_info("业务线程已退出");

    log_info("等待日志线程退出...");
    pthread_join(g_log_thread, NULL);
    log_info("日志线程已退出");

    log_info("所有线程已退出");

    // 清理系统资源
    system_cleanup();

    return 0;
}

