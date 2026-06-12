/*
 * fyz_key_test.c - LRADC 按键测试应用
 *
 * 功能：
 *   1. 自动搜索 fyz-lradc-keys 设备节点
 *   2. 实时显示按键事件（按下 / 抬起 / 长按计时）
 *   3. 统计按键次数
 *   4. 支持 Ctrl+C 优雅退出并打印统计结果
 *
 * 编译：gcc fyz_key_test.c -o fyz_key_test
 * 运行：sudo ./fyz_key_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>   /* struct input_event、EVIOCGNAME、KEY_* */

/* ===== 终端颜色 ===== */
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"
#define C_BOLD    "\033[1m"
#define C_RESET   "\033[0m"

/* ===== 设备搜索路径 ===== */
#define INPUT_DIR     "/dev/input"
#define TARGET_NAME   "fyz-lradc-keys"   /* 与驱动中 input->name 一致 */
#define MAX_KEYS      16                  /* 最多统计的键值数量 */

/* ===== 全局变量 ===== */
static volatile int g_running = 1;  /* 主循环控制，Ctrl+C 置0 */

/* 按键统计结构 */
struct key_stat {
    int      code;          /* 键值 */
    char     name[32];      /* 键名 */
    int      press_count;   /* 按下次数 */
    long     hold_ms;       /* 最近一次按住时长（ms） */
    struct timespec press_time; /* 按下时间戳 */
    int      is_pressed;    /* 当前是否按下 */
};

static struct key_stat g_stats[MAX_KEYS];
static int             g_stat_count = 0;

/* ================================================================
 * 工具函数
 * ================================================================ */

/**
 * sig_handler() - 捕获 Ctrl+C，优雅退出
 */
static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/**
 * keycode_to_name() - 键值转可读名称
 * @code: linux,code 键值
 * 返回：键名字符串
 */
static const char *keycode_to_name(int code)
{
    /* 常用键值映射表 */
    static const struct { int code; const char *name; } map[] = {
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
    for (int i = 0; i < (int)(sizeof(map)/sizeof(map[0])); i++)
        if (map[i].code == code)
            return map[i].name;
    return "KEY_UNKNOWN";
}

/**
 * find_or_add_stat() - 查找或新增按键统计项
 */
static struct key_stat *find_or_add_stat(int code)
{
    for (int i = 0; i < g_stat_count; i++)
        if (g_stats[i].code == code)
            return &g_stats[i];

    if (g_stat_count >= MAX_KEYS)
        return NULL;

    struct key_stat *s = &g_stats[g_stat_count++];
    memset(s, 0, sizeof(*s));
    s->code = code;
    strncpy(s->name, keycode_to_name(code), sizeof(s->name) - 1);
    return s;
}

/**
 * timespec_diff_ms() - 计算两个时间点的差值（毫秒）
 */
static long timespec_diff_ms(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec  - start->tv_sec)  * 1000
         + (end->tv_nsec - start->tv_nsec) / 1000000;
}

/* ================================================================
 * 设备搜索
 * ================================================================ */

/**
 * find_input_device() - 在 /dev/input/ 下搜索目标设备
 * @buf: 输出找到的设备路径
 * @len: 缓冲区长度
 *
 * 遍历 /dev/input/event* ，通过 EVIOCGNAME ioctl 读取设备名，
 * 匹配 TARGET_NAME 字符串。
 *
 * 返回：0 找到，-1 未找到
 */
static int find_input_device(char *buf, int len)
{
    DIR           *dir;
    struct dirent *ent;
    char           path[64];
    char           name[256];
    int            fd;

    dir = opendir(INPUT_DIR);
    if (!dir) {
        perror("opendir /dev/input");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        /* 只检查 event* 设备节点 */
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, ent->d_name);

        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        /* EVIOCGNAME ioctl：获取设备名称（与驱动中 input->name 对应） */
        memset(name, 0, sizeof(name));
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
            if (strstr(name, TARGET_NAME)) {
                /* 找到目标设备 */
                close(fd);
                closedir(dir);
                strncpy(buf, path, len - 1);
                buf[len - 1] = '\0';
                printf(C_GREEN "✓ 找到设备：%s (%s)\n" C_RESET, path, name);
                return 0;
            }
        }
        close(fd);
    }

    closedir(dir);
    return -1;
}

