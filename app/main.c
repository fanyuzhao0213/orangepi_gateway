/**
 * @file main.c
 * @brief Gate_orangepi 主程序
 * @author PiCtrl Team
 * @date 2026-06-12
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
#include "hal_oled.h"
#include "oled_display.h"
#include "web_service.h"
#include "key_service.h"

/* ============================================================
 * 编译时间定义
 * ============================================================ */
#define COMPILE_TIME __DATE__ " " __TIME__
#define COMPILE_TIMESTAMP __DATE__ " " __TIME__

/* ============================================================
 * 终端颜色定义
 * ============================================================ */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ============================================================
 * 全局变量
 * ============================================================ */

static volatile int g_running = 1;          /**< 运行标志 */
static pthread_t g_network_thread;          /**< 网络线程 */
static pthread_t g_business_thread;         /**< 业务线程 */
static pthread_t g_log_thread;              /**< 日志线程 */
static pthread_t g_web_thread;              /**< Web 服务线程 */
static pthread_t g_key_thread;             /**< 按键服务线程 */

/* ============================================================
 * 守护进程相关
 * ============================================================ */

/**
 * @brief 初始化守护进程
 */
static int daemon_init(void)
{
    pid_t pid;
    int i;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid > 0) {
        exit(0);
    }

    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid > 0) {
        exit(0);
    }

    umask(0);

    if (chdir("/") < 0) {
        perror("chdir");
        return -1;
    }

    for (i = 0; i < 3; i++) {
        close(i);
        open("/dev/null", O_RDWR);
    }

    return 0;
}

/* ============================================================
 * 信号处理
 * ============================================================ */

static void signal_handler(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            g_running = 0;
            break;

        case SIGUSR1:
            log_info("收到 SIGUSR1，重新加载配置");
            config_load("./gate_orangepi.conf");
            break;

        case SIGUSR2:
            log_info("收到 SIGUSR2，切换日志等级");
            break;

        default:
            break;
    }
}

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

    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 * 线程函数
 * ============================================================ */

static void* network_thread_func(void *arg)
{
    (void)arg;
    log_info("网络线程启动");

    int server_port = atoi(config_get("server_port", "8888"));

    if (tcp_server_start(server_port) < 0) {
        log_error("TCP 服务器启动失败");
        return NULL;
    }

    while (g_running && tcp_server_is_running()) {
        sleep(1);
    }

    log_info("网络线程退出");
    return NULL;
}

static void* business_thread_func(void *arg)
{
    (void)arg;
    message_t msg;
    char response[256];

    log_info("业务线程启动");

    while (g_running) {
        if (mq_pop(&msg) < 0) {
            usleep(10000);
            continue;
        }

        /* 支持带参数命令，如 LED TRIGGER TIMER 500 */
        event_process_raw(msg.data, response, sizeof(response));

        if (msg.client_fd >= 0) {
            write(msg.client_fd, response, strlen(response));
        }
    }

    log_info("业务线程退出");
    return NULL;
}

static void* log_thread_func(void *arg)
{
    (void)arg;
    log_info("日志线程启动");

    while (g_running) {
        sleep(5);
    }

    log_info("日志线程退出");
    return NULL;
}

static void* web_thread_func(void *arg)
{
    (void)arg;
    log_info("Web 服务线程启动");

    int web_port = atoi(config_get("web_port", "8080"));

    if (web_service_start(web_port) < 0) {
        log_error("Web 服务启动失败");
        return NULL;
    }

    log_info("Web 服务线程退出");
    return NULL;
}

/**
 * @brief 按键服务线程
 */
static void* key_thread_func(void *arg)
{
    (void)arg;
    log_info("按键服务线程启动");

    /* key_service_init() 已经在 system_init() 中调用
     * 这里只需要等待服务运行即可
     */

    while (g_running && key_service_is_running()) {
        sleep(1);
    }

    log_info("按键服务线程退出");
    return NULL;
}

/* ============================================================
 * 系统状态打印
 * ============================================================ */

static void print_system_status(void)
{
    device_status_t status;
    int clientcount;
    time_t now;
    char time_str[64];

    now = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    device_status_get_all(&status);
    clientcount = client_count();

    const char *trigger_str;
    switch (status.led_trigger) {
        case LED_TRIGGER_HEARTBEAT:
            trigger_str = "heartbeat";
            break;
        case LED_TRIGGER_TIMER:
            trigger_str = "timer";
            break;
        default:
            trigger_str = "none";
            break;
    }

    log_info("系统状态: [%s] LED=%s(%s), 客户端=%d, 温度=%.1f°C",
             time_str,
             status.led_status ? "ON" : "OFF",
             trigger_str,
             clientcount,
             status.temperature / 10.0);
}

