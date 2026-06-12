#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "hal_led.h"

/* ===== 终端颜色 ===== */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ===== 时间戳 helper ===== */
static void print_ts(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf(C_BLUE "[%s] " C_RESET, buf);
}

/* ===== LED 路径配置 ===== */
static const char *LED_PATHS[] = {
    "/sys/devices/platform/fyz_led",
    "/sys/class/leds/green_led",
    NULL
};

static char g_led_path[256] = {0};

/* ===== 文件操作辅助函数 ===== */

static int find_led_path(void)
{
    for (int i = 0; LED_PATHS[i] != NULL; i++) {
        if (access(LED_PATHS[i], F_OK) == 0) {
            strncpy(g_led_path, LED_PATHS[i], sizeof(g_led_path) - 1);
            g_led_path[sizeof(g_led_path) - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static int write_to_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        if (errno != ENOENT) {
            print_ts();
            fprintf(stderr, C_RED "✗ 打开文件失败: %s - %s\n" C_RESET, path, strerror(errno));
        }
        return -1;
    }

    ssize_t n = write(fd, value, strlen(value));
    close(fd);

    if (n < 0) {
        print_ts();
        fprintf(stderr, C_RED "✗ 写入文件失败: %s - %s\n" C_RESET, path, strerror(errno));
        return -1;
    }
    return 0;
}

static int read_from_file(const char *path, char *buffer, int size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    int n = read(fd, buffer, size - 1);
    close(fd);

    if (n > 0) {
        buffer[n] = '\0';
        if (n > 1 && buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }
    }
    return n;
}

static void make_path(char *buf, int len, const char *filename)
{
    snprintf(buf, len, "%s/%s", g_led_path, filename);
}

/* ===== 公共 API 实现 ===== */

int hal_led_init(int gpio_pin)
{
    (void)gpio_pin;

    printf(C_BOLD "\n╔═══════════════════════════════════════════╗\n" C_RESET);
    printf(C_BOLD "║           LED 初始化                      ║\n" C_RESET);
    printf(C_BOLD "╚═══════════════════════════════════════════╝\n" C_RESET);

    if (find_led_path() < 0) {
        fprintf(stderr, C_RED "\n┌─────────────────────────────────────────┐\n" C_RESET);
        fprintf(stderr, C_RED "│ ✗ LED 初始化失败                         │\n" C_RESET);
        fprintf(stderr, C_RED "├─────────────────────────────────────────┤\n" C_RESET);
        fprintf(stderr, C_RED "│ 未找到 LED 设备节点，请确认：            │\n" C_RESET);
        fprintf(stderr, C_RED "│   1. LED 驱动已加载                      │\n" C_RESET);
        fprintf(stderr, C_RED "│   2. 检查 /sys/class/leds/              │\n" C_RESET);
        fprintf(stderr, C_RED "└─────────────────────────────────────────┘\n" C_RESET);
        return -1;
    }

    printf(C_BOLD C_GREEN "✓ LED 初始化成功\n" C_RESET);
    printf(C_BOLD "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" C_RESET);
    printf(C_CYAN "  设备路径: " C_RESET "%s\n", g_led_path);
    printf(C_BOLD "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" C_RESET);

    hal_led_set_trigger(LED_TRIGGER_NONE);
    return hal_led_off(0);
}

int hal_led_on(int gpio_pin)
{
    (void)gpio_pin;

    if (g_led_path[0] == '\0') {
        return -1;
    }

    char path[512];

    make_path(path, sizeof(path), "trigger");
    if (write_to_file(path, "none") < 0) {
        return -1;
    }

    make_path(path, sizeof(path), "brightness");
    if (write_to_file(path, "255") < 0) {
        return -1;
    }

    print_ts();
    printf(C_BOLD C_GREEN "✓ LED 点亮\n" C_RESET);
    return 0;
}

int hal_led_off(int gpio_pin)
{
    (void)gpio_pin;

    if (g_led_path[0] == '\0') {
        return -1;
    }

    char path[512];

    make_path(path, sizeof(path), "trigger");
    if (write_to_file(path, "none") < 0) {
        return -1;
    }

    make_path(path, sizeof(path), "brightness");
    if (write_to_file(path, "0") < 0) {
        return -1;
    }

    print_ts();
    printf(C_BOLD C_YELLOW "✓ LED 熄灭\n" C_RESET);
    return 0;
}

int hal_led_toggle(int gpio_pin)
{
    int br = hal_led_get_brightness();
    if (br < 0) {
        return -1;
    }

    print_ts();
    if (br > 0) {
        printf(C_BOLD C_YELLOW "⚡ LED 切换: " C_RESET "● → ○\n");
        return hal_led_off(gpio_pin);
    } else {
        printf(C_BOLD C_GREEN "⚡ LED 切换: " C_RESET "○ → ●\n");
        return hal_led_on(gpio_pin);
    }
}

int hal_led_set_trigger(led_trigger_mode_t mode)
{
    if (g_led_path[0] == '\0') {
        return -1;
    }

    const char *mode_str;

    switch (mode) {
        case LED_TRIGGER_HEARTBEAT:
            mode_str = "heartbeat";
            break;
        case LED_TRIGGER_TIMER:
            mode_str = "timer";
            break;
        default:
            mode_str = "none";
            break;
    }

    char path[512];
    make_path(path, sizeof(path), "trigger");
    return write_to_file(path, mode_str);
}

int hal_led_set_trigger_with_delay(led_trigger_mode_t mode, int delay_on_ms, int delay_off_ms)
{
    if (g_led_path[0] == '\0') {
        return -1;
    }

    const char *mode_str;
    const char *icon;

    /* 限制范围: 10ms ~ 10000ms */
    if (delay_on_ms < 10)   delay_on_ms = 10;
    if (delay_on_ms > 10000) delay_on_ms = 10000;
    if (delay_off_ms < 10)  delay_off_ms = 10;
    if (delay_off_ms > 10000) delay_off_ms = 10000;

    switch (mode) {
        case LED_TRIGGER_HEARTBEAT:
            mode_str = "heartbeat";
            icon = C_RED "♥" C_RESET;
            print_ts();
            printf(C_BOLD C_CYAN "♥ LED 触发模式: " C_RESET "%s heartbeat\n", icon);
            break;
        case LED_TRIGGER_TIMER:
            mode_str = "timer";
            icon = C_CYAN "⏱" C_RESET;
            print_ts();
            printf(C_BOLD C_CYAN "⏱ LED 触发模式: " C_RESET "%s timer (on=%dms off=%dms)\n",
                   icon, delay_on_ms, delay_off_ms);
            break;
        case LED_TRIGGER_NONE:
        default:
            mode_str = "none";
            icon = C_YELLOW "●" C_RESET;
            print_ts();
            printf(C_BOLD C_CYAN "● LED 触发模式: " C_RESET "%s manual\n", icon);
            break;
    }

    char path[512];

    /* 设置 trigger */
    make_path(path, sizeof(path), "trigger");
    if (write_to_file(path, mode_str) < 0) {
        return -1;
    }

    /* TIMER 模式需要设置 delay_on/delay_off（如果文件存在） */
    if (mode == LED_TRIGGER_TIMER) {
        char val[32];

        make_path(path, sizeof(path), "delay_on");
        if (access(path, F_OK) == 0) {
            snprintf(val, sizeof(val), "%d", delay_on_ms);
            write_to_file(path, val);
        }

        make_path(path, sizeof(path), "delay_off");
        if (access(path, F_OK) == 0) {
            snprintf(val, sizeof(val), "%d", delay_off_ms);
            write_to_file(path, val);
        }
    }

    return 0;
}

int hal_led_get_trigger(led_trigger_mode_t *mode)
{
    if (g_led_path[0] == '\0' || mode == NULL) {
        return -1;
    }

    char path[512];
    char buf[64];

    make_path(path, sizeof(path), "trigger");
    if (read_from_file(path, buf, sizeof(buf)) < 0) {
        return -1;
    }

    if (strstr(buf, "heartbeat") != NULL) {
        *mode = LED_TRIGGER_HEARTBEAT;
    } else if (strstr(buf, "timer") != NULL) {
        *mode = LED_TRIGGER_TIMER;
    } else {
        *mode = LED_TRIGGER_NONE;
    }

    return 0;
}

int hal_led_get_brightness(void)
{
    if (g_led_path[0] == '\0') {
        return -1;
    }

    char path[512];
    char buf[16];

    make_path(path, sizeof(path), "brightness");
    if (read_from_file(path, buf, sizeof(buf)) < 0) {
        return -1;
    }

    return atoi(buf) > 0 ? 1 : 0;
}