#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "hal_file.h"


/**
 * @brief 内部函数：创建目录路径
 * @param path: 完整路径
 * @return 0: 成功, -1: 失败
 */
static int create_parent_dir(const char *path)
{
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return -1;
    }

    // 从根目录开始逐级创建
    char *p = path_copy;
    if (p[0] == '/') {
        p++; // 跳过根目录
    }

    while (*p != '\0') {
        if (*p == '/') {
            *p = '\0';
            // 检查目录是否存在
            struct stat st;
            if (stat(path_copy, &st) != 0) {
                // 目录不存在，创建
                if (mkdir(path_copy, 0755) != 0) {
                    perror("mkdir");
                    free(path_copy);
                    return -1;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                // 存在但不是目录
                fprintf(stderr, "create_parent_dir: %s 存在但不是目录\n", path_copy);
                free(path_copy);
                return -1;
            }
            *p = '/';
        }
        p++;
    }

    free(path_copy);
    return 0;
}

/**
 * @brief 写入文件（自动创建文件）
 * @param path: 文件路径
 * @param data: 要写入的数据
 * @param len: 数据长度
 * @return 成功返回写入字节数，失败返回负值
 */
int file_write(const char *path, const void *data, size_t len)
{
    if (path == NULL || data == NULL) {
        fprintf(stderr, "file_write: 参数无效\n");
        return FILE_IO_ERROR_OTHER;
    }

    // 尝试创建父目录
    if (create_parent_dir(path) < 0) {
        fprintf(stderr, "file_write: 创建目录失败 %s\n", path);
        return FILE_IO_ERROR_CREATE;
    }

    // 以写入模式打开文件，如果不存在则自动创建
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        if (errno == EACCES || errno == EPERM) {
            return FILE_IO_ERROR_PERM;
        }
        return FILE_IO_ERROR_OPEN;
    }

    size_t written = fwrite(data, 1, len, fp);
    if (written != len) {
        perror("fwrite");
        fclose(fp);
        return FILE_IO_ERROR_WRITE;
    }

    fclose(fp);
    return (int)written;
}



/**
 * @brief 读取文件
 * @param path: 文件路径
 * @param buf: 读取缓冲区
 * @param buf_size: 缓冲区大小
 * @return 成功返回读取字节数，失败返回负值
 */
int file_read(const char *path, void *buf, size_t buf_size)
{
    if (path == NULL || buf == NULL || buf_size == 0) {
        fprintf(stderr, "file_read: 参数无效\n");
        return FILE_IO_ERROR_OTHER;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        // 文件不存在是正常情况，不需要打印错误
        if (errno == ENOENT) {
            return FILE_IO_ERROR_OPEN;  // 文件不存在
        }
        perror("fopen");
        if (errno == EACCES || errno == EPERM) {
            return FILE_IO_ERROR_PERM;
        }
        return FILE_IO_ERROR_OPEN;
    }

    size_t read = fread(buf, 1, buf_size, fp);
    if (ferror(fp)) {
        perror("fread");
        fclose(fp);
        return FILE_IO_ERROR_READ;
    }

    fclose(fp);
    return (int)read;
}

/**
 * @brief 检查文件是否存在
 * @param path: 文件路径
 * @return 1: 存在, 0: 不存在, -1: 错误
 */
int file_exists(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    struct stat st;
    if (stat(path, &st) == 0) {
        return 1;
    } else {
        if (errno == ENOENT) {
            return 0;
        }
        perror("stat");
        return -1;
    }
}

/**
 * @brief 获取文件大小
 * @param path: 文件路径
 * @return 成功返回文件大小，失败返回 -1
 */
long file_size(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        perror("stat");
        return -1;
    }

    return st.st_size;
}

/**
 * @brief 删除文件
 * @param path: 文件路径
 * @return 0: 成功, -1: 失败
 */
int file_delete(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    if (remove(path) != 0) {
        perror("remove");
        return -1;
    }

    return 0;
}

/**
 * @brief 创建目录（支持递归创建）
 * @param path: 目录路径
 * @return 0: 成功, -1: 失败
 */
int file_mkdir(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    // 检查目录是否已存在
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            // 目录已存在
            return 0;
        } else {
            // 存在但不是目录
            fprintf(stderr, "file_mkdir: %s 存在但不是目录\n", path);
            return -1;
        }
    }

    // 目录不存在，递归创建父目录
    return create_parent_dir(path);
}



void test_write_read(const char *path)
{
    printf("\n=== 测试写入和读取 ===\n");

    // 测试数据
    const char *test_data = "Hello, HAL File I/O!\nThis is a test.\n";
    int len = strlen(test_data);

    // 写入文件
    printf("写入文件: %s\n", path);
    int result = file_write(path, test_data, len);
    if (result < 0) {
        printf("写入失败: %d\n", result);
        return;
    }
    printf("写入成功: %d 字节\n", result);

    // 检查文件是否存在（修正）
    int exists = file_exists(path);
    if (exists == 1) {
        printf("文件存在\n");
    } else if (exists == 0) {
        printf("文件不存在\n");
    } else {
        printf("检查文件状态出错\n");
    }

    // 获取文件大小（只有文件存在时才获取）
    if (exists == 1) {
        long size = file_size(path);
        if (size >= 0) {
            printf("文件大小: %ld 字节\n", size);
        } else {
            printf("获取文件大小失败\n");
        }
    } else {
        printf("文件不存在，无法获取大小\n");
    }

    // 读取文件（只有文件存在时才读取）
    if (exists == 1) {
        char buf[256] = {0};
        int read_len = file_read(path, buf, sizeof(buf) - 1);
        if (read_len < 0) {
            printf("读取失败: %d\n", read_len);
            return;
        }
        printf("读取成功: %d 字节\n", read_len);
        printf("读取内容:\n%s\n", buf);
    } else {
        printf("文件不存在，跳过读取\n");
    }
}


void test_nested_path(void)
{
    printf("\n=== 测试嵌套路径 ===\n");
    const char *path = "/home/orangepi/fyz_test/status.dat";
    const char *data = "LED_STATUS=1\n";

    int result = file_write(path, data, strlen(data));
    if (result < 0) {
        printf("写入嵌套路径失败: %d\n", result);
    } else {
        printf("写入嵌套路径成功: %d 字节\n", result);
        // 验证
        char buf[64];
        int read_len = file_read(path, buf, sizeof(buf));
        printf("读取内容: %.*s\n", read_len, buf);
    }
}

void test_error_cases(void)
{
    printf("\n=== 测试错误情况 ===\n");

    // 测试读取不存在的文件
    int result = file_read("/nonexistent/file.txt", NULL, 0);
    printf("读取不存在文件: %d (期望: %d)\n", result, FILE_IO_ERROR_OPEN);

    // 测试写入只读目录
    result = file_write("/proc/test.txt", "test", 4);
    printf("写入只读目录: %d (期望: %d)\n", result, FILE_IO_ERROR_PERM);
}


