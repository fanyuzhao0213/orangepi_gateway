#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cmd_parser.h"

/* ===== 工具函数 ===== */

static char* trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void to_upper_str(char *str)
{
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

/* ===== 命令解析 ===== */

cmd_t parse_cmd(const char *buf)
{
    char cmd_buf[128];

    if (buf == NULL) return CMD_UNKNOWN;

    size_t len = strlen(buf);
    if (len >= sizeof(cmd_buf)) return CMD_UNKNOWN;

    strncpy(cmd_buf, buf, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    trim(cmd_buf);
    to_upper_str(cmd_buf);

    /* LED 控制 */
    if (strcmp(cmd_buf, "LED ON") == 0) return CMD_LED_ON;
    else if (strcmp(cmd_buf, "LED OFF") == 0) return CMD_LED_OFF;
    else if (strcmp(cmd_buf, "LED TOGGLE") == 0) return CMD_LED_TOGGLE;

    /* LED 触发模式（无参数） */
    else if (strcmp(cmd_buf, "LED TRIGGER NONE") == 0) return CMD_LED_TRIGGER_NONE;
    else if (strcmp(cmd_buf, "LED TRIGGER HEARTBEAT") == 0) return CMD_LED_TRIGGER_HEARTBEAT;
    else if (strcmp(cmd_buf, "LED TRIGGER TIMER") == 0) return CMD_LED_TRIGGER_TIMER;

    /* LED 触发模式（带参数） */
    else if (strncmp(cmd_buf, "LED TRIGGER HEARTBEAT ", 21) == 0) return CMD_LED_TRIGGER_HEARTBEAT;
    else if (strncmp(cmd_buf, "LED TRIGGER TIMER ", 18) == 0) return CMD_LED_TRIGGER_TIMER;

    /* 状态查询 */
    else if (strcmp(cmd_buf, "GET STATUS") == 0) return CMD_GET_STATUS;
    else if (strcmp(cmd_buf, "GET TEMP") == 0) return CMD_GET_TEMP;
    else if (strcmp(cmd_buf, "GET CLIENT") == 0) return CMD_GET_CLIENT;

    /* 配置 */
    else if (strcmp(cmd_buf, "RELOAD CONFIG") == 0) return CMD_RELOAD_CONFIG;

    /* OLED */
    else if (strcmp(cmd_buf, "OLED HELP") == 0) return CMD_OLED_HELP;
    else if (strcmp(cmd_buf, "OLED MAIN") == 0) return CMD_OLED_MAIN;

    /* 按键统计 */
    else if (strcmp(cmd_buf, "KEY STATS") == 0) return CMD_KEY_STATS;

    return CMD_UNKNOWN;
}

/* ===== 触发模式参数解析 ===== */

led_trigger_mode_t parse_trigger_mode(const char *buf, int *interval_ms)
{
    char cmd_buf[128];

    if (interval_ms) *interval_ms = 500;
    if (buf == NULL) return LED_TRIGGER_NONE;

    strncpy(cmd_buf, buf, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    trim(cmd_buf);
    to_upper_str(cmd_buf);

    if (strcmp(cmd_buf, "LED TRIGGER NONE") == 0) {
        return LED_TRIGGER_NONE;
    } else if (strcmp(cmd_buf, "LED TRIGGER HEARTBEAT") == 0) {
        return LED_TRIGGER_HEARTBEAT;
    } else if (strncmp(cmd_buf, "LED TRIGGER HEARTBEAT ", 21) == 0) {
        return LED_TRIGGER_HEARTBEAT;
    } else if (strcmp(cmd_buf, "LED TRIGGER TIMER") == 0) {
        return LED_TRIGGER_TIMER;
    } else if (strncmp(cmd_buf, "LED TRIGGER TIMER ", 18) == 0) {
        if (interval_ms) {
            *interval_ms = atoi(cmd_buf + 18);
            if (*interval_ms <= 0 || *interval_ms > 10000) *interval_ms = 500;
        }
        return LED_TRIGGER_TIMER;
    }

    return LED_TRIGGER_NONE;
}