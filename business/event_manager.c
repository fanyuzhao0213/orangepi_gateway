#include <stdio.h>
#include <string.h>
#include <time.h>
#include "event_manager.h"
#include "../hal/hal_led.h"
#include "../hal/oled_display.h"
#include "../service/device_service.h"
#include "../service/config_service.h"
#include "../service/log_service.h"
#include "../service/key_service.h"

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

/* ===== 内部函数 ===== */

static int handle_led_on(char *response, int response_size)
{
    device_status_set_led(1);
    hal_led_on(0);

    print_ts();
    printf(C_BOLD C_GREEN "✓ 命令  " C_RESET "LED ON → LED已点亮\n");

    log_info("CMD: LED ON - LED已打开");

    snprintf(response, response_size, "OK: LED turned ON\r\n");
    return 0;
}

static int handle_led_off(char *response, int response_size)
{
    device_status_set_led(0);
    hal_led_off(0);

    print_ts();
    printf(C_BOLD C_YELLOW "✓ 命令  " C_RESET "LED OFF → LED已熄灭\n");

    log_info("CMD: LED OFF - LED已关闭");

    snprintf(response, response_size, "OK: LED turned OFF\r\n");
    return 0;
}

static int handle_led_toggle(char *response, int response_size)
{
    int current = device_status_get_led();
    int new_state = (current > 0) ? 0 : 1;

    device_status_set_led(new_state);
    hal_led_toggle(0);

    print_ts();
    printf(C_BOLD C_CYAN "✓ 命令  " C_RESET "LED TOGGLE → LED切换到 %s\n",
           new_state ? C_GREEN "● ON" C_RESET : C_YELLOW "○ OFF" C_RESET);

    log_info("CMD: LED TOGGLE - LED切换到 %s", new_state ? "ON" : "OFF");

    snprintf(response, response_size, "OK: LED turned %s\r\n", new_state ? "ON" : "OFF");
    return 0;
}

/**
 * @brief 处理 LED TRIGGER 命令（支持带参数）
 */
static int handle_led_trigger(cmd_t cmd, const char *raw_cmd, char *response, int response_size)
{
    led_trigger_mode_t mode;
    const char *mode_name;
    int interval_ms = 0;

    mode = parse_trigger_mode(raw_cmd, &interval_ms);

    switch (mode) {
        case LED_TRIGGER_HEARTBEAT:
            mode_name = "heartbeat";
            break;
        case LED_TRIGGER_TIMER:
            mode_name = "timer";
            break;
        default:
            mode_name = "none";
            break;
    }

    device_status_set_led_trigger(mode);

    /* TIMER 模式使用带延迟参数设置 */
    if (mode == LED_TRIGGER_TIMER) {
        hal_led_set_trigger_with_delay(mode, interval_ms, interval_ms);
    } else {
        hal_led_set_trigger(mode);
    }

    print_ts();
    if (mode == LED_TRIGGER_TIMER && interval_ms > 0) {
        printf(C_BOLD C_CYAN "✓ 命令  " C_RESET "LED TRIGGER %s (on=%dms off=%dms)\n",
               mode_name, interval_ms, interval_ms);
        snprintf(response, response_size, "OK: LED trigger set to %s (on=%dms off=%dms)\r\n",
                 mode_name, interval_ms, interval_ms);
    } else {
        printf(C_BOLD C_CYAN "✓ 命令  " C_RESET "LED TRIGGER %s\n", mode_name);
        snprintf(response, response_size, "OK: LED trigger set to %s\r\n", mode_name);
    }

    log_info("CMD: LED TRIGGER %s - 触发模式已设置", mode_name);

    return 0;
}

static int handle_get_status(char *response, int response_size)
{
    device_status_t st;
    device_status_get_all(&st);

    const char *trigger_str;
    const char *led_icon;
    switch (st.led_trigger) {
        case LED_TRIGGER_HEARTBEAT:
            trigger_str = "heartbeat";
            led_icon = C_RED "♥" C_RESET;
            break;
        case LED_TRIGGER_TIMER:
            trigger_str = "timer";
            led_icon = C_CYAN "⏱" C_RESET;
            break;
        default:
            trigger_str = "none";
            led_icon = st.led_status ? C_GREEN "●" C_RESET : C_YELLOW "○" C_RESET;
            break;
    }

    print_ts();
    printf(C_BOLD "📊 系统状态:\n" C_RESET);
    printf(C_BOLD "   ├─ LED:    " C_RESET "%s (%s)\n", led_icon,
           st.led_status ? C_GREEN "ON" C_RESET : C_YELLOW "OFF" C_RESET);
    printf(C_BOLD "   ├─ 触发:   " C_RESET "%s\n", trigger_str);
    printf(C_BOLD "   ├─ 客户端: " C_RESET "%d\n", st.client_count);
    printf(C_BOLD "   └─ 温度:   " C_RESET "%.1f°C\n", st.temperature / 10.0);

    log_info("CMD: GET STATUS - LED=%d(%s), CLIENT=%d, TEMP=%d",
             st.led_status, trigger_str, st.client_count, st.temperature);

    snprintf(response, response_size,
             "LED=%d CLIENT=%d TEMP=%d TRIGGER=%s\r\n",
             st.led_status, st.client_count, st.temperature, trigger_str);
    return 0;
}

