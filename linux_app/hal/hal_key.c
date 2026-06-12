/*
 * hal_key.c - 按键驱动抽象层
 *
 * 功能：
 *   1. 自动搜索 fyz-lradc-keys 设备节点
 *   2. 实时读取按键事件（按下/抬起/长按）
 *   3. 通过回调函数通知按键事件
 *
 * 设备节点: /dev/input/eventX
 * 设备名称: fyz-lradc-keys
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/input.h>
#include <pthread.h>
#include <time.h>
#include "hal_key.h"

/* ===== 终端颜色 ===== */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ===== 设备搜索路径 ===== */
#define INPUT_DIR       "/dev/input"
#define TARGET_NAME     "fyz-lradc-keys"
#define MAX_KEYS        16

/* ===== 全局变量 ===== */
static int          g_fd = -1;
static int          g_running = 1;
static pthread_t    g_key_thread;
static key_callback_t g_callback = NULL;
static char         g_dev_path[64] = {0};
static int          g_inited = 0;
static int          g_wakeup_pipe[2] = {-1, -1};

/* 按键统计结构 */
static key_info_t   g_stats[MAX_KEYS];
static int          g_stat_count = 0;
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ===== 键值映射表 ===== */
static const struct { int code; const char *name; } g_key_map[] = {
    {0,   "KEY_RESERVED"},  {1,  "KEY_ESC"},
    {2,   "KEY_1"},         {3,  "KEY_2"},
    {4,   "KEY_3"},         {5,  "KEY_4"},
    {28,  "KEY_ENTER"},     {29, "KEY_LEFTCTRL"},
    {56,  "KEY_LEFTALT"},   {57, "KEY_SPACE"},
    {59,  "KEY_F1"},        {60, "KEY_F2"},
    {103, "KEY_UP"},        {105, "KEY_LEFT"},
    {106, "KEY_RIGHT"},     {108, "KEY_DOWN"},
    {114, "KEY_VOLUMEDOWN"},{115, "KEY_VOLUMEUP"},
    {116, "KEY_POWER"},
};

/* ===== 工具函数 ===== */

static const char* keycode_to_name(int code)
{
    for (size_t i = 0; i < sizeof(g_key_map)/sizeof(g_key_map[0]); i++) {
        if (g_key_map[i].code == code) {
            return g_key_map[i].name;
        }
    }
    return "KEY_UNKNOWN";
}

static key_info_t* find_or_add_stat(int code)
{
    pthread_mutex_lock(&g_stats_mutex);
    for (int i = 0; i < g_stat_count; i++) {
        if (g_stats[i].code == code) {
            pthread_mutex_unlock(&g_stats_mutex);
            return &g_stats[i];
        }
    }
    if (g_stat_count >= MAX_KEYS) {
        pthread_mutex_unlock(&g_stats_mutex);
        return NULL;
    }
    key_info_t *s = &g_stats[g_stat_count++];
    memset(s, 0, sizeof(*s));
    s->code = code;
    s->name = keycode_to_name(code);
    pthread_mutex_unlock(&g_stats_mutex);
    return s;
}

static long timespec_diff_ms(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000
         + (end->tv_nsec - start->tv_nsec) / 1000000;
}

static void print_ts(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf(C_BLUE "[%s] " C_RESET, buf);
}

static int find_input_device(char *buf, int len)
{
    DIR *dir;
    struct dirent *ent;
    char path[512];
    char name[256];
    int fd;

    dir = opendir(INPUT_DIR);
    if (!dir) {
        perror("opendir /dev/input");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, ent->d_name);
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        memset(name, 0, sizeof(name));
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
            if (strstr(name, TARGET_NAME)) {
                close(fd);
                closedir(dir);
                size_t copy_len = (size_t)len - 1;
                if (copy_len > strlen(path)) copy_len = strlen(path);
                memcpy(buf, path, copy_len);
                buf[copy_len] = '\0';
                return 0;
            }
        }
        close(fd);
    }

    closedir(dir);
    return -1;
}

static void print_device_info(int fd)
{
    char name[256] = {0};
    char phys[256] = {0};
    struct input_id id;

    ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
    ioctl(fd, EVIOCGPHYS(sizeof(phys) - 1), phys);
    ioctl(fd, EVIOCGID, &id);

    printf(C_BOLD "\n┌─────────────────────────────────────────┐\n" C_RESET);
    printf(C_BOLD "│ 🔘 按键设备信息                          │\n" C_RESET);
    printf(C_BOLD "├─────────────────────────────────────────┤\n" C_RESET);
    printf(C_BOLD "│ 名称    : " C_RESET "%s\n", name);
    printf(C_BOLD "│ 物理路径: " C_RESET "%s\n", phys);
    printf(C_BOLD "│ 总线类型: " C_RESET "0x%04x\n", id.bustype);
    printf(C_BOLD "│ 厂商ID  : " C_RESET "0x%04x\n", id.vendor);
    printf(C_BOLD "│ 产品ID  : " C_RESET "0x%04x\n", id.product);
    printf(C_BOLD "│ 版本    : " C_RESET "0x%04x\n", id.version);

    unsigned char key_bits[KEY_MAX / 8 + 1];
    memset(key_bits, 0, sizeof(key_bits));
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);

    printf(C_BOLD "│ 支持的键值:\n" C_RESET);
    int count = 0;
    for (int i = 0; i < KEY_MAX; i++) {
        if (key_bits[i / 8] & (1 << (i % 8))) {
            printf(C_BOLD "│   " C_RESET "code=%-4d  %s\n", i, keycode_to_name(i));
            count++;
        }
    }
    if (count == 0) {
        printf(C_BOLD "│   " C_RESET "(未找到支持的键值)\n");
    }

    printf(C_BOLD "└─────────────────────────────────────────┘\n" C_RESET);
}