/* ============================================================
 * 初始化与清理
 * ============================================================ */

static int system_init(void)
{
    printf(C_BOLD "\n╔══════════════════════════════════════════════╗\n" C_RESET);
    printf(C_BOLD "║       Gate_orangepi 系统初始化              ║\n" C_RESET);
    printf(C_BOLD "╚══════════════════════════════════════════════╝\n" C_RESET);

    /* 1. 初始化 HAL LED */
    if (hal_led_init(0) < 0) {
        fprintf(stderr, C_RED "[错误] HAL LED 初始化失败\n" C_RESET);
        return -1;
    }
    printf(C_GREEN "✓ HAL LED 初始化完成\n" C_RESET);

    /* 2. 初始化日志 */
    const char *log_file = config_get("log_file", "./gate_orangepi.log");
    log_init(log_file);
    log_info("==========================================");
    log_info("Gate_orangepi 服务启动");
    log_info("编译时间: %s", COMPILE_TIME);
    log_info("==========================================");

    /* 3. 加载配置 */
    config_load("./gate_orangepi.conf");
    log_info("配置加载完成");

    /* 4. 初始化设备状态 */
    device_status_init();
    log_info("设备状态初始化完成");

    /* 5. 初始化消息队列 */
    if (mq_init() < 0) {
        log_error("消息队列初始化失败");
        return -1;
    }
    printf(C_GREEN "✓ 消息队列初始化完成\n" C_RESET);

    /* 6. 初始化客户端管理器 */
    if (client_manager_init() < 0) {
        log_error("客户端管理器初始化失败");
        return -1;
    }
    printf(C_GREEN "✓ 客户端管理器初始化完成\n" C_RESET);

    /* 7. 初始化 OLED 显示模块 */
    if (oled_display_init(2, 0x3C) < 0) {
        log_error("OLED 初始化失败");
        return -1;
    }
    printf(C_GREEN "✓ OLED 初始化完成\n" C_RESET);

    /* 8. 异步显示开机欢迎界面 */
    if (oled_display_welcome_async(OLED_DISPLAY_VERSION) < 0) {
        log_warn("OLED 欢迎界面异步显示启动失败");
    }
    printf(C_CYAN "  OLED 欢迎界面正在显示...\n" C_RESET);

    /* 9. 初始化按键服务 */
    if (key_service_init() < 0) {
        log_warn("按键服务初始化失败（驱动可能未加载）");
        printf(C_YELLOW "⚠ 按键服务初始化失败，请检查驱动\n" C_RESET);
    } else {
        printf(C_GREEN "✓ 按键服务初始化完成\n" C_RESET);
    }

    printf(C_BOLD "\n╔══════════════════════════════════════════════╗\n" C_RESET);
    printf(C_BOLD "║       系统初始化完成                        ║\n" C_RESET);
    printf(C_BOLD "╚══════════════════════════════════════════════╝\n\n" C_RESET);

    return 0;
}

