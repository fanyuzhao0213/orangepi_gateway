/*
 * fyz_led_app.c - FYZ LED 应用层控制程序
 *
 * 编译：gcc fyz_led_app.c -o fyz_led_app
 * 运行：sudo ./fyz_led_app
 *
 * 功能：
 *   1. 菜单交互，手动控制 LED 亮灭
 *   2. 切换触发模式（none / heartbeat / timer）
 *   3. 查询当前状态
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* ===== 驱动 sysfs 节点路径 ===== */
#define SYSFS_BASE   "/sys/devices/platform/fyz_led"
#define PATH_BR      SYSFS_BASE "/brightness"
#define PATH_TRIG    SYSFS_BASE "/trigger"

/* ===== 终端颜色宏（让输出更直观） ===== */
#define CLR_RED      "\033[31m"
#define CLR_GREEN    "\033[32m"
#define CLR_YELLOW   "\033[33m"
#define CLR_CYAN     "\033[36m"
#define CLR_RESET    "\033[0m"
#define CLR_BOLD     "\033[1m"

/* ========================================================
 * 基础 sysfs 读写
 * ======================================================== */

/**
 * sysfs_write() - 向 sysfs 节点写入字符串
 * @path: 节点完整路径
 * @val:  要写入的字符串
 * 返回：0 成功，-1 失败
 */
static int sysfs_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, CLR_RED "[错误] 打开 %s 失败: %s\n" CLR_RESET,
                path, strerror(errno));
        return -1;
    }
    ssize_t n = write(fd, val, strlen(val));
    close(fd);
    if (n < 0) {
        fprintf(stderr, CLR_RED "[错误] 写入 %s 失败: %s\n" CLR_RESET,
                path, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * sysfs_read() - 从 sysfs 节点读取字符串
 * @path: 节点完整路径
 * @buf:  输出缓冲区
 * @len:  缓冲区大小
 * 返回：实际读取字节数，-1 失败
 */
static int sysfs_read(const char *path, char *buf, int len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, CLR_RED "[错误] 打开 %s 失败: %s\n" CLR_RESET,
                path, strerror(errno));
        return -1;
    }
    int n = read(fd, buf, len - 1);
    close(fd);
    if (n > 0) {
        buf[n] = '\0';
        /* 去掉末尾换行符 */
        if (n > 1 && buf[n - 1] == '\n')
            buf[n - 1] = '\0';
    }
    return n;
}

/* ========================================================
 * LED 操作封装
 * ======================================================== */

/** led_set_brightness() - 设置亮度，val: 1=亮 0=灭 */
static int led_set_brightness(int val)
{
    return sysfs_write(PATH_BR, val ? "1" : "0");
}

/** led_set_trigger() - 设置触发模式，mode: "none"/"heartbeat"/"timer" */
static int led_set_trigger(const char *mode)
{
    return sysfs_write(PATH_TRIG, mode);
}

/** led_get_brightness() - 读取当前亮度，返回 0/1，-1 失败 */
static int led_get_brightness(void)
{
    char buf[8];
    if (sysfs_read(PATH_BR, buf, sizeof(buf)) < 0)
        return -1;
    return atoi(buf);
}

/** led_get_trigger() - 读取当前触发模式，写入 buf */
static int led_get_trigger(char *buf, int len)
{
    return sysfs_read(PATH_TRIG, buf, len);
}

/* ========================================================
 * 状态显示
 * ======================================================== */

static void show_status(void)
{
    char trig[64];
    int  br = led_get_brightness();

    printf("\n");
    printf(CLR_BOLD "┌─────────────────────────────┐\n" CLR_RESET);
    printf(CLR_BOLD "│        LED 当前状态          │\n" CLR_RESET);
    printf(CLR_BOLD "├─────────────────────────────┤\n" CLR_RESET);

    /* 亮度 */
    if (br < 0) {
        printf(CLR_BOLD "│ 亮度    : " CLR_RESET CLR_RED "读取失败\n" CLR_RESET);
    } else {
        printf(CLR_BOLD "│ 亮度    : " CLR_RESET "%s\n",
               br ? CLR_GREEN "● 亮" CLR_RESET : CLR_YELLOW "○ 灭" CLR_RESET);
    }

    /* 触发模式 */
    if (led_get_trigger(trig, sizeof(trig)) < 0) {
        printf(CLR_BOLD "│ 触发模式: " CLR_RESET CLR_RED "读取失败\n" CLR_RESET);
    } else {
        printf(CLR_BOLD "│ 触发模式: " CLR_RESET CLR_CYAN "%s\n" CLR_RESET, trig);
    }

    printf(CLR_BOLD "└─────────────────────────────┘\n" CLR_RESET);
}

