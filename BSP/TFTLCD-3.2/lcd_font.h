/**
 ******************************************************************************
 * @file    lcd_font.h
 * @brief   W25Qxx 字库 Flash 读取 + GB2312 中英文渲染 + 数字显示
 *
 * 字库 Flash 内部分区 (厂家预烧 GB2312 点阵, 签名 "JYC-4MbByte-FONT-FLASH"):
 *   - ASCII 字模:  6x12 ~ 13x26, 8 档字号
 *   - 汉字字模:   12x12 ~ 26x26, 8 档字号 (按 GB2312 区位码索引)
 ******************************************************************************
 */
#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===================== 颜色索引 (与 lcd.h 一致) ===================== */
#ifndef BLACK
#define BLACK   0
#define RED     1
#define GREEN   2
#define BLUE    3
#define WHITE   4
#endif

/* ===================== W25Qxx SPI Flash 命令集 ===================== */
#define W25X_WriteEnable        0x06
#define W25X_WriteDisable       0x04
#define W25X_ReadStatusReg      0x05
#define W25X_WriteStatusReg     0x01
#define W25X_ReadData           0x03
#define W25X_FastReadData       0x0B
#define W25X_FastReadDual       0x3B
#define W25X_PageProgram        0x02
#define W25X_BlockErase         0xD8
#define W25X_SectorErase        0x20
#define W25X_ChipErase          0xC7
#define W25X_PowerDown          0xB9
#define W25X_ReleasePowerDown   0xAB
#define W25X_DeviceID           0xAB
#define W25X_ManufactDeviceID   0x90
#define W25X_JedecDeviceID      0x9F

/* ===================== 字库 Flash 地址表 (ASCII) ===================== */
#define CHAR6_12_ADD    0x1000
#define CHAR7_14_ADD    0x1600
#define CHAR8_16_ADD    0x1D00
#define CHAR9_18_ADD    0x2500
#define CHAR10_20_ADD   0x3700
#define CHAR11_22_ADD   0x4B00
#define CHAR12_24_ADD   0x6100
#define CHAR13_26_ADD   0x7900

/* ===================== 字库 Flash 地址表 (汉字 GB2312) ===================== */
#define CHINA12_12_ADD  0x9300
#define CHINA14_14_ADD  0x39300
#define CHINA16_16_ADD  0x71300
#define CHINA18_18_ADD  0xB1300
#define CHINA20_20_ADD  0x11D300
#define CHINA22_22_ADD  0x195300
#define CHINA24_24_ADD  0x219300
#define CHINA26_26_ADD  0x2A9300
#define END_ADD         0x379300

#define TRUE    1
#define FALSE   0

/* ===================== 字号枚举 ===================== */
typedef enum
{
    SONG_STYLE12,
    SONG_STYLE14,
    SONG_STYLE16,
    SONG_STYLE18,
    SONG_STYLE20,
    SONG_STYLE22,
    SONG_STYLE24,
    SONG_STYLE26
} type_of_font;

/* ===================== 字符显示模式 (全局, 由 SET_FONT_STYLE 填充) ===================== */
struct DIS_CHAR_MODE_t
{
    unsigned char  CHAR_WIDE;           /* ASCII 字符像素宽 */
    unsigned char  CHAR_HIGH;           /* ASCII 字符像素高 */
    unsigned char  WORD_WIDE;           /* 汉字像素宽 */
    unsigned char  WORD_HIGH;           /* 汉字像素高 */
    unsigned short int CHAR_DATA_SIZE;  /* ASCII 单字数据字节数 */
    unsigned short int WORD_DATA_SIZE;  /* 汉字单字数据字节数 */
    unsigned short int BACK_COLOR;      /* 背景色索引 */
    unsigned short int FONT_COLOR;      /* 前景色索引 */
    unsigned int   BASE_WORD_ADD;       /* 汉字字模基址 */
    unsigned int   BASE_CHAR_ADD;       /* ASCII 字模基址 */
};
extern struct DIS_CHAR_MODE_t DIS_CHAR_MODE;

/* RGB565 调色板, 索引 0~4 对应 BLACK/RED/GREEN/BLUE/WHITE, [0]=高字节 [1]=低字节 */
extern const unsigned char TAB_COLOR[][2];

/* 字模读取缓冲 (最大 26*26 = 4*26 = 104 字节) */
extern unsigned char FONT_BUFFER[104];

/* ===================== API ===================== */

/**
 * @brief 从字库 Flash 读 NumByteToRead 字节
 * @param pBuffer    接收缓冲
 * @param ReadAddr   24 位 Flash 内地址
 * @param NumByteToRead 读取字节数
 */
void W25QXX_Read(unsigned char *pBuffer, unsigned int ReadAddr,
                 unsigned short int NumByteToRead);

/** @brief 字库 Flash 签名校验, 通过返回 TRUE, 失败返回 FALSE */
unsigned char CHECK_FALSH(void);

/**
 * @brief 设置字号 / 前景色 / 背景色
 * @param font_color  前景色索引 (BLACK~WHITE)
 * @param back_color  背景色索引
 * @param TYPE_CHAR   字号 (SONG_STYLE12~26)
 */
void SET_FONT_STYLE(unsigned char font_color, unsigned char back_color,
                    type_of_font TYPE_CHAR);

/**
 * @brief 在 (x_start, y_start) 显示中英文混合字符串
 * @note  中文必须用 GB2312 编码 (双字节, MSB/LSB >= 0xA1)
 *        英文为单字节 ASCII. 自动换行处理.
 */
void DIS_CHINESE(unsigned short int x_start, unsigned short int y_start,
                 char *string);

/**
 * @brief Display an ASCII/Chinese string using automatic source encoding detection.
 * @note  Accepts UTF-8 or GB2312. The built-in UTF-8 mapping covers common
 *        instrument and waveform UI terms; unsupported characters show as '?'.
 */
void DIS_CHINESE_AUTO(unsigned short int x_start, unsigned short int y_start,
                      const char *string);

/** @brief 在指定位置显示浮点数 (含清背景, 避免残影) */
void LCD_DisplayFloat(unsigned short int x, unsigned short int y,
                      double num, int len, int precision, type_of_font font);

/** @brief 在指定位置显示整数 (含清背景) */
void LCD_DisplayNumber(unsigned short int x, unsigned short int y,
                       double num, int len, type_of_font font);

#ifdef __cplusplus
}
#endif
#endif /* __LCD_FONT_H */