static void system_cleanup(void)
{
    log_info("开始清理系统资源...");

    /* 停止按键服务 */
    if (key_service_is_running()) {
        key_service_close();
        log_info("按键服务已关闭");
    }

    /* 停止 Web 服务 */
    web_service_stop();
    log_info("Web 服务已停止");

    /* 停止 TCP 服务器 */
    tcp_server_stop();
    log_info("TCP 服务器已停止");

    /* 销毁客户端管理器 */
    client_manager_destroy();
    log_info("客户端管理器已销毁");

    /* 销毁消息队列 */
    mq_destroy();
    log_info("消息队列已销毁");

    /* 关闭 LED */
    hal_led_off(0);
    log_info("LED 已关闭");

    log_info("Gate_orangepi 服务停止");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    int opt;

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

    if (daemon_mode) {
        int ppid = getppid();
        const char *invocation_id = getenv("INVOCATION_ID");

        if (ppid != 1 && invocation_id == NULL) {
            if (daemon_init() < 0) {
                fprintf(stderr, "守护进程初始化失败\n");
                return 1;
            }
        } else {
            fprintf(stderr, "检测到服务管理器启动 (ppid=%d, invocation_id=%s),跳过 double-fork\n",
                    ppid, invocation_id ? "yes" : "no");
        }
    }

    signal_setup();

    /* 切换到可执行文件所在目录 */
    {
        char exe_dir[512] = {0};
        ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (len > 0) {
            char *slash = strrchr(exe_dir, '/');
            if (slash != NULL) {
                *slash = '\0';
                if (chdir(exe_dir) == 0) {
                    fprintf(stderr, "工作目录已切换到: %s\n", exe_dir);
                }
            }
        }
    }

    /* 初始化系统 */
    if (system_init() < 0) {
        fprintf(stderr, "系统初始化失败\n");
        return 1;
    }

    /* 创建网络线程 */
    if (pthread_create(&g_network_thread, NULL, network_thread_func, NULL) != 0) {
        log_error("网络线程创建失败");
        return 1;
    }
    log_info("网络线程已创建");

    /* 创建业务线程 */
    if (pthread_create(&g_business_thread, NULL, business_thread_func, NULL) != 0) {
        log_error("业务线程创建失败");
        return 1;
    }
    log_info("业务线程已创建");

    /* 创建日志线程 */
    if (pthread_create(&g_log_thread, NULL, log_thread_func, NULL) != 0) {
        log_error("日志线程创建失败");
        return 1;
    }
    log_info("日志线程已创建");

    /* 创建 Web 服务线程 */
    if (pthread_create(&g_web_thread, NULL, web_thread_func, NULL) != 0) {
        log_error("Web 服务线程创建失败");
        return 1;
    }
    log_info("Web 服务线程已创建");

    /* 创建按键服务线程（按键服务在 system_init 中已初始化） */
    if (key_service_is_running()) {
        if (pthread_create(&g_key_thread, NULL, key_thread_func, NULL) != 0) {
            log_error("按键服务线程创建失败");
        } else {
            log_info("按键服务线程已创建");
        }
    } else {
        /* 按键服务未运行，线程 ID 置 0 */
        g_key_thread = 0;
    }

    /* 读取配置参数 */
    int server_port = atoi(config_get("server_port", "8888"));
    int web_port = atoi(config_get("web_port", "8080"));
    int status_interval = atoi(config_get("status_interval", "10"));

    /* 获取本机 IP 地址 */
    char ip_address[256] = "127.0.0.1";
    FILE *fp = popen("/sbin/ifconfig | grep -Eo 'inet (addr:)?([0-9]*\\.){3}[0-9]*' | grep -Eo '([0-9]*\\.){3}[0-9]*' | grep -v '127.0.0.1' | head -n 1", "r");
    if (fp != NULL) {
        if (fgets(ip_address, sizeof(ip_address), fp) != NULL) {
            ip_address[strcspn(ip_address, "\n")] = '\0';
        }
        pclose(fp);
    }

    printf(C_BOLD "\n╔══════════════════════════════════════════════╗\n" C_RESET);
    printf(C_BOLD "║       Gate_orangepi 服务已启动               ║\n" C_RESET);
    printf(C_BOLD "╠══════════════════════════════════════════════╣\n" C_RESET);
    printf(C_BOLD "║  TCP 服务器:  " C_RESET "%s:%d\n", ip_address, server_port);
    printf(C_BOLD "║  Web 管理界面:" C_RESET " http://%s:%d\n", ip_address, web_port);
    printf(C_BOLD "║  编译时间:    " C_RESET "%s\n", COMPILE_TIME);
    printf(C_BOLD "╚══════════════════════════════════════════════╝\n" C_RESET);

    log_info("========================================");
    log_info("TCP 客户端命令说明:");
    log_info("  LED ON              - 打开LED灯");
    log_info("  LED OFF             - 关闭LED灯");
    log_info("  LED TOGGLE          - 切换LED状态");
    log_info("  LED TRIGGER NONE    - 手动模式");
    log_info("  LED TRIGGER HEARTBEAT - 心跳模式");
    log_info("  LED TRIGGER TIMER   - 定时器模式");
    log_info("  GET STATUS          - 获取系统状态");
    log_info("  GET TEMP            - 获取温度");
    log_info("  GET CLIENT          - 获取客户端数量");
    log_info("  KEY STATS           - 获取按键统计");
    log_info("  RELOAD CONFIG       - 重新加载配置");
    log_info("Web 管理界面: http://%s:%d", ip_address, web_port);
    log_info("========================================");

    /* 主循环 */
    while (g_running) {
        print_system_status();

        device_status_t status;
        device_status_get_all(&status);

        if (!oled_display_is_welcome_running()) {
            oled_display_main(ip_address, server_port,
                             status.client_count,
                             status.temperature,
                             status.led_status);
        }

        sleep(status_interval);
    }

    log_info("收到退出信号，开始关闭...");

    mq_stop();
    web_service_stop();

    log_info("等待网络线程退出...");
    pthread_join(g_network_thread, NULL);

    log_info("等待业务线程退出...");
    pthread_join(g_business_thread, NULL);

    log_info("等待日志线程退出...");
    pthread_join(g_log_thread, NULL);

    log_info("等待 Web 服务线程退出...");
    pthread_join(g_web_thread, NULL);

    log_info("所有线程已退出");

    system_cleanup();

    return 0;
}