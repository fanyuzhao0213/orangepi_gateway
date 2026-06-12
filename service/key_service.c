/*
 * key_service.c - 按键服务
 *
 * 功能：
 *   1. 封装 hal_key 层，提供高级按键服务
 *   2. 处理按键事件，映射到系统动作
 *   3. 与 event_manager 集成，处理业务逻辑
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <linux/input.h>
#include "key_service.h"
#include "../hal/hal_key.h"
#include "../hal/hal_led.h"
#include "../hal/oled_display.h"
#include "device_service.h"
#include "log_service.h"

/* ===== 终端颜色 ===== */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ===== 常量定义 ===== */
#define LONG_PRESS_THRESHOLD_MS  2000   /* 长按阈值: 2秒 */

/* ===== 时间戳 helper ===== */
static void print_ts(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf(C_BLUE "[%s] " C_RESET, buf);
}

/* ===== 全局变量 ===== */
static int g_key_service_running = 0;
static pthread_mutex_t g_key_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 按键动作统计 */
static struct {
    int total_presses;
    int total_releases;
    int total_long_presses;
    time_t last_press_time;
} g_key_stats = {0, 0, 0, 0};

/* ===== 内部函数（前向声明）===== */
static void handle_key_long_press(key_info_t *key);

/* ===== 内部函数实现 ===== */

static void handle_key_press(key_info_t *key)
{
    pthread_mutex_lock(&g_key_mutex);
    g_key_stats.total_presses++;
    g_key_stats.last_press_time = time(NULL);
    pthread_mutex_unlock(&g_key_mutex);

    print_ts();
    printf(C_GREEN "▼ 按下  " C_RESET C_BOLD "%-16s" C_RESET
           " code=%-4d  第 %d 次\n",
           key->name, key->code, key->press_count);

    log_info("[KEY] 按键按下: %s (code=%d)",
             key->name ? key->name : "UNKNOWN", key->code);

    /* KEY_2 (code=3) 按下时打印命令使用说明 */
    if (key->code == 3) {
        print_ts();
        printf(C_CYAN "📋 命令使用说明:\n" C_RESET);
        printf(C_CYAN "  LED ON/OFF/TOGGLE - LED控制\n" C_RESET);
        printf(C_CYAN "  LED TRIGGER NONE/HEARTBEAT/TIMER [ms] - 触发模式\n" C_RESET);
        printf(C_CYAN "  GET STATUS/TEMP/CLIENT - 状态查询\n" C_RESET);
        printf(C_CYAN "  OLED HELP/MAIN - OLED显示\n" C_RESET);
        printf(C_CYAN "  KEY STATS - 按键统计\n" C_RESET);
        printf(C_CYAN "  Web: http://%s:%s\n" C_RESET,
               "192.168.1.125", "8080");
    }
}

static void handle_key_long_press(key_info_t *key)
{
    pthread_mutex_lock(&g_key_mutex);
    g_key_stats.total_long_presses++;
    pthread_mutex_unlock(&g_key_mutex);

    print_ts();
    printf(C_BLUE "🔥 长按  " C_RESET C_BOLD "%-16s" C_RESET
           " 时长 %ld ms\n",
           key->name, key->hold_ms);

    log_info("[KEY] 长按操作: %s, 时长 %ld ms",
             key->name ? key->name : "UNKNOWN", key->hold_ms);

    /* 长按操作：根据键值执行不同任务 */
    switch (key->code) {
        case KEY_F1:
            print_ts();
            printf(C_CYAN "⚡ 操作  " C_RESET "F1 → 切换 LED 状态\n");
            hal_led_toggle(0);
            device_status_set_led(hal_led_get_brightness());
            oled_display_message("LED",
                hal_led_get_brightness() ? C_GREEN "● 已点亮" C_RESET : C_YELLOW "○ 已熄灭" C_RESET,
                1);
            break;

        case KEY_F2:
            print_ts();
            printf(C_CYAN "⚡ 操作  " C_RESET "F2 → 显示帮助界面\n");
            oled_display_help();
            break;

        case KEY_LEFT:
            print_ts();
            printf(C_CYAN "⚡ 操作  " C_RESET "LEFT → 上一个页面\n");
            break;
        case KEY_RIGHT:
            print_ts();
            printf(C_CYAN "⚡ 操作  " C_RESET "RIGHT → 下一个页面\n");
            break;

        default:
            print_ts();
            printf(C_YELLOW "⚠ 未定义  code=%d\n" C_RESET, key->code);
            break;
    }
}

static void handle_key_release(key_info_t *key)
{
    pthread_mutex_lock(&g_key_mutex);
    g_key_stats.total_releases++;
    pthread_mutex_unlock(&g_key_mutex);

    print_ts();
    printf(C_YELLOW "▲ 抬起  " C_RESET C_BOLD "%-16s" C_RESET
           " code=%-4d  按住 %ld ms\n",
           key->name, key->code, key->hold_ms);

    log_info("[KEY] 按键抬起: %s (code=%d), 按住 %ld ms",
             key->name ? key->name : "UNKNOWN", key->code, key->hold_ms);

    if (key->hold_ms >= LONG_PRESS_THRESHOLD_MS) {
        handle_key_long_press(key);
    }
}

/**
 * @brief 按键回调函数（被 hal_key 调用）
 */
static void key_callback(key_info_t *key)
{
    if (!key) return;

    switch (key->type) {
        case KEY_EVENT_PRESS:
            handle_key_press(key);
            break;

        case KEY_EVENT_RELEASE:
            handle_key_release(key);
            break;

        case KEY_EVENT_REPEAT:
            break;

        default:
            break;
    }
}

/* ===== 公共 API 实现 ===== */

int key_service_init(void)
{
    if (hal_key_init() < 0) {
        return -1;
    }

    hal_key_register_callback(key_callback);
    memset(&g_key_stats, 0, sizeof(g_key_stats));
    g_key_service_running = 1;
    return 0;
}

void key_service_close(void)
{
    if (!g_key_service_running) {
        return;
    }

    g_key_service_running = 0;
    hal_key_close();
}

int key_service_is_running(void)
{
    return g_key_service_running && hal_key_is_inited();
}

void key_service_get_stats(int *presses, int *releases, int *long_presses)
{
    if (presses) *presses = g_key_stats.total_presses;
    if (releases) *releases = g_key_stats.total_releases;
    if (long_presses) *long_presses = g_key_stats.total_long_presses;
}
