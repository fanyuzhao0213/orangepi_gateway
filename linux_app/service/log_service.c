#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log_service.h"
#include "../hal/hal_file.h"

static char g_log_file[256] = "/var/log/pi_ctrl.log";
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 获取当前时间字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
static void get_time_str(char *buf, size_t size)
{
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief 获取日志级别字符串
 * @param level 日志级别
 * @return 级别字符串
 */
static const char* get_level_str(log_level_t level)
{
    switch (level) {
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

/**
 * @brief 初始化日志系统
 * @param log_file 日志文件路径
 */
void log_init(const char *log_file)
{
    if (log_file != NULL) {
        strncpy(g_log_file, log_file, sizeof(g_log_file) - 1);
        g_log_file[sizeof(g_log_file) - 1] = '\0';
    }

    // 创建日志文件目录
    char *last_slash = strrchr(g_log_file, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        file_mkdir(g_log_file);  // 创建目录（支持递归创建）
        *last_slash = '/';
    }

    // 写入初始日志
    log_info("日志系统初始化完成");
}

/**
 * @brief 写日志
 * @param level 日志级别
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_write(log_level_t level, const char *fmt, ...)
{
    char time_buf[32];
    char msg_buf[1024];
    char log_buf[1100];
    va_list args;

    // 获取当前时间
    get_time_str(time_buf, sizeof(time_buf));

    // 格式化消息
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // 组装日志行
    snprintf(log_buf, sizeof(log_buf),
             "[%s][%s] %s\n",
             time_buf,
             get_level_str(level),
             msg_buf);

    // 多线程安全写入
    pthread_mutex_lock(&g_log_mutex);

    // 写入文件
    file_write(g_log_file, log_buf, strlen(log_buf));

    // 同时输出到控制台（调试用）
    printf("%s", log_buf);

    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief 写日志（简化版，自动添加 INFO 级别）
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_info(const char *fmt, ...)
{
    char msg_buf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    log_write(LOG_LEVEL_INFO, "%s", msg_buf);
}

/**
 * @brief 写警告日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_warn(const char *fmt, ...)
{
    char msg_buf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    log_write(LOG_LEVEL_WARN, "%s", msg_buf);
}

/**
 * @brief 写错误日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_error(const char *fmt, ...)
{
    char msg_buf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    log_write(LOG_LEVEL_ERROR, "%s", msg_buf);
}


