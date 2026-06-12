#ifndef KEY_SERVICE_H
#define KEY_SERVICE_H

#include <stdint.h>
#include <hal/hal_key.h>

/* 按键动作定义 */
typedef enum {
    KEY_ACTION_NONE     = 0,
    KEY_ACTION_PRESS,       /* 按下 */
    KEY_ACTION_RELEASE,     /* 抬起 */
    KEY_ACTION_LONG_PRESS    /* 长按 (>2秒) */
} key_action_t;

/* 按键配置 */
typedef struct {
    int           key_code;      /* 键值 */
    const char   *key_name;       /* 按键名称 */
    key_action_t  action;        /* 当前动作 */
    long          hold_duration;  /* 按住时长(ms) */
} key_config_t;

/* ===== 按键服务 API ===== */

/**
 * @brief 初始化按键服务
 * @return 0: 成功, -1: 失败
 */
int key_service_init(void);

/**
 * @brief 关闭按键服务
 */
void key_service_close(void);

/**
 * @brief 检查按键服务是否运行
 * @return 1: 运行中, 0: 未运行
 */
int key_service_is_running(void);

/**
 * @brief 获取按键事件回调
 * @param key: 按键信息
 */
void key_service_on_key_event(key_info_t *key);

/**
 * @brief 获取按键统计信息
 * @param presses: 输出按下次数
 * @param releases: 输出抬起次数
 * @param long_presses: 输出长按次数
 */
void key_service_get_stats(int *presses, int *releases, int *long_presses);

#endif /* KEY_SERVICE_H */