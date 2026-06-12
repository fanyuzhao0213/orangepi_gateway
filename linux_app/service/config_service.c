#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config_service.h"
#include "hal_file.h"

static config_item_t *g_config_head = NULL;
static pthread_mutex_t g_config_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 查找配置项
 * @param key 键名
 * @return 配置项指针，未找到返回 NULL
 */
static config_item_t* find_config_item(const char *key)
{
    config_item_t *item = g_config_head;
    while (item != NULL) {
        if (strcmp(item->key, key) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

/**
 * @brief 解析配置行
 * @param line 配置行字符串
 * @param key 输出键名
 * @param value 输出值
 * @return 0: 成功, -1: 失败
 */
static int parse_config_line(const char *line, char *key, char *value)
{
    char *eq_pos = strchr(line, '=');
    if (eq_pos == NULL) {
        return -1;
    }

    // 提取键名
    int key_len = eq_pos - line;
    if (key_len >= 64) {
        return -1;
    }
    memcpy(key, line, key_len);
    key[key_len] = '\0';

    // 提取值
    const char *val_start = eq_pos + 1;
    int val_len = strlen(val_start);
    if (val_len >= 256) {
        return -1;
    }
    strcpy(value, val_start);

    return 0;
}

/**
 * @brief 设置默认配置
 */
static void set_default_config(void)
{
    // 设置默认配置项
    config_set("server_port", "8888");
    config_set("max_clients", "20");
    config_set("log_level", "INFO");
    config_set("log_file", "./gate_orangepi.log");
    config_set("status_interval", "10");
}

/**
 * @brief 加载配置文件
 * @param conf_file 配置文件路径
 */
void config_load(const char *conf_file)
{
    char buf[4096];
    int read_len;
    char *line;
    char *next_line;
    char key[64];
    char value[256];

    // 清空当前配置
    config_clear();

    // 读取文件
    read_len = file_read(conf_file, buf, sizeof(buf) - 1);
    if (read_len < 0) {
        // 文件不存在，设置默认配置并创建文件
        set_default_config();
        config_save(conf_file);
        return;
    }
    buf[read_len] = '\0';

    // 解析每一行
    line = buf;
    while (*line != '\0') {
        // 查找行结束
        next_line = strchr(line, '\n');
        if (next_line != NULL) {
            *next_line = '\0';
        }

        // 跳过空行和注释
        if (line[0] != '\0' && line[0] != '#') {
            // 去除行尾回车
            char *end = line + strlen(line) - 1;
            while (end >= line && (*end == '\r' || *end == '\n' || *end == ' ')) {
                *end = '\0';
                end--;
            }

            // 解析 key=value
            if (parse_config_line(line, key, value) == 0) {
                config_set(key, value);
            }
        }

        if (next_line == NULL) {
            break;
        }
        line = next_line + 1;
    }
}

/**
 * @brief 保存配置文件
 * @param conf_file 配置文件路径
 */
void config_save(const char *conf_file)
{
    char buf[8192] = {0};
    int offset = 0;
    config_item_t *item;

    pthread_mutex_lock(&g_config_mutex);

    // 生成配置内容
    item = g_config_head;
    while (item != NULL) {
        offset += snprintf(buf + offset, sizeof(buf) - offset,
                          "%s=%s\n", item->key, item->value);
        item = item->next;
    }

    pthread_mutex_unlock(&g_config_mutex);

    // 写入文件
    if (offset > 0) {
        file_write(conf_file, buf, offset);
    }
}

/**
 * @brief 获取配置项值
 * @param key 配置项键名
 * @param default_value 默认值（如果键不存在）
 * @return 配置项值，如果不存在返回 default_value
 */
const char* config_get(const char *key, const char *default_value)
{
    config_item_t *item;
    const char *result = default_value;

    pthread_mutex_lock(&g_config_mutex);

    item = find_config_item(key);
    if (item != NULL) {
        result = item->value;
    }

    pthread_mutex_unlock(&g_config_mutex);

    return result;
}

/**
 * @brief 设置配置项值
 * @param key 配置项键名
 * @param value 配置项值
 */
void config_set(const char *key, const char *value)
{
    config_item_t *item;

    pthread_mutex_lock(&g_config_mutex);

    item = find_config_item(key);
    if (item != NULL) {
        // 更新现有项
        strncpy(item->value, value, sizeof(item->value) - 1);
        item->value[sizeof(item->value) - 1] = '\0';
    } else {
        // 创建新项
        item = (config_item_t *)malloc(sizeof(config_item_t));
        if (item != NULL) {
            strncpy(item->key, key, sizeof(item->key) - 1);
            item->key[sizeof(item->key) - 1] = '\0';
            strncpy(item->value, value, sizeof(item->value) - 1);
            item->value[sizeof(item->value) - 1] = '\0';
            item->next = g_config_head;
            g_config_head = item;
        }
    }

    pthread_mutex_unlock(&g_config_mutex);
}

/**
 * @brief 清除所有配置
 */
void config_clear(void)
{
    config_item_t *item;
    config_item_t *next;

    pthread_mutex_lock(&g_config_mutex);

    item = g_config_head;
    while (item != NULL) {
        next = item->next;
        free(item);
        item = next;
    }
    g_config_head = NULL;

    pthread_mutex_unlock(&g_config_mutex);
}