/* ========================================================
 * 菜单
 * ======================================================== */

static void print_menu(void)
{
    printf("\n");
    printf(CLR_BOLD "===== FYZ LED 控制菜单 =====\n" CLR_RESET);
    printf("  1. 点亮 LED\n");
    printf("  2. 熄灭 LED\n");
    printf("  3. 设置触发模式：none（手动）\n");
    printf("  4. 设置触发模式：heartbeat（心跳）\n");
    printf("  5. 设置触发模式：timer（定时器 500ms）\n");
    printf("  6. 查看当前状态\n");
    printf("  7. 演示：流水灯（闪烁 10 次）\n");
    printf("  0. 退出\n");
    printf(CLR_BOLD "============================\n" CLR_RESET);
    printf("请输入选项: ");
}

/* ========================================================
 * 演示：手动闪烁 10 次
 * ======================================================== */

static void demo_blink(int count, int interval_ms)
{
    printf(CLR_CYAN "[演示] 开始闪烁 %d 次，间隔 %d ms...\n" CLR_RESET,
           count, interval_ms);

    /* 先切到 none 模式，确保手动控制有效 */
    led_set_trigger("none");

    for (int i = 0; i < count; i++) {
        led_set_brightness(1);
        printf("  [%2d] ● 亮\n", i + 1);
        usleep(interval_ms * 1000);

        led_set_brightness(0);
        printf("  [%2d] ○ 灭\n", i + 1);
        usleep(interval_ms * 1000);
    }
    printf(CLR_CYAN "[演示] 完成\n" CLR_RESET);
}

/* ========================================================
 * 主函数
 * ======================================================== */

int main(void)
{
    int choice;
    int ret;

    printf(CLR_BOLD CLR_CYAN
           "\nFYZ LED 应用层控制程序\n"
           "驱动路径: " SYSFS_BASE "\n"
           CLR_RESET);

    /* 程序启动时先显示一次状态 */
    show_status();

    while (1) {
        print_menu();

        if (scanf("%d", &choice) != 1) {
            /* 清除非法输入 */
            while (getchar() != '\n');
            printf(CLR_RED "输入无效，请重新输入\n" CLR_RESET);
            continue;
        }

        switch (choice) {

        case 1:
            ret = led_set_trigger("none");   /* 先切 none，再设亮度 */
            if (ret == 0) ret = led_set_brightness(1);
            printf(ret == 0
                   ? CLR_GREEN "✓ LED 已点亮\n" CLR_RESET
                   : CLR_RED   "✗ 操作失败（是否有 root 权限？）\n" CLR_RESET);
            break;

        case 2:
            ret = led_set_trigger("none");
            if (ret == 0) ret = led_set_brightness(0);
            printf(ret == 0
                   ? CLR_GREEN "✓ LED 已熄灭\n" CLR_RESET
                   : CLR_RED   "✗ 操作失败\n" CLR_RESET);
            break;

        case 3:
            ret = led_set_trigger("none");
            printf(ret == 0
                   ? CLR_GREEN "✓ 触发模式：none（手动控制）\n" CLR_RESET
                   : CLR_RED   "✗ 操作失败\n" CLR_RESET);
            break;

        case 4:
            ret = led_set_trigger("heartbeat");
            printf(ret == 0
                   ? CLR_GREEN "✓ 触发模式：heartbeat（心跳）\n" CLR_RESET
                   : CLR_RED   "✗ 操作失败\n" CLR_RESET);
            break;

        case 5:
            ret = led_set_trigger("timer");
            printf(ret == 0
                   ? CLR_GREEN "✓ 触发模式：timer（500ms 翻转）\n" CLR_RESET
                   : CLR_RED   "✗ 操作失败\n" CLR_RESET);
            break;

        case 6:
            show_status();
            break;

        case 7:
            demo_blink(10, 300);   /* 10 次，300ms 间隔 */
            break;

        case 0:
            printf(CLR_YELLOW "退出，LED 保持当前状态\n" CLR_RESET);
            return 0;

        default:
            printf(CLR_RED "无效选项，请输入 0~7\n" CLR_RESET);
            break;
        }
    }

    return 0;
}




