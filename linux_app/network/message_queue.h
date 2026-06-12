#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stddef.h>

/**
 * @brief 消息结构
 */
typedef struct {
    int client_fd;          // 客户端文件描述符
    char data[256];         // 消息数据
    size_t data_len;        // 数据长度
} message_t;

/**
 * @brief 初始化消息队列
 * @return 0: 成功, -1: 失败
 */
int mq_init(void);

/**
 * @brief 销毁消息队列
 */
void mq_destroy(void);

/**
 * @brief 推送消息到队列
 * @param msg 消息指针
 * @return 0: 成功, -1: 失败
 */
int mq_push(const message_t *msg);

/**
 * @brief 从队列弹出消息
 * @param msg 输出消息指针
 * @return 0: 成功, -1: 队列为空
 */
int mq_pop(message_t *msg);

/**
 * @brief 获取队列中的消息数量
 * @return 消息数量
 */
int mq_count(void);

/**
 * @brief 停止消息队列（唤醒所有等待的线程）
 */
void mq_stop(void);

#endif /* MESSAGE_QUEUE_H */

