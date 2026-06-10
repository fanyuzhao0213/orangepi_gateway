/**
 * @file hal_oled.c
 * @brief OLED驱动HAL层实现
 * @note 基于江协科技OLED驱动，适配Linux I2C接口
 */

#include "hal_oled.h"
#include "hal_oled_data.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <stdlib.h>

/*全局变量*********************/

static uint8_t OLED_DisplayBuf[8][128];
static int g_i2c_fd = -1;
static uint8_t g_i2c_addr = 0x3C;

/*********************全局变量*/


/*通信协议*********************/

static void OLED_WriteCommand(uint8_t Command)
{
    if (g_i2c_fd < 0) {
        return;
    }

    uint8_t buf[2] = {0x00, Command};
    if (write(g_i2c_fd, buf, 2) != 2) {
        fprintf(stderr, "I2C write cmd failed: %s\n", strerror(errno));
    }
}

static void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
    if (g_i2c_fd < 0 || Data == NULL || Count == 0) {
        return;
    }

    uint8_t *buf = (uint8_t *)malloc(Count + 1);
    if (buf == NULL) {
        fprintf(stderr, "malloc failed for OLED_WriteData\n");
        return;
    }

    buf[0] = 0x40;
    memcpy(&buf[1], Data, Count);

    if (write(g_i2c_fd, buf, Count + 1) != (int)(Count + 1)) {
        fprintf(stderr, "I2C write data failed: %s\n", strerror(errno));
    }

    free(buf);
}

/*********************通信协议*/


/*硬件配置*********************/

static void OLED_SetCursor(uint8_t Page, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Page);
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
	OLED_WriteCommand(0x00 | (X & 0x0F));
}

/*********************硬件配置*/


/*工具函数*********************/

static uint8_t OLED_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
	int16_t i, j, c = 0;

	for (i = 0, j = nvert - 1; i < nvert; j = i++) {
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i])) {
			c = !c;
		}
	}
	return c;
}

static uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14 * 180;
	if (StartAngle < EndAngle) {
		if (PointAngle >= StartAngle && PointAngle <= EndAngle) {
			return 1;
		}
	} else {
		if (PointAngle >= StartAngle || PointAngle <= EndAngle) {
			return 1;
		}
	}
	return 0;
}

/*********************工具函数*/


/*初始化函数*********************/

int hal_oled_init(int i2c_bus, uint8_t i2c_addr)
{
    char i2c_dev[64];

    g_i2c_addr = i2c_addr;

    snprintf(i2c_dev, sizeof(i2c_dev), "/dev/i2c-%d", i2c_bus);
    g_i2c_fd = open(i2c_dev, O_WRONLY);
    if (g_i2c_fd < 0) {
        fprintf(stderr, "无法打开I2C设备 %s: %s\n", i2c_dev, strerror(errno));
        return -1;
    }

    if (ioctl(g_i2c_fd, I2C_SLAVE, i2c_addr) < 0) {
        fprintf(stderr, "无法设置I2C从机地址 0x%02X: %s\n", i2c_addr, strerror(errno));
        close(g_i2c_fd);
        g_i2c_fd = -1;
        return -1;
    }

    OLED_WriteCommand(0xAE);
	OLED_WriteCommand(0xD5);
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);
	OLED_WriteCommand(0xA1);
	OLED_WriteCommand(0xC8);
	OLED_WriteCommand(0xDA);
	OLED_WriteCommand(0x12);
	OLED_WriteCommand(0x81);
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);
	OLED_WriteCommand(0xA6);
	OLED_WriteCommand(0x8D);
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);

	hal_oled_clear();
	hal_oled_update();

    return 0;
}

void hal_oled_deinit(void)
{
    if (g_i2c_fd >= 0) {
        OLED_WriteCommand(0xAE);
        close(g_i2c_fd);
        g_i2c_fd = -1;
    }
}

/*********************初始化函数*/


/*更新函数*********************/

