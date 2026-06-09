#ifndef LOG_SERVICE_H
#define LOG_SERVICE_H

/**
 * @brief 日志级别
 */
typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

/**
 * @brief 初始化日志系统
 * @param log_file 日志文件路径
 */
void log_init(const char *log_file);

/**
 * @brief 写日志
 * @param level 日志级别
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_write(log_level_t level, const char *fmt, ...);

/**
 * @brief 写日志（简化版，自动添加 INFO 级别）
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_info(const char *fmt, ...);

/**
 * @brief 写警告日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_warn(const char *fmt, ...);

/**
 * @brief 写错误日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_error(const char *fmt, ...);

#endif