static int handle_get_temp(char *response, int response_size)
{
    int temp = device_status_get_temperature();

    print_ts();
    printf(C_BOLD "🌡 命令  " C_RESET "GET TEMP → 温度: %.1f°C\n", temp / 10.0);

    log_info("CMD: GET TEMP - TEMP=%d", temp);

    snprintf(response, response_size, "TEMP=%d\r\n", temp);
    return 0;
}

static int handle_get_client(char *response, int response_size)
{
    int clients = device_status_get_client_count();

    print_ts();
    printf(C_BOLD "👥 命令  " C_RESET "GET CLIENT → 客户端数量: %d\n", clients);

    log_info("CMD: GET CLIENT - CLIENT=%d", clients);

    snprintf(response, response_size, "CLIENT=%d\r\n", clients);
    return 0;
}

static int handle_reload_config(char *response, int response_size)
{
    config_load("/home/orangepi/fyz_test/pi_ctrl.conf");

    print_ts();
    printf(C_BOLD C_GREEN "✓ 命令  " C_RESET "RELOAD CONFIG → 配置已重新加载\n");

    log_info("CMD: RELOAD CONFIG - 配置已重新加载");

    snprintf(response, response_size, "CONFIG RELOADED\r\n");
    return 0;
}

static int handle_oled_help(char *response, int response_size)
{
    oled_display_help();

    print_ts();
    printf(C_BOLD C_CYAN "✓ 命令  " C_RESET "OLED HELP → 显示命令帮助界面\n");

    log_info("CMD: OLED HELP - 显示命令帮助");

    snprintf(response, response_size, "OK: Showing help\r\n");
    return 0;
}

static int handle_oled_main(char *response, int response_size)
{
    oled_display_message("提示", "返回主界面", 1);

    print_ts();
    printf(C_BOLD C_CYAN "✓ 命令  " C_RESET "OLED MAIN → 返回主界面\n");

    log_info("CMD: OLED MAIN - 返回主界面");

    snprintf(response, response_size, "OK: Back to main\r\n");
    return 0;
}

static int handle_key_stats(char *response, int response_size)
{
    int presses = 0, releases = 0, long_presses = 0;
    key_service_get_stats(&presses, &releases, &long_presses);

    print_ts();
    printf(C_BOLD "🔘 按键统计:\n" C_RESET);
    printf(C_BOLD "   ├─ 按下次数:     " C_RESET "%d\n", presses);
    printf(C_BOLD "   ├─ 抬起次数:     " C_RESET "%d\n", releases);
    printf(C_BOLD "   └─ 长按次数:     " C_RESET "%d\n", long_presses);

    log_info("CMD: KEY STATS - presses=%d, releases=%d, long_presses=%d",
             presses, releases, long_presses);

    snprintf(response, response_size,
             "KEY STATS: presses=%d, releases=%d, long_presses=%d\r\n",
             presses, releases, long_presses);
    return 0;
}

static int handle_unknown(char *response, int response_size)
{
    print_ts();
    printf(C_BOLD C_RED "✗ 命令  " C_RESET "UNKNOWN → 未知命令\n");

    log_warn("CMD: UNKNOWN - 收到未知命令");

    snprintf(response, response_size, "ERROR: Unknown command\r\n");
    return 0;
}

/* ===== 公共 API（前向声明）===== */
int event_process(cmd_t cmd, char *response, int response_size);
int event_process_raw(const char *raw_cmd, char *response, int response_size);

int event_process(cmd_t cmd, char *response, int response_size)
{
    if (response == NULL || response_size <= 0) {
        return -1;
    }

    snprintf(response, response_size, "ERROR\r\n");

    switch (cmd) {
        case CMD_LED_ON:
            return handle_led_on(response, response_size);
        case CMD_LED_OFF:
            return handle_led_off(response, response_size);
        case CMD_LED_TOGGLE:
            return handle_led_toggle(response, response_size);
        case CMD_LED_TRIGGER_NONE:
            return event_process_raw("LED TRIGGER NONE", response, response_size);
        case CMD_LED_TRIGGER_HEARTBEAT:
            return event_process_raw("LED TRIGGER HEARTBEAT", response, response_size);
        case CMD_LED_TRIGGER_TIMER:
            return event_process_raw("LED TRIGGER TIMER", response, response_size);
        case CMD_GET_STATUS:
            return handle_get_status(response, response_size);
        case CMD_GET_TEMP:
            return handle_get_temp(response, response_size);
        case CMD_GET_CLIENT:
            return handle_get_client(response, response_size);
        case CMD_RELOAD_CONFIG:
            return handle_reload_config(response, response_size);
        case CMD_OLED_HELP:
            return handle_oled_help(response, response_size);
        case CMD_OLED_MAIN:
            return handle_oled_main(response, response_size);
        case CMD_KEY_STATS:
            return handle_key_stats(response, response_size);
        case CMD_UNKNOWN:
        default:
            return handle_unknown(response, response_size);
    }
}

/**
 * @brief 处理带原始命令字符串的事件（用于解析参数）
 */
int event_process_raw(const char *raw_cmd, char *response, int response_size)
{
    cmd_t cmd = parse_cmd(raw_cmd);

    if (cmd == CMD_LED_TRIGGER_NONE ||
        cmd == CMD_LED_TRIGGER_HEARTBEAT ||
        cmd == CMD_LED_TRIGGER_TIMER) {
        return handle_led_trigger(cmd, raw_cmd, response, response_size);
    }

    return event_process(cmd, response, response_size);
}