void hal_oled_update(void)
{
	uint8_t j;
	for (j = 0; j < 8; j++) {
		OLED_SetCursor(j, 0);
		OLED_WriteData(OLED_DisplayBuf[j], 128);
	}
}

void hal_oled_update_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t j;
	int16_t Page, Page1;

	Page = Y / 8;
	Page1 = (Y + Height - 1) / 8 + 1;
	if (Y < 0) {
		Page -= 1;
		Page1 -= 1;
	}

	for (j = Page; j < Page1; j++) {
		if (X >= 0 && X <= 127 && j >= 0 && j <= 7) {
			OLED_SetCursor(j, X);
			OLED_WriteData(&OLED_DisplayBuf[j][X], Width);
		}
	}
}

/*********************更新函数*/


/*显存控制函数*********************/

void hal_oled_clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j++) {
		for (i = 0; i < 128; i++) {
			OLED_DisplayBuf[j][i] = 0x00;
		}
	}
}

void hal_oled_clear_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;

	for (j = Y; j < Y + Height; j++) {
		for (i = X; i < X + Width; i++) {
			if (i >= 0 && i <= 127 && j >= 0 && j <= 63) {
				OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
			}
		}
	}
}

void hal_oled_reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j++) {
		for (i = 0; i < 128; i++) {
			OLED_DisplayBuf[j][i] ^= 0xFF;
		}
	}
}

void hal_oled_reverse_area(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;

	for (j = Y; j < Y + Height; j++) {
		for (i = X; i < X + Width; i++) {
			if (i >= 0 && i <= 127 && j >= 0 && j <= 63) {
				OLED_DisplayBuf[j / 8][i] ^= 0x01 << (j % 8);
			}
		}
	}
}

/*********************显存控制函数*/


/*显示函数*********************/

void hal_oled_show_char(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
	if (FontSize == OLED_8X16) {
		hal_oled_show_image(X, Y, 8, 16, (uint8_t *)OLED_F8x16[Char - ' ']);
	} else if(FontSize == OLED_6X8) {
		hal_oled_show_image(X, Y, 6, 8, (uint8_t *)OLED_F6x8[Char - ' ']);
	}
}

void hal_oled_show_string(int16_t X, int16_t Y, const char *String, uint8_t FontSize)
{
	uint16_t i = 0;
	char SingleChar[5];
	uint8_t CharLength = 0;
	uint16_t XOffset = 0;
	uint16_t pIndex;

	while (String[i] != '\0') {

#ifdef OLED_CHARSET_UTF8
		if ((String[i] & 0x80) == 0x00) {
			CharLength = 1;
			SingleChar[0] = String[i++];
			SingleChar[1] = '\0';
		} else if ((String[i] & 0xE0) == 0xC0) {
			CharLength = 2;
			SingleChar[0] = String[i++];
			SingleChar[1] = String[i++];
			SingleChar[2] = '\0';
		} else if ((String[i] & 0xF0) == 0xE0) {
			CharLength = 3;
			SingleChar[0] = String[i++];
			SingleChar[1] = String[i++];
			SingleChar[2] = String[i++];
			SingleChar[3] = '\0';
		} else if ((String[i] & 0xF8) == 0xF0) {
			CharLength = 4;
			SingleChar[0] = String[i++];
			SingleChar[1] = String[i++];
			SingleChar[2] = String[i++];
			SingleChar[3] = String[i++];
			SingleChar[4] = '\0';
		} else {
			i++;
			continue;
		}
#endif

#ifdef OLED_CHARSET_GB2312
		CharLength = 2;
		SingleChar[0] = String[i++];
		SingleChar[1] = String[i++];
		SingleChar[2] = '\0';
#endif

		if (FontSize == OLED_8X16) {
			if (CharLength == 1) {
				hal_oled_show_char(X + XOffset, Y, SingleChar[0], FontSize);
				XOffset += 8;
			} else {
				for (pIndex = 0; ; pIndex++) {
					if (OLED_CF16x16[pIndex].Index[0] == '\0') {
						hal_oled_show_image(X + XOffset, Y, 16, 16, (uint8_t *)OLED_CF16x16[pIndex].Data);
						break;
					}
					if (strcmp(OLED_CF16x16[pIndex].Index, SingleChar) == 0) {
						hal_oled_show_image(X + XOffset, Y, 16, 16, (uint8_t *)OLED_CF16x16[pIndex].Data);
						break;
					}
				}
				XOffset += 16;
			}
		} else if (FontSize == OLED_6X8) {
			if (CharLength == 1) {
				hal_oled_show_char(X + XOffset, Y, SingleChar[0], FontSize);
				XOffset += 6;
			} else {
				hal_oled_show_char(X + XOffset, Y, '?', FontSize);
				XOffset += 6;
			}
		}
	}
}

