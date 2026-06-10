#ifndef __HAL_OLED_H
#define __HAL_OLED_H

#include <stdint.h>

/* OLED 屏幕参数 */
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_PAGES   8

/*FontSize参数取值*/
#define OLED_8X16				8
#define OLED_6X8				6

/*IsFilled参数数值*/
#define OLED_UNFILLED			0
#define OLED_FILLED				1

/*函数声明*/

/*初始化函数*/
int hal_oled_init(int i2c_bus, uint8_t i2c_addr);

/*更新函数*/
void hal_oled_update(void);
void hal_oled_update_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/*显存控制函数*/
void hal_oled_clear(void);
void hal_oled_clear_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void hal_oled_reverse(void);
void hal_oled_reverse_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/*显示函数*/
void hal_oled_show_char(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void hal_oled_show_string(int16_t X, int16_t Y, const char *String, uint8_t FontSize);
void hal_oled_show_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void hal_oled_show_signed_num(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void hal_oled_show_hex_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void hal_oled_show_bin_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void hal_oled_show_float_num(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void hal_oled_show_image(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);
void hal_oled_printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);

/*绘图函数*/
void hal_oled_draw_point(int16_t X, int16_t Y);
uint8_t hal_oled_get_point(int16_t X, int16_t Y);
void hal_oled_draw_line(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void hal_oled_draw_rectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void hal_oled_draw_triangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void hal_oled_draw_circle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);
void hal_oled_draw_ellipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
void hal_oled_draw_arc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

/*关闭OLED*/
void hal_oled_deinit(void);

#endif
