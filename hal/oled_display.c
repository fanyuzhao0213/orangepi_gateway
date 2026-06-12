#include "oled_display.h"
#include "hal_oled.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

static int g_blink = 0;
static pthread_t g_welcome_thread;
static int g_welcome_running = 0;

/**
 * @brief 欢迎界面线程函数
 */
static void* welcome_thread_func(void *arg)
{
    const char *version = (const char *)arg;
    int i;

    g_welcome_running = 1;

    hal_oled_clear();

    // 显示标题（居中）
    hal_oled_show_string(10, 0, "Gate_orangepi", OLED_8X16);
    // 显示版本信息（居中）
    hal_oled_show_string(10, 16, version, OLED_8X16);
    // 显示加载提示（居中）
    hal_oled_show_string(10, 40, "系统启动中...", OLED_8X16);
    hal_oled_update();

    // 进度条动画
    for (i = 0; i <= 128 && g_welcome_running; i += 8) {
        hal_oled_draw_line(i, 56, i + 4, 56);
        hal_oled_draw_line(i, 57, i + 4, 57);
        hal_oled_update();
        usleep(30000); // 加快动画速度，减少阻塞
    }

    g_welcome_running = 0;
    return NULL;
}

/**
 * @brief 初始化 OLED 显示模块
 */
int oled_display_init(int i2c_bus, uint8_t i2c_addr)
{
    return hal_oled_init(i2c_bus, i2c_addr);
}

/**
 * @brief 异步显示开机欢迎界面
 */
