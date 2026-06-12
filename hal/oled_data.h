#ifndef OLED_DATA_H
#define OLED_DATA_H

/**
 * @file oled_data.h
 * @brief OLED 字体数据定义
 * @note 8x16 ASCII 字体数据
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 8x16 ASCII 字体数据
 * @note 支持 ASCII 0x20-0x7F（空格到DEL）
 */
extern const unsigned char font_8x16[][16];

#ifdef __cplusplus
}
#endif

#endif /* OLED_DATA_H */