void hal_oled_show_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	char Buffer[12];
	sprintf(Buffer, "%d", Number);

	for (i = 0; i < Length; i++) {
		if (Buffer[i] == '\0') {
			for (; i < Length; i++) {
				hal_oled_show_char(X + 8 * i, Y, ' ', FontSize);
			}
			break;
		}
		hal_oled_show_char(X + 8 * i, Y, Buffer[i], FontSize);
	}
}

void hal_oled_show_signed_num(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	char Buffer[12];

	if (Number < 0) {
		Number = -Number;
		hal_oled_show_char(X, Y, '-', FontSize);
		for (i = 0; i < Length - 1; i++) {
			Buffer[i] = ' ';
		}
		Buffer[i] = '\0';
		sprintf(Buffer + (Length - 1 - strlen(Buffer)), "%d", Number);
		for (i = 0; i < Length - 1; i++) {
			hal_oled_show_char(X + 8 * (i + 1), Y, Buffer[i], FontSize);
		}
	} else {
		hal_oled_show_char(X, Y, '+', FontSize);
		sprintf(Buffer, "%d", Number);
		for (i = 0; i < Length - 1; i++) {
			if (Buffer[i] == '\0') {
				for (; i < Length - 1; i++) {
					hal_oled_show_char(X + 8 * (i + 1), Y, ' ', FontSize);
				}
				break;
			}
			hal_oled_show_char(X + 8 * (i + 1), Y, Buffer[i], FontSize);
		}
	}
}

void hal_oled_show_hex_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	char Buffer[12];
	sprintf(Buffer, "%X", Number);

	for (i = 0; i < Length; i++) {
		if (Buffer[i] == '\0') {
			for (; i < Length; i++) {
				hal_oled_show_char(X + 8 * i, Y, ' ', FontSize);
			}
			break;
		}
		hal_oled_show_char(X + 8 * i, Y, Buffer[i], FontSize);
	}
}

void hal_oled_show_bin_num(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	char Buffer[33];
	sprintf(Buffer, "%lu", (unsigned long)Number);

	for (i = 0; i < Length; i++) {
		if (i >= 32 || Buffer[i - (32 - strlen(Buffer))] == '\0') {
			hal_oled_show_char(X + 8 * i, Y, '0', FontSize);
		} else {
			hal_oled_show_char(X + 8 * i, Y, Buffer[i - (32 - strlen(Buffer))], FontSize);
		}
	}
}

void hal_oled_show_float_num(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
	char Buffer[30];
	char IntStr[15];
	char FraStr[15];
	uint8_t i;

	sprintf(Buffer, "%f", Number);
	sscanf(Buffer, "%[^.].%s", IntStr, FraStr);

	for (i = 0; i < IntLength; i++) {
		if (IntStr[i] == '\0') {
			for (; i < IntLength; i++) {
				hal_oled_show_char(X + 8 * i, Y, ' ', FontSize);
			}
			break;
		}
		hal_oled_show_char(X + 8 * i, Y, IntStr[i], FontSize);
	}

	hal_oled_show_char(X + 8 * i, Y, '.', FontSize);
	i++;

	for (i = 0; i < FraLength; i++) {
		if (FraStr[i] == '\0') {
			for (; i < FraLength; i++) {
				hal_oled_show_char(X + 8 * (i + IntLength + 1), Y, '0', FontSize);
			}
			break;
		}
		hal_oled_show_char(X + 8 * (i + IntLength + 1), Y, FraStr[i], FontSize);
	}
}

