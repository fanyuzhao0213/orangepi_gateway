#ifndef HAL_FILE_H
#define HAL_FILE_H

#include <stddef.h>

/**
 * @brief 文件操作返回值
 */
typedef enum {
    FILE_IO_SUCCESS = 0,        // 成功
    FILE_IO_ERROR_OPEN = -1,    // 打开文件失败
    FILE_IO_ERROR_READ = -2,    // 读取失败
    FILE_IO_ERROR_WRITE = -3,   // 写入失败
    FILE_IO_ERROR_CREATE = -4,  // 创建文件失败
    FILE_IO_ERROR_PERM = -5,    // 权限错误
    FILE_IO_ERROR_OTHER = -99   // 其他错误
} file_io_result_t;

/**
 * @brief 写入文件
 * @param path: 文件路径
 * @param data: 要写入的数据
 * @param len: 数据长度
 * @return 成功返回写入字节数，失败返回负值（见 file_io_result_t）
 */
int file_write(const char *path, const void *data, size_t len);

/**
 * @brief 读取文件
 * @param path: 文件路径
 * @param buf: 读取缓冲区
 * @param buf_size: 缓冲区大小
 * @return 成功返回读取字节数，失败返回负值（见 file_io_result_t）
 */
int file_read(const char *path, void *buf, size_t buf_size);

/**
 * @brief 检查文件是否存在
 * @param path: 文件路径
 * @return 1: 存在, 0: 不存在, -1: 错误
 */
int file_exists(const char *path);

/**
 * @brief 获取文件大小
 * @param path: 文件路径
 * @return 成功返回文件大小，失败返回 -1
 */
long file_size(const char *path);

/**
 * @brief 删除文件
 * @param path: 文件路径
 * @return 0: 成功, -1: 失败
 */
int file_delete(const char *path);

/**
 * @brief 创建目录（支持递归创建）
 * @param path: 目录路径
 * @return 0: 成功, -1: 失败
 */
int file_mkdir(const char *path);



//test
void test_error_cases(void);
void test_nested_path(void);
void test_write_read(const char *path);
#endif /* HAL_FILE_H */