int oled_display_welcome_async(const char *version)
{
    if (g_welcome_running) {
        // 欢迎界面正在显示，取消之前的
        g_welcome_running = 0;
        pthread_join(g_welcome_thread, NULL);
    }

    if (pthread_create(&g_welcome_thread, NULL, welcome_thread_func, (void *)version) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief 等待欢迎界面显示完成（可选）
 */
void oled_display_wait_welcome_done(void)
{
    if (g_welcome_running) {
        pthread_join(g_welcome_thread, NULL);
    }
}

/**
 * @brief 检查欢迎界面是否正在显示
 */
int oled_display_is_welcome_running(void)
{
    return g_welcome_running;
}

/**
 * @brief 显示系统主界面（简洁布局）
 */
void oled_display_main(const char *ip_address, int port,
                       int client_count, int temperature, int led_status)
{
    char temp_str[20];
    char client_str[20];
    char port_str[20];
    char time_str[20];
    time_t now;
    struct tm *local;

    hal_oled_clear();

    // ===== 顶栏：标题 + 时间 =====
    now = time(NULL);
    local = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local);

    hal_oled_show_string(0, 0, "Gate_orangepi", OLED_6X8);
    hal_oled_show_string(80, 0, time_str, OLED_6X8);

    // 分隔线（可选）
    // hal_oled_draw_line(0, 10, 127, 10, OLED_WHITE);

    // ===== 网络信息 =====
    hal_oled_show_string(0, 14, "IP:", OLED_6X8);
    hal_oled_show_string(20, 14, ip_address, OLED_6X8);

    sprintf(port_str, "Port: %d", port);
    hal_oled_show_string(0, 30, port_str, OLED_6X8);

    // ===== 状态信息 =====
    sprintf(client_str, "Clients: %d", client_count);
    hal_oled_show_string(0, 44, client_str, OLED_6X8);

    sprintf(temp_str, "Temp: %.1fC", temperature / 10.0);
    hal_oled_show_string(0, 58, temp_str, OLED_6X8);

    // ===== LED 状态 =====
    hal_oled_show_string(70, 44, "LED:", OLED_6X8);
    if (led_status) {
        hal_oled_show_string(100, 44, "ON", OLED_6X8);
        hal_oled_draw_circle(122, 43, 3, OLED_FILLED);  // 填充圆
    } else {
        hal_oled_show_string(100, 44, "OFF", OLED_6X8);
        hal_oled_draw_circle(122, 43, 3, OLED_UNFILLED); // 空心圆
    }

    // ===== 活动指示器（右下角） =====
    g_blink = !g_blink;
    if (g_blink) {
        hal_oled_draw_point(126, 62);
    }

    hal_oled_update();
}

/**
 * @brief 显示命令帮助界面
 */
void oled_display_help(void)
{
    static int page = 0;

    hal_oled_clear();

    // 标题栏
    hal_oled_draw_rectangle(0, 0, 128, 16, OLED_FILLED);
    hal_oled_reverse_area(0, 0, 128, 16);
    hal_oled_show_string(32, 0, "命令帮助", OLED_8X16);

    // 分隔线
    hal_oled_draw_line(0, 16, 127, 16);

    if (page == 0) {
        // 第一页命令（控制宽度：英文6px，中文16px）
        // LED ON - 打开LED = 3*6 + 3*6 + 2*16 = 18+18+32 = 68px ✓
        // LED OFF - 关闭LED = 3*6 + 4*6 + 2*16 = 18+24+32 = 74px ✓
        // GET STATUS - 获取状态 = 3*6 + 6*6 + 4*16 = 18+36+64 = 118px ✓
        // GET TEMP - 获取温度 = 3*6 + 4*6 + 4*16 = 18+24+64 = 106px ✓
        hal_oled_show_string(2, 20, "LED ON - 打开LED", OLED_6X8);
        hal_oled_show_string(2, 28, "LED OFF - 关闭LED", OLED_6X8);
        hal_oled_show_string(2, 36, "GET STATUS - 获取状态", OLED_6X8);
        hal_oled_show_string(2, 44, "GET TEMP - 获取温度", OLED_6X8);
    } else {
        // 第二页命令
        // GET CLIENT - 获取客户端 = 3*6 + 6*6 + 5*16 = 18+36+80 = 134px ✗
        // 改为：GET CLIENT - 获取客端 = 3*6 + 6*6 + 4*16 = 18+36+64 = 118px ✓
        // RELOAD - 重载配置 = 6*6 + 4*16 = 36+64 = 100px ✓
        // OLED HELP - 显示帮助 = 4*6 + 4*16 = 24+64 = 88px ✓
        // OLED MAIN - 返回主界面 = 4*6 + 5*16 = 24+80 = 104px ✓
        hal_oled_show_string(2, 20, "GET CLIENT - 获取客端", OLED_6X8);
        hal_oled_show_string(2, 28, "RELOAD - 重载配置", OLED_6X8);
        hal_oled_show_string(2, 36, "OLED HELP - 显示帮助", OLED_6X8);
        hal_oled_show_string(2, 44, "OLED MAIN - 返回主界面", OLED_6X8);
    }

    // 底部页码
    static const char *page_texts[] = {"页1/2", "页2/2"};
    hal_oled_show_string(100, 0, page_texts[page], OLED_6X8);

    hal_oled_update();

    // 翻页（下次调用自动切换）
    page = (page + 1) % 2;
}

/**
 * @brief 显示消息提示界面
 */
void oled_display_message(const char *title, const char *message, int timeout)
{
    hal_oled_clear();

    // 标题栏
    hal_oled_draw_rectangle(0, 0, 128, 16, OLED_FILLED);
    hal_oled_reverse_area(0, 0, 128, 16);
    hal_oled_show_string(2, 0, title, OLED_8X16);

    // 分隔线
    hal_oled_draw_line(0, 16, 127, 16);

    // 消息内容（支持多行）
    char line1[32] = {0};
    char line2[32] = {0};

    if (strlen(message) > 16) {
        strncpy(line1, message, 16);
        strncpy(line2, message + 16, 16);
    } else {
        strcpy(line1, message);
    }

    hal_oled_show_string(2, 24, line1, OLED_8X16);
    if (strlen(line2) > 0) {
        hal_oled_show_string(2, 40, line2, OLED_8X16);
    }

    hal_oled_update();

    if (timeout > 0) {
        sleep(timeout);
    }
}

/**
 * @brief 显示接收到的命令
 */
void oled_display_command(const char *cmd)
{
    hal_oled_clear();

    // 标题栏
    hal_oled_draw_rectangle(0, 0, 128, 16, OLED_FILLED);
    hal_oled_reverse_area(0, 0, 128, 16);
    hal_oled_show_string(2, 0, "收到命令", OLED_8X16);

    // 分隔线
    hal_oled_draw_line(0, 16, 127, 16);

    // 显示命令内容
    hal_oled_show_string(2, 24, cmd, OLED_8X16);

    // 显示时间
    char time_str[20];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local);
    hal_oled_show_string(2, 44, time_str, OLED_6X8);

    hal_oled_update();
    sleep(2);
}

/**
 * @brief 更新显示缓存到屏幕
 */
void oled_display_update(void)
{
    hal_oled_update();
}

/**
 * @brief 清屏
 */
void oled_display_clear(void)
{
    hal_oled_clear();
    hal_oled_update();
}

/**
 * @brief 关闭 OLED 显示
 */
void oled_display_deinit(void)
{
    hal_oled_deinit();
}
