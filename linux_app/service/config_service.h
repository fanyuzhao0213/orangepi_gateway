#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

/**
 * @brief 配置项结构
 */
typedef struct config_item {
    char key[64];
    char value[256];
    struct config_item *next;
} config_item_t;

/**
 * @brief 加载配置文件
 * @param conf_file 配置文件路径
 */
void config_load(const char *conf_file);

/**
 * @brief 保存配置文件
 * @param conf_file 配置文件路径
 */
void config_save(const char *conf_file);

/**
 * @brief 获取配置项值
 * @param key 配置项键名
 * @param default_value 默认值（如果键不存在）
 * @return 配置项值，如果不存在返回 default_value
 */
const char* config_get(const char *key, const char *default_value);

/**
 * @brief 设置配置项值
 * @param key 配置项键名
 * @param value 配置项值
 */
void config_set(const char *key, const char *value);

/**
 * @brief 清除所有配置
 */
void config_clear(void);

#endif


