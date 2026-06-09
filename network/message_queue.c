#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "message_queue.h"

#define MAX_QUEUE_SIZE 100

typedef struct {
    message_t messages[MAX_QUEUE_SIZE];
    int head;           // 队头索引
    int tail;           // 队尾索引
    int count;          // 当前消息数量
    int running;        // 运行标志，0表示停止
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} message_queue_t;

static message_queue_t g_queue;

/**
 * @brief 初始化消息队列
 * @return 0: 成功, -1: 失败
 */
int mq_init(void)
{
    g_queue.head = 0;
    g_queue.tail = 0;
    g_queue.count = 0;
    g_queue.running = 1;

    if (pthread_mutex_init(&g_queue.mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&g_queue.cond, NULL) != 0) {
        pthread_mutex_destroy(&g_queue.mutex);
        return -1;
    }

    return 0;
}

/**
 * @brief 销毁消息队列
 */
void mq_destroy(void)
{
    pthread_cond_destroy(&g_queue.cond);
    pthread_mutex_destroy(&g_queue.mutex);
}

/**
 * @brief 推送消息到队列
 * @param msg 消息指针
 * @return 0: 成功, -1: 失败
 */
int mq_push(const message_t *msg)
{
    if (msg == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_queue.mutex);

    if (g_queue.count >= MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&g_queue.mutex);
        return -1;
    }

    // 复制消息到队尾
    g_queue.messages[g_queue.tail] = *msg;
    g_queue.tail = (g_queue.tail + 1) % MAX_QUEUE_SIZE;
    g_queue.count++;

    // 唤醒等待的消费者
    pthread_cond_signal(&g_queue.cond);

    pthread_mutex_unlock(&g_queue.mutex);
    return 0;
}

/**
 * @brief 从队列弹出消息
 * @param msg 输出消息指针
 * @return 0: 成功, -1: 队列为空
 */
int mq_pop(message_t *msg)
{
    if (msg == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_queue.mutex);

    // 等待消息或队列停止
    while (g_queue.count == 0 && g_queue.running) {
        pthread_cond_wait(&g_queue.cond, &g_queue.mutex);
    }

    // 检查是否是因为队列停止而唤醒
    if (!g_queue.running) {
        pthread_mutex_unlock(&g_queue.mutex);
        return -1;  // 返回错误表示队列已停止
    }

    // 从队头取出消息
    *msg = g_queue.messages[g_queue.head];
    g_queue.head = (g_queue.head + 1) % MAX_QUEUE_SIZE;
    g_queue.count--;

    pthread_mutex_unlock(&g_queue.mutex);
    return 0;
}

/**
 * @brief 获取队列中的消息数量
 * @return 消息数量
 */
int mq_count(void)
{
    int count;
    pthread_mutex_lock(&g_queue.mutex);
    count = g_queue.count;
    pthread_mutex_unlock(&g_queue.mutex);
    return count;
}

/**
 * @brief 停止消息队列（唤醒所有等待的线程）
 */
void mq_stop(void)
{
    pthread_mutex_lock(&g_queue.mutex);
    g_queue.running = 0;
    pthread_mutex_unlock(&g_queue.mutex);

    // 唤醒所有等待的线程
    pthread_cond_broadcast(&g_queue.cond);
}