void hal_oled_show_image(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
	uint16_t i, j;
	uint8_t ShowData;

	for (i = 0; i < Height / 8; i++) {
		for (j = 0; j < Width; j++) {
			ShowData = Image[i * Width + j];
			if (X >= 0 && X <= 127 && Y + 8 * i >= 0 && Y + 8 * i <= 63) {
				OLED_DisplayBuf[(Y + 8 * i) / 8][X + j] |= ShowData;
			}
		}
	}
}

void hal_oled_printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{
	char Buffer[256];
	va_list args;

	va_start(args, format);
	vsnprintf(Buffer, sizeof(Buffer), format, args);
	va_end(args);

	hal_oled_show_string(X, Y, Buffer, FontSize);
}

/*********************显示函数*/


/*绘图函数*********************/

void hal_oled_draw_point(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >= 0 && Y <= 63) {
		OLED_DisplayBuf[Y / 8][X] |= 0x01 << (Y % 8);
	}
}

uint8_t hal_oled_get_point(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >= 0 && Y <= 63) {
		return (OLED_DisplayBuf[Y / 8][X] >> (Y % 8)) & 0x01;
	}
	return 0;
}

void hal_oled_draw_line(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
	int16_t i;
	int16_t XDistance = X1 - X0;
	int16_t YDistance = Y1 - Y0;
	int16_t XDir = XDistance > 0 ? 1 : -1;
	int16_t YDir = YDistance > 0 ? 1 : -1;
	int16_t Error = 0;

	XDistance = XDistance >= 0 ? XDistance : -XDistance;
	YDistance = YDistance >= 0 ? YDistance : -YDistance;

	if (XDistance > YDistance) {
		for (i = 0; i <= XDistance; i++) {
			hal_oled_draw_point(X0 + XDir * i, Y0 + YDir * (YDistance * i + XDistance / 2) / XDistance);
			Error += YDistance;
			if (Error >= XDistance) {
				Error -= XDistance;
				Y0 += YDir;
			}
		}
	} else {
		for (i = 0; i <= YDistance; i++) {
			hal_oled_draw_point(X0 + XDir * (XDistance * i + YDistance / 2) / YDistance, Y0 + YDir * i);
			Error += XDistance;
			if (Error >= YDistance) {
				Error -= YDistance;
				X0 += XDir;
			}
		}
	}
}

void hal_oled_draw_rectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	uint16_t i, j;

	if (IsFilled == OLED_UNFILLED) {
		for (i = 0; i < Height; i++) {
			hal_oled_draw_point(X, Y + i);
			hal_oled_draw_point(X + Width - 1, Y + i);
		}
		for (i = 0; i < Width; i++) {
			hal_oled_draw_point(X + i, Y);
			hal_oled_draw_point(X + i, Y + Height - 1);
		}
	} else {
		for (i = 0; i < Height; i++) {
			for (j = 0; j < Width; j++) {
				hal_oled_draw_point(X + j, Y + i);
			}
		}
	}
}

