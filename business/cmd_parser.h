#ifndef CMD_PARSER_H
#define CMD_PARSER_H

/**
 * @brief 命令枚举
 */
typedef enum {
    CMD_LED_ON,          // LED ON
    CMD_LED_OFF,         // LED OFF
    CMD_GET_STATUS,      // GET STATUS
    CMD_GET_TEMP,        // GET TEMP
    CMD_GET_CLIENT,      // GET CLIENT
    CMD_RELOAD_CONFIG,   // RELOAD CONFIG
    CMD_OLED_HELP,       // OLED HELP
    CMD_OLED_MAIN,       // OLED MAIN
    CMD_UNKNOWN          // 未知命令
} cmd_t;

/**
 * @brief 解析命令字符串
 * @param buf 输入命令字符串
 * @return 对应的命令枚举值，未识别返回 CMD_UNKNOWN
 */
cmd_t parse_cmd(const char *buf);

#endif /* CMD_PARSER_H */


