/**
 ******************************************************************************
 * @file    lcd.h
 * @brief   3.2" TFT ST7789VW 驱动层 (240x320, RGB565)
 *          硬件 SPI2, 4 线串口接口 (SCK/MISO/MOSI/DC/CS/RST)
 ******************************************************************************
 */
#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===================== 屏幕几何参数 ===================== */
#define TFT_COLUMN_NUMBER   240     /* 物理像素宽 (x 方向) */
#define TFT_LINE_NUMBER     320     /* 物理像素高 (y 方向) */
#define TFT_COLUMN_OFFSET   0
#define TFT_LINE_OFFSET     0

/* ===================== 颜色索引 (配合 TAB_COLOR 调色板) ===================== */
#define BLACK   0
#define RED     1
#define GREEN   2
#define BLUE    3
#define WHITE   4

/* ===================== API ===================== */
/**
 * @brief  ST7789VW 初始化命令序列 (Sleep Out / Porch / VCOM / Gamma / Display On).
 *         调用前需先 LCD_Bus_Init() + 复位 (本函数内部已做硬复位).
 */
void TFT_init(void);

/** @brief 发送命令字节到 TFT (会自动管理 CS) */
void TFT_SEND_CMD(unsigned char cmd);

/** @brief 发送数据字节到 TFT (会自动管理 CS) */
void TFT_SEND_DATA(unsigned char data);

/** @brief 设置 GRAM 显示窗口, 后续像素写入范围限制在 (x1,y1)-(x2,y2) */
void TFT_SET_ADD(unsigned short x_start, unsigned short y_start,
                 unsigned short x_end,   unsigned short y_end);

/** @brief 清屏为白色 */
void TFT_clear(void);

/** @brief 全屏填充指定颜色索引 */
void TFT_full(unsigned int color_index);

/** @brief 在矩形区域填充指定颜色索引 */
void TFT_Fill_Rectangle(unsigned short x_start, unsigned short y_start,
                        unsigned short x_end,   unsigned short y_end,
                        unsigned int color_index);

/** @brief 画单个像素点 (x,y, 颜色索引). 性能一般, 用于少量描点. */
void LCD_DrawPoint(unsigned short x, unsigned short y, unsigned int color_index);

#ifdef __cplusplus
}
#endif
#endif /* __LCD_H */