void hal_oled_draw_triangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
	int16_t i, j;
	int16_t XMin, XMax;
	int16_t XTemp1, XTemp2;
	int16_t YTemp = 0;
	int16_t *vertx, *verty;

	if (IsFilled == OLED_UNFILLED) {
		hal_oled_draw_line(X0, Y0, X1, Y1);
		hal_oled_draw_line(X1, Y1, X2, Y2);
		hal_oled_draw_line(X2, Y2, X0, Y0);
	} else {
		XMin = (X0 < X1) ? X0 : X1;
		XMin = (XMin < X2) ? XMin : X2;
		XMax = (X0 > X1) ? X0 : X1;
		XMax = (XMax > X2) ? XMax : X2;

		vertx = (int16_t *)malloc(sizeof(int16_t) * 3);
		verty = (int16_t *)malloc(sizeof(int16_t) * 3);
		if (vertx == NULL || verty == NULL) {
			free(vertx);
			free(verty);
			return;
		}

		vertx[0] = X0;
		vertx[1] = X1;
		vertx[2] = X2;
		verty[0] = Y0;
		verty[1] = Y1;
		verty[2] = Y2;

		for (i = XMin; i <= XMax; i++) {
			XTemp1 = XMin;
			XTemp2 = XMin;
			for (j = 0; j < 3; j++) {
				if (vertx[j] >= i && (vertx[j] < XTemp1 || XTemp1 < XMin)) {
					XTemp1 = vertx[j];
					YTemp = verty[j];
				}
				if (vertx[j] <= i && (vertx[j] > XTemp2 || XTemp2 > XMax)) {
					XTemp2 = vertx[j];
				}
			}
			for (j = YTemp; j < YTemp + 63; j++) {
				if (OLED_pnpoly(3, vertx, verty, i, j)) {
					hal_oled_draw_point(i, j);
				}
			}
		}

		free(vertx);
		free(verty);
	}
}

void hal_oled_draw_circle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled)
{
	int16_t i, j;

	if (IsFilled == OLED_UNFILLED) {
		for (i = 0; i <= Radius * 2; i++) {
			for (j = 0; j <= Radius * 2; j++) {
				if ((i - Radius) * (i - Radius) + (j - Radius) * (j - Radius) <= Radius * Radius) {
					hal_oled_draw_point(X + i - Radius, Y + j - Radius);
				}
			}
		}
	} else {
		for (i = 0; i <= Radius * 2; i++) {
			for (j = 0; j <= Radius * 2; j++) {
				if ((i - Radius) * (i - Radius) + (j - Radius) * (j - Radius) <= Radius * Radius) {
					OLED_DisplayBuf[(Y + j - Radius) / 8][X + i - Radius] = 0xFF;
				}
			}
		}
	}
}

void hal_oled_draw_ellipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t i, j;

	if (IsFilled == OLED_UNFILLED) {
		for (i = 0; i <= A * 2; i++) {
			for (j = 0; j <= B * 2; j++) {
				if ((i - A) * (i - A) * B * B + (j - B) * (j - B) * A * A <= A * A * B * B) {
					hal_oled_draw_point(X + i - A, Y + j - B);
				}
			}
		}
	} else {
		for (i = 0; i <= A * 2; i++) {
			for (j = 0; j <= B * 2; j++) {
				if ((i - A) * (i - A) * B * B + (j - B) * (j - B) * A * A <= A * A * B * B) {
					OLED_DisplayBuf[(Y + j - B) / 8][X + i - A] = 0xFF;
				}
			}
		}
	}
}

void hal_oled_draw_arc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t i, j;

	if (StartAngle > EndAngle) {
		StartAngle -= 360;
	}

	if (IsFilled == OLED_UNFILLED) {
		for (i = 0; i <= Radius * 2; i++) {
			for (j = 0; j <= Radius * 2; j++) {
				if ((i - Radius) * (i - Radius) + (j - Radius) * (j - Radius) <= Radius * Radius) {
					if (OLED_IsInAngle(i - Radius, j - Radius, StartAngle, EndAngle)) {
						hal_oled_draw_point(X + i - Radius, Y + j - Radius);
					}
				}
			}
		}
	} else {
		for (i = 0; i <= Radius * 2; i++) {
			for (j = 0; j <= Radius * 2; j++) {
				if ((i - Radius) * (i - Radius) + (j - Radius) * (j - Radius) <= Radius * Radius) {
					if (OLED_IsInAngle(i - Radius, j - Radius, StartAngle, EndAngle)) {
						OLED_DisplayBuf[(Y + j - Radius) / 8][X + i - Radius] = 0xFF;
					}
				}
			}
		}
	}
}

/*********************绘图函数*/
