/**
 ******************************************************************************
 * @file    lcd.c
 * @brief   ST7789VW 驱动层实现 (从 diansai-basic/main.c 抽离)
 ******************************************************************************
 */
#include "lcd.h"
#include "lcd_bus.h"
#include "lcd_font.h"       /* TAB_COLOR 调色板 */
#include "stm32f4xx_hal.h"

/* ===================== 内部 CS 缓存管理 =====================
 * 原工程用 CS 状态缓存避免每条命令都拉高/拉低 CS, 提升批量传输效率.
 * 一段连续的命令/数据序列中, 只有首末才真正操作 CS.
 */
static uint8_t tft_cs_is_low = 0;   /* 0: CS 高 (未选中), 1: CS 低 (选中) */

static void TFT_CS_Enable(void)
{
    if (!tft_cs_is_low)
    {
        SPI_CS_0;
        tft_cs_is_low = 1;
    }
}

static void TFT_CS_Disable(void)
{
    if (tft_cs_is_low)
    {
        SPI_CS_1;
        tft_cs_is_low = 0;
    }
}

/* ===================== 内部无 CS 操作的命令/数据发送 ===================== */
static void _TFT_SEND_CMD_NO_CS(unsigned char cmd)
{
    SPI_DC_0;                       /* DC=0: 命令 */
    LCD_Bus_WriteByte(cmd);
}

static void _TFT_SEND_DATA_NO_CS(unsigned char data)
{
    SPI_DC_1;                       /* DC=1: 数据 */
    LCD_Bus_WriteByte(data);
}

/* ===================== 对外 API ===================== */
void TFT_SEND_CMD(unsigned char cmd)
{
    TFT_CS_Enable();
    _TFT_SEND_CMD_NO_CS(cmd);
    TFT_CS_Disable();
}

void TFT_SEND_DATA(unsigned char data)
{
    TFT_CS_Enable();
    _TFT_SEND_DATA_NO_CS(data);
    TFT_CS_Disable();
}

void TFT_SET_ADD(unsigned short x_start, unsigned short y_start,
                 unsigned short x_end,   unsigned short y_end)
{
    unsigned short sx = x_start + TFT_COLUMN_OFFSET;
    unsigned short ex = x_end   + TFT_COLUMN_OFFSET;
    unsigned short sy = y_start + TFT_LINE_OFFSET;
    unsigned short ey = y_end   + TFT_LINE_OFFSET;

    TFT_CS_Enable();                /* 进入连续命令序列, 不立即释放 CS */

    _TFT_SEND_CMD_NO_CS(0x2A);      /* Column Address Set */
    _TFT_SEND_DATA_NO_CS((unsigned char)(sx >> 8));
    _TFT_SEND_DATA_NO_CS((unsigned char)(sx & 0xFF));
    _TFT_SEND_DATA_NO_CS((unsigned char)(ex >> 8));
    _TFT_SEND_DATA_NO_CS((unsigned char)(ex & 0xFF));

    _TFT_SEND_CMD_NO_CS(0x2B);      /* Row Address Set */
    _TFT_SEND_DATA_NO_CS((unsigned char)(sy >> 8));
    _TFT_SEND_DATA_NO_CS((unsigned char)(sy & 0xFF));
    _TFT_SEND_DATA_NO_CS((unsigned char)(ey >> 8));
    _TFT_SEND_DATA_NO_CS((unsigned char)(ey & 0xFF));

    _TFT_SEND_CMD_NO_CS(0x2C);      /* Memory Write */
    /* CS 保持低, 等调用方发完像素后由 TFT_CS_Disable() 释放 */
}

void TFT_clear(void)
{
    unsigned int total_points = (unsigned int)TFT_COLUMN_NUMBER * TFT_LINE_NUMBER;
    TFT_SET_ADD(0, 0, TFT_COLUMN_NUMBER - 1, TFT_LINE_NUMBER - 1);
    SPI_DC_1;                       /* 后续是 GRAM 像素数据 */
    LCD_Bus_FillColor(0xFFFFU, total_points);    /* 白色 */
    TFT_CS_Disable();
}

void TFT_full(unsigned int color_index)
{
    unsigned int  total_points = (unsigned int)TFT_COLUMN_NUMBER * TFT_LINE_NUMBER;
    uint16_t color = ((uint16_t)TAB_COLOR[color_index][0] << 8) |
                      TAB_COLOR[color_index][1];
    TFT_SET_ADD(0, 0, TFT_COLUMN_NUMBER - 1, TFT_LINE_NUMBER - 1);
    SPI_DC_1;
    LCD_Bus_FillColor(color, total_points);
    TFT_CS_Disable();
}

void TFT_Fill_Rectangle(unsigned short x_start, unsigned short y_start,
                        unsigned short x_end,   unsigned short y_end,
                        unsigned int color_index)
{
    unsigned long total_points;
    uint16_t color;

    if (x_start > x_end || y_start > y_end) return;

    total_points = (unsigned long)(x_end - x_start + 1) * (y_end - y_start + 1);
    color = ((uint16_t)TAB_COLOR[color_index][0] << 8) |
             TAB_COLOR[color_index][1];

    TFT_SET_ADD(x_start, y_start, x_end, y_end);
    SPI_DC_1;
    LCD_Bus_FillColor(color, total_points);
    TFT_CS_Disable();
}

