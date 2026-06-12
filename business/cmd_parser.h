#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include "../hal/hal_led.h"

/**
 * @brief 命令枚举
 */
typedef enum {
    CMD_LED_ON,               /* LED ON - 点亮LED */
    CMD_LED_OFF,             /* LED OFF - 熄灭LED */
    CMD_LED_TOGGLE,          /* LED TOGGLE - 切换LED状态 */
    CMD_LED_TRIGGER_NONE,    /* LED TRIGGER NONE - 手动模式 */
    CMD_LED_TRIGGER_HEARTBEAT, /* LED TRIGGER HEARTBEAT - 心跳模式 */
    CMD_LED_TRIGGER_TIMER,   /* LED TRIGGER TIMER [ms] - 定时器模式 */
    CMD_GET_STATUS,          /* GET STATUS - 获取状态 */
    CMD_GET_TEMP,            /* GET TEMP - 获取温度 */
    CMD_GET_CLIENT,          /* GET CLIENT - 获取客户端数量 */
    CMD_RELOAD_CONFIG,       /* RELOAD CONFIG - 重新加载配置 */
    CMD_OLED_HELP,           /* OLED HELP - 显示帮助 */
    CMD_OLED_MAIN,           /* OLED MAIN - 显示主界面 */
    CMD_KEY_STATS,           /* KEY STATS - 获取按键统计 */
    CMD_UNKNOWN              /* 未知命令 */
} cmd_t;

/**
 * @brief 解析命令字符串
 * @param buf 输入命令字符串
 * @return 对应的命令枚举值，未识别返回 CMD_UNKNOWN
 */
cmd_t parse_cmd(const char *buf);

/**
 * @brief 解析触发模式命令的参数
 * @param buf 输入命令字符串
 * @param interval_ms 输出: 间隔毫秒（仅 TIMER 模式）
 * @return 触发模式
 */
led_trigger_mode_t parse_trigger_mode(const char *buf, int *interval_ms);

#endif /* CMD_PARSER_H */