/* ================================================================
 * 设备信息打印
 * ================================================================ */

/**
 * print_device_info() - 打印设备支持的按键信息
 * @fd: 已打开的设备文件描述符
 */
static void print_device_info(int fd)
{
    char name[256] = {0};
    char phys[256] = {0};
    struct input_id id;

    /* 获取设备基本信息 */
    ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
    ioctl(fd, EVIOCGPHYS(sizeof(phys) - 1), phys);
    ioctl(fd, EVIOCGID, &id);

    printf("\n");
    printf(C_BOLD "┌─────────────────────────────────────┐\n" C_RESET);
    printf(C_BOLD "│           设备信息                   │\n" C_RESET);
    printf(C_BOLD "├─────────────────────────────────────┤\n" C_RESET);
    printf(C_BOLD "│ 名称    : " C_RESET "%s\n", name);
    printf(C_BOLD "│ 物理路径: " C_RESET "%s\n", phys);
    printf(C_BOLD "│ 总线类型: " C_RESET "0x%04x\n", id.bustype);
    printf(C_BOLD "│ 厂商ID  : " C_RESET "0x%04x\n", id.vendor);
    printf(C_BOLD "│ 产品ID  : " C_RESET "0x%04x\n", id.product);
    printf(C_BOLD "│ 版本    : " C_RESET "0x%04x\n", id.version);

    /* 获取支持的按键位图 */
    unsigned char key_bits[KEY_MAX / 8 + 1];
    memset(key_bits, 0, sizeof(key_bits));
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);

    printf(C_BOLD "│ 支持的键值:\n" C_RESET);
    int found = 0;
    for (int i = 0; i < KEY_MAX; i++) {
        /* 检查第 i 位是否置1 */
        if (key_bits[i / 8] & (1 << (i % 8))) {
            printf("│   code=%-4d  %s\n", i, keycode_to_name(i));
            found++;
        }
    }
    if (!found)
        printf("│   (未找到支持的键值)\n");

    printf(C_BOLD "└─────────────────────────────────────┘\n" C_RESET);
    printf("\n");
    printf(C_CYAN "开始监听按键事件，按 Ctrl+C 退出...\n\n" C_RESET);
}

/* ================================================================
 * 事件处理
 * ================================================================ */

/**
 * handle_event() - 处理一个 input_event
 * @ev: 从 /dev/input/eventX 读到的事件结构体
 *
 * struct input_event 结构：
 *   time  : 事件发生的时间戳（秒+微秒）
 *   type  : 事件类型（EV_KEY=1, EV_SYN=0 等）
 *   code  : 键值（KEY_2=3, KEY_LEFTCTRL=29 等）
 *   value : 1=按下, 0=抬起, 2=长按重复
 */
static void handle_event(struct input_event *ev)
{
    struct timespec now;

    /* 只处理按键事件，跳过 EV_SYN 同步事件 */
    if (ev->type != EV_KEY)
        return;

    clock_gettime(CLOCK_MONOTONIC, &now);

    struct key_stat *s = find_or_add_stat(ev->code);
    if (!s) return;

    if (ev->value == 1) {
        /* ── 按键按下 ── */
        s->press_count++;
        s->is_pressed  = 1;
        s->press_time  = now;  /* 记录按下时间，用于计算按住时长 */

        printf(C_GREEN "[按下] " C_RESET
               C_BOLD  "%-16s" C_RESET
               " code=%-4d  "
               "第 %d 次\n",
               s->name, ev->code, s->press_count);

    } else if (ev->value == 0) {
        /* ── 按键抬起 ── */
        s->is_pressed = 0;

        /* 计算本次按住时长 */
        if (s->press_time.tv_sec > 0)
            s->hold_ms = timespec_diff_ms(&s->press_time, &now);

        printf(C_YELLOW "[抬起] " C_RESET
               C_BOLD   "%-16s" C_RESET
               " code=%-4d  "
               "按住 %ld ms\n",
               s->name, ev->code, s->hold_ms);

    } else if (ev->value == 2) {
        /* ── 长按重复（内核自动产生） ── */
        long hold = timespec_diff_ms(&s->press_time, &now);
        printf(C_BLUE  "[长按] " C_RESET
               C_BOLD  "%-16s" C_RESET
               " code=%-4d  "
               "已按住 %ld ms\n",
               s->name, ev->code, hold);
    }
}