void LCD_DrawPoint(unsigned short x, unsigned short y, unsigned int color_index)
{
    uint16_t color = ((uint16_t)TAB_COLOR[color_index][0] << 8) |
                      TAB_COLOR[color_index][1];
    uint8_t  buf[2];

    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER) return;

    buf[0] = (uint8_t)(color >> 8);
    buf[1] = (uint8_t)(color & 0xFF);

    TFT_SET_ADD(x, y, x, y);
    SPI_DC_1;
    LCD_Bus_WriteDMA(buf, 2U);
    TFT_CS_Disable();
}

/* ===================== ST7789VW 初始化序列 (一字未改, 已实测可用) ===================== */
void TFT_init(void)
{
    /* 硬复位 */
    SPI_RST_0;
    HAL_Delay(100);
    SPI_RST_1;
    HAL_Delay(100);

    TFT_CS_Enable();            /* 整段初始化序列共用一次 CS */

    _TFT_SEND_CMD_NO_CS(0x11);  /* Sleep Out */
    HAL_Delay(120);

    _TFT_SEND_CMD_NO_CS(0x36);  /* MADCTL: 显存方向 */
    _TFT_SEND_DATA_NO_CS(0x00);

    _TFT_SEND_CMD_NO_CS(0x3A);  /* COLMOD: RGB565 16-bit */
    _TFT_SEND_DATA_NO_CS(0x55);

    _TFT_SEND_CMD_NO_CS(0xb2);  /* Porctrl: Porch setting (ST7789 独有) */
    _TFT_SEND_DATA_NO_CS(0x0c);
    _TFT_SEND_DATA_NO_CS(0x0c);
    _TFT_SEND_DATA_NO_CS(0x00);
    _TFT_SEND_DATA_NO_CS(0x33);
    _TFT_SEND_DATA_NO_CS(0x33);

    _TFT_SEND_CMD_NO_CS(0xb7);  /* GCTRL: Gate ctrl */
    _TFT_SEND_DATA_NO_CS(0x35);

    _TFT_SEND_CMD_NO_CS(0xbb);  /* VCOM setting */
    _TFT_SEND_DATA_NO_CS(0x1F);

    _TFT_SEND_CMD_NO_CS(0xc0);  /* LCM ctrl */
    _TFT_SEND_DATA_NO_CS(0x2c);

    _TFT_SEND_CMD_NO_CS(0xc2);  /* VDV/VRH cmd enable */
    _TFT_SEND_DATA_NO_CS(0x01);

    _TFT_SEND_CMD_NO_CS(0xc3);  /* VRH set */
    _TFT_SEND_DATA_NO_CS(0x12);

    _TFT_SEND_CMD_NO_CS(0xc4);  /* VDV set */
    _TFT_SEND_DATA_NO_CS(0x20);

    _TFT_SEND_CMD_NO_CS(0xc6);  /* Frame rate ctrl */
    _TFT_SEND_DATA_NO_CS(0x0f);

    _TFT_SEND_CMD_NO_CS(0xd0);  /* Power ctrl 1 */
    _TFT_SEND_DATA_NO_CS(0xa4);
    _TFT_SEND_DATA_NO_CS(0xa1);

    _TFT_SEND_CMD_NO_CS(0xe0);  /* Positive Gamma (14 参数) */
    _TFT_SEND_DATA_NO_CS(0xd0); _TFT_SEND_DATA_NO_CS(0x0d);
    _TFT_SEND_DATA_NO_CS(0x14); _TFT_SEND_DATA_NO_CS(0x0b);
    _TFT_SEND_DATA_NO_CS(0x0b); _TFT_SEND_DATA_NO_CS(0x07);
    _TFT_SEND_DATA_NO_CS(0x3a); _TFT_SEND_DATA_NO_CS(0x44);
    _TFT_SEND_DATA_NO_CS(0x50); _TFT_SEND_DATA_NO_CS(0x08);
    _TFT_SEND_DATA_NO_CS(0x13); _TFT_SEND_DATA_NO_CS(0x13);
    _TFT_SEND_DATA_NO_CS(0x2d); _TFT_SEND_DATA_NO_CS(0x32);

    _TFT_SEND_CMD_NO_CS(0xe1);  /* Negative Gamma (14 参数) */
    _TFT_SEND_DATA_NO_CS(0xd0); _TFT_SEND_DATA_NO_CS(0x0d);
    _TFT_SEND_DATA_NO_CS(0x14); _TFT_SEND_DATA_NO_CS(0x0b);
    _TFT_SEND_DATA_NO_CS(0x0b); _TFT_SEND_DATA_NO_CS(0x07);
    _TFT_SEND_DATA_NO_CS(0x3a); _TFT_SEND_DATA_NO_CS(0x44);
    _TFT_SEND_DATA_NO_CS(0x50); _TFT_SEND_DATA_NO_CS(0x08);
    _TFT_SEND_DATA_NO_CS(0x13); _TFT_SEND_DATA_NO_CS(0x13);
    _TFT_SEND_DATA_NO_CS(0x2d); _TFT_SEND_DATA_NO_CS(0x32);

    _TFT_SEND_CMD_NO_CS(0x20);  /* Display inversion off */
    _TFT_SEND_CMD_NO_CS(0x29);  /* Display on */
    HAL_Delay(20);

    TFT_CS_Disable();
}
