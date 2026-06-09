#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cmd_parser.h"

/**
 * @brief 去除字符串首尾空白字符
 * @param str 输入字符串
 * @return 处理后的字符串指针
 */
static char* trim(char *str)
{
    char *end;

    // 去除前导空白
    while (isspace((unsigned char)*str)) {
        str++;
    }

    // 空字符串
    if (*str == 0) {
        return str;
    }

    // 去除尾部空白
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';

    return str;
}

/**
 * @brief 将字符串转换为大写
 * @param str 输入字符串
 */
static void to_upper(char *str)
{
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

/**
 * @brief 解析命令字符串
 * @param buf 输入命令字符串
 * @return 对应的命令枚举值，未识别返回 CMD_UNKNOWN
 */
cmd_t parse_cmd(const char *buf)
{
    char cmd_buf[64];
    size_t len;

    if (buf == NULL) {
        return CMD_UNKNOWN;
    }

    // 复制并处理输入
    len = strlen(buf);
    if (len >= sizeof(cmd_buf)) {
        return CMD_UNKNOWN;
    }
    strncpy(cmd_buf, buf, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    // 去除首尾空白
    trim(cmd_buf);

    // 转换为大写
    to_upper(cmd_buf);

    // 匹配命令
    if (strcmp(cmd_buf, "LED ON") == 0) {
        return CMD_LED_ON;
    } else if (strcmp(cmd_buf, "LED OFF") == 0) {
        return CMD_LED_OFF;
    } else if (strcmp(cmd_buf, "GET STATUS") == 0) {
        return CMD_GET_STATUS;
    } else if (strcmp(cmd_buf, "GET TEMP") == 0) {
        return CMD_GET_TEMP;
    } else if (strcmp(cmd_buf, "GET CLIENT") == 0) {
        return CMD_GET_CLIENT;
    } else if (strcmp(cmd_buf, "RELOAD CONFIG") == 0) {
        return CMD_RELOAD_CONFIG;
    }

    return CMD_UNKNOWN;
}


