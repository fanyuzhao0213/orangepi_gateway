#ifndef HAL_KEY_H
#define HAL_KEY_H

#include <stdint.h>
#include <time.h>

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_PRESS   = 1,  /* 按下 */
    KEY_EVENT_RELEASE  = 0,  /* 抬起 */
    KEY_EVENT_REPEAT   = 2  /* 长按重复 */
} key_event_type_t;

/* 按键信息结构 */
typedef struct {
    int           code;       /* 键值 */
    const char   *name;       /* 键名 */
    key_event_type_t type;    /* 事件类型 */
    long          hold_ms;    /* 按住时长(ms) */
    int           press_count; /* 按下次数 */
    struct timespec press_time; /* 按下时间戳 */
    int           is_pressed;  /* 当前是否按下 */
} key_info_t;

/* 按键回调函数类型 */
typedef void (*key_callback_t)(key_info_t *key);

/* ===== 公共 API ===== */

/**
 * @brief 初始化按键驱动
 * @return 0: 成功, -1: 失败
 */
int hal_key_init(void);

/**
 * @brief 关闭按键驱动
 */
void hal_key_close(void);

/**
 * @brief 注册按键回调函数
 * @param callback: 按键事件回调函数
 */
void hal_key_register_callback(key_callback_t callback);

/**
 * @brief 检查按键驱动是否已初始化
 * @return 1: 已初始化, 0: 未初始化
 */
int hal_key_is_inited(void);

/**
 * @brief 获取设备路径
 * @return 设备路径字符串
 */
const char* hal_key_get_device_path(void);

/**
 * @brief 获取支持的按键数量
 * @return 按键数量
 */
int hal_key_get_supported_count(void);

#endif /* HAL_KEY_H */