static void handle_event(struct input_event *ev)
{
    if (ev->type != EV_KEY) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* 只处理 KEY_2 (code=3)，忽略其他按键 */
    if (ev->code != 3) {
        return;
    }

    key_info_t *s = find_or_add_stat(ev->code);
    if (!s) return;

    s->type = (key_event_type_t)ev->value;

    print_ts();

    if (ev->value == 1) {
        s->press_count++;
        s->is_pressed = 1;
        s->press_time = now;
        s->hold_ms = 0;

        printf(C_GREEN "▼ 按下  " C_RESET C_BOLD "%-16s" C_RESET
               " code=%-4d  第 %d 次\n",
               s->name, ev->code, s->press_count);

    } else if (ev->value == 0) {
        s->is_pressed = 0;
        if (s->press_time.tv_sec > 0) {
            s->hold_ms = timespec_diff_ms(&s->press_time, &now);
        }

        printf(C_YELLOW "▲ 抬起  " C_RESET C_BOLD "%-16s" C_RESET
               " code=%-4d  按住 %ld ms\n",
               s->name, ev->code, s->hold_ms);

    } else if (ev->value == 2) {
        s->hold_ms = timespec_diff_ms(&s->press_time, &now);
        printf(C_BLUE "● 长按  " C_RESET C_BOLD "%-16s" C_RESET
               " code=%-4d  已按住 %ld ms\n",
               s->name, ev->code, s->hold_ms);
    }

    if (g_callback) {
        g_callback(s);
    }
}

/* ===== 按键监听线程 ===== */

static void* key_listener_thread(void *arg)
{
    (void)arg;
    struct input_event ev;
    ssize_t n;

    while (g_running) {
        fd_set rfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_SET(g_fd, &rfds);
        FD_SET(g_wakeup_pipe[0], &rfds);
        int maxfd = (g_fd > g_wakeup_pipe[0] ? g_fd : g_wakeup_pipe[0]) + 1;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(maxfd, &rfds, NULL, NULL, &tv);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            print_ts();
            fprintf(stderr, C_RED "✗ select 失败: %s\n" C_RESET, strerror(errno));
            break;
        }

        if (ret == 0) {
            continue;
        }

        if (FD_ISSET(g_wakeup_pipe[0], &rfds)) {
            char tmp;
            read(g_wakeup_pipe[0], &tmp, 1);
            print_ts();
            printf(C_YELLOW "☆ 收到关闭信号，退出监听\n" C_RESET);
            break;
        }

        if (FD_ISSET(g_fd, &rfds)) {
            n = read(g_fd, &ev, sizeof(ev));

            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                print_ts();
                fprintf(stderr, C_RED "✗ 读取按键事件失败: %s\n" C_RESET, strerror(errno));
                break;
            }

            if (n < (ssize_t)sizeof(ev)) {
                continue;
            }

            handle_event(&ev);
        }
    }

    print_ts();
    printf(C_YELLOW "☆ 按键监听线程已退出\n" C_RESET);
    return NULL;
}

/* ===== 公共 API 实现 ===== */

int hal_key_init(void)
{
    if (find_input_device(g_dev_path, sizeof(g_dev_path)) < 0) {
        return -1;
    }

    if (pipe(g_wakeup_pipe) < 0) {
        return -1;
    }

    g_fd = open(g_dev_path, O_RDONLY);
    if (g_fd < 0) {
        close(g_wakeup_pipe[0]);
        close(g_wakeup_pipe[1]);
        g_wakeup_pipe[0] = g_wakeup_pipe[1] = -1;
        return -1;
    }

    g_running = 1;
    if (pthread_create(&g_key_thread, NULL, key_listener_thread, NULL) != 0) {
        close(g_fd);
        g_fd = -1;
        close(g_wakeup_pipe[0]);
        close(g_wakeup_pipe[1]);
        g_wakeup_pipe[0] = g_wakeup_pipe[1] = -1;
        return -1;
    }

    g_inited = 1;
    return 0;
}

void hal_key_close(void)
{
    if (!g_inited) {
        return;
    }

    g_running = 0;

    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }

    if (g_wakeup_pipe[1] >= 0) {
        char c = 0;
        write(g_wakeup_pipe[1], &c, 1);
        close(g_wakeup_pipe[1]);
        g_wakeup_pipe[1] = -1;
    }

    if (g_wakeup_pipe[0] >= 0) {
        close(g_wakeup_pipe[0]);
        g_wakeup_pipe[0] = -1;
    }

    if (g_key_thread != 0) {
        pthread_join(g_key_thread, NULL);
        g_key_thread = 0;
    }

    g_inited = 0;
}

void hal_key_register_callback(key_callback_t callback)
{
    g_callback = callback;
}

int hal_key_is_inited(void)
{
    return g_inited;
}

const char* hal_key_get_device_path(void)
{
    return g_dev_path[0] ? g_dev_path : NULL;
}

int hal_key_get_supported_count(void)
{
    return g_stat_count;
}