/* ================================================================
 * 统计结果打印
 * ================================================================ */

static void print_summary(void)
{
    printf("\n");
    printf(C_BOLD "╔══════════════════════════════════════╗\n" C_RESET);
    printf(C_BOLD "║            按键统计结果               ║\n" C_RESET);
    printf(C_BOLD "╠══════════════════════════════════════╣\n" C_RESET);

    if (g_stat_count == 0) {
        printf(C_BOLD "║  " C_RESET "（没有检测到任何按键事件）\n");
    } else {
        printf(C_BOLD "║  %-16s  %-8s  %-10s\n" C_RESET,
               "键名", "按下次数", "最后按住时长");
        printf(C_BOLD "╠══════════════════════════════════════╣\n" C_RESET);
        for (int i = 0; i < g_stat_count; i++) {
            printf("║  %-16s  %-8d  %ld ms\n",
                   g_stats[i].name,
                   g_stats[i].press_count,
                   g_stats[i].hold_ms);
        }
    }

    printf(C_BOLD "╚══════════════════════════════════════╝\n" C_RESET);
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    char               dev_path[64];
    int                fd;
    struct input_event ev;
    ssize_t            n;

    printf(C_BOLD C_CYAN
           "\nFYZ LRADC 按键测试程序\n"
           "==============================\n"
           C_RESET);

    /* 1. 注册信号处理（Ctrl+C 优雅退出） */
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* 2. 自动搜索设备节点 */
    printf("正在搜索设备 [%s]...\n", TARGET_NAME);
    if (find_input_device(dev_path, sizeof(dev_path)) < 0) {
        fprintf(stderr,
                C_RED "未找到设备，请确认：\n"
                "  1. 驱动已加载：sudo insmod fyz_lradc_keys.ko\n"
                "  2. 系统驱动已卸载：sudo rmmod sun4i_lradc_keys\n"
                "  3. 有读取权限：sudo ./fyz_key_test\n"
                C_RESET);
        return 1;
    }

    /* 3. 打开设备节点（阻塞模式，read 会等待事件） */
    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, C_RED "打开设备失败: %s\n" C_RESET, strerror(errno));
        return 1;
    }

    /* 4. 打印设备信息 */
    print_device_info(fd);

    /* 5. 主循环：阻塞读取事件 */
    while (g_running) {
        /*
         * read() 阻塞等待，直到有事件发生才返回。
         * 每次读取一个 struct input_event（固定大小 24 字节）。
         * Ctrl+C 触发信号后 read 返回 -EINTR，退出循环。
         */
        n = read(fd, &ev, sizeof(ev));

        if (n < 0) {
            if (errno == EINTR) {
                /* 被信号中断（Ctrl+C），正常退出 */
                break;
            }
            fprintf(stderr, C_RED "读取事件失败: %s\n" C_RESET, strerror(errno));
            break;
        }

        if (n < (ssize_t)sizeof(ev)) {
            /* 读到的数据不完整，跳过 */
            fprintf(stderr, C_YELLOW "警告：事件数据不完整，跳过\n" C_RESET);
            continue;
        }

        /* 处理事件 */
        handle_event(&ev);
    }

    /* 6. 关闭设备，打印统计 */
    close(fd);
    print_summary();
    printf(C_CYAN "程序已退出\n" C_RESET);

    return 0;
}




