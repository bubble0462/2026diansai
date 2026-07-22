/**
 ******************************************************************************
 * @file    lcd_font.c
 * @brief   W25Qxx 字库 Flash 读取 + GB2312 中英文渲染 + 数字显示实现
 *          (从 diansai-basic/main.c 抽离, 原样保留渲染算法)
 ******************************************************************************
 */
#include "lcd_font.h"
#include "lcd.h"
#include "lcd_bus.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* ===================== 全局变量定义 ===================== */
struct DIS_CHAR_MODE_t DIS_CHAR_MODE;

/* RGB565 调色板: BLACK / RED / GREEN / BLUE / WHITE */
const unsigned char TAB_COLOR[][2] =
{
    {0x00, 0x00},    /* BLACK  0x0000 */
    {0xF8, 0x00},    /* RED    0xF800 */
    {0x07, 0xE0},    /* GREEN  0x07E0 */
    {0x00, 0x1F},    /* BLUE   0x001F */
    {0xFF, 0xFF},    /* WHITE  0xFFFF */
};

unsigned char FONT_BUFFER[104];        /* 26*26 最大 = 4*26 = 104 字节 */

/* ===================== W25Qxx 字库 Flash 读取 ===================== */
void W25QXX_Read(unsigned char *pBuffer, unsigned int ReadAddr,
                 unsigned short int NumByteToRead)
{
    unsigned short int i;
    SPI_CS2_0;                                   /* 选中字库 Flash */
    LCD_Bus_WriteByte(W25X_ReadData);            /* 0x03 标准读取命令 */
    LCD_Bus_WriteByte((unsigned char)(ReadAddr >> 16));   /* 24bit 地址 高 */
    LCD_Bus_WriteByte((unsigned char)(ReadAddr >> 8));    /*              中 */
    LCD_Bus_WriteByte((unsigned char)(ReadAddr));         /*              低 */
    for (i = 0; i < NumByteToRead; i++)
    {
        pBuffer[i] = LCD_Bus_TransferByte(0xFFU);        /* 主机发 0xFF 读 MISO */
    }
    SPI_CS2_1;
}

unsigned char CHECK_FALSH(void)
{
    unsigned int x;
    const unsigned char string[] = "JYC-4MbByte-FONT-FLASH";
    W25QXX_Read(FONT_BUFFER, 0x000000, 22);
    for (x = 0; x < 22; x++)
    {
        if (FONT_BUFFER[x] != string[x])
        {
            return (FALSE);
        }
    }
    return (TRUE);
}

/* ===================== 字号设置 ===================== */
void SET_FONT_STYLE(unsigned char font_color, unsigned char back_color,
                    type_of_font TYPE_CHAR)
{
    DIS_CHAR_MODE.BACK_COLOR = back_color;
    DIS_CHAR_MODE.FONT_COLOR = font_color;
    switch (TYPE_CHAR)
    {
    case SONG_STYLE12:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR6_12_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA12_12_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 12;
        DIS_CHAR_MODE.CHAR_HIGH = 12;
        DIS_CHAR_MODE.CHAR_WIDE = 6;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 12;
        DIS_CHAR_MODE.WORD_HIGH = 12;
        DIS_CHAR_MODE.WORD_WIDE = 12;
        break;

    case SONG_STYLE14:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR7_14_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA14_14_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 14;
        DIS_CHAR_MODE.CHAR_HIGH = 14;
        DIS_CHAR_MODE.CHAR_WIDE = 7;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 14;
        DIS_CHAR_MODE.WORD_HIGH = 14;
        DIS_CHAR_MODE.WORD_WIDE = 14;
        break;

    case SONG_STYLE16:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR8_16_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA16_16_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 16;
        DIS_CHAR_MODE.CHAR_HIGH = 16;
        DIS_CHAR_MODE.CHAR_WIDE = 8;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 16;
        DIS_CHAR_MODE.WORD_HIGH = 16;
        DIS_CHAR_MODE.WORD_WIDE = 16;
        break;

    case SONG_STYLE18:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR9_18_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA18_18_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 18;
        DIS_CHAR_MODE.CHAR_HIGH = 18;
        DIS_CHAR_MODE.CHAR_WIDE = 9;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 18;
        DIS_CHAR_MODE.WORD_HIGH = 18;
        DIS_CHAR_MODE.WORD_WIDE = 18;
        break;

    case SONG_STYLE20:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR10_20_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA20_20_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 20;
        DIS_CHAR_MODE.CHAR_HIGH = 20;
        DIS_CHAR_MODE.CHAR_WIDE = 10;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 20;
        DIS_CHAR_MODE.WORD_HIGH = 20;
        DIS_CHAR_MODE.WORD_WIDE = 20;
        break;

    case SONG_STYLE22:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR11_22_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA22_22_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 22;
        DIS_CHAR_MODE.CHAR_HIGH = 22;
        DIS_CHAR_MODE.CHAR_WIDE = 11;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 22;
        DIS_CHAR_MODE.WORD_HIGH = 22;
        DIS_CHAR_MODE.WORD_WIDE = 22;
        break;

    case SONG_STYLE24:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR12_24_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA24_24_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 24;
        DIS_CHAR_MODE.CHAR_HIGH = 24;
        DIS_CHAR_MODE.CHAR_WIDE = 12;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 24;
        DIS_CHAR_MODE.WORD_HIGH = 24;
        DIS_CHAR_MODE.WORD_WIDE = 24;
        break;

    case SONG_STYLE26:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR13_26_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA26_26_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 26;
        DIS_CHAR_MODE.CHAR_HIGH = 26;
        DIS_CHAR_MODE.CHAR_WIDE = 13;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 4 * 26;
        DIS_CHAR_MODE.WORD_HIGH = 26;
        DIS_CHAR_MODE.WORD_WIDE = 26;
        break;
    }
}

/* ===================== 中英文渲染核心 ===================== */
void DIS_CHINESE(unsigned short int x_start, unsigned short int y_start, char *string)
{
    unsigned char times = 0, CACHE = 0;
    unsigned int Address, x = 0, z = 0, m, n, f;
    unsigned char WORD_CODE_MSB, WORD_CODE_LSB;
    unsigned int ADD_X_START = x_start;
    unsigned int ADD_Y_START = y_start;
    unsigned int ADD_X_END   = x_start + DIS_CHAR_MODE.WORD_WIDE - 1;
    unsigned int ADD_Y_END   = y_start + DIS_CHAR_MODE.WORD_HIGH;

    while (*string != '\0')
    {
        WORD_CODE_MSB = *string++;
        WORD_CODE_LSB = *string++;

        if (((unsigned char)WORD_CODE_MSB >= 0xA1) && ((unsigned char)WORD_CODE_LSB >= 0xA1))
        {
            /* ============= 汉字 (GB2312 双字节) ============= */
            Address = (WORD_CODE_MSB - 0xA1) * 94;
            Address = (Address + (WORD_CODE_LSB - 0xA1));
            Address = Address * (DIS_CHAR_MODE.WORD_DATA_SIZE);
            Address = Address + DIS_CHAR_MODE.BASE_WORD_ADD;
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.WORD_DATA_SIZE));
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.WORD_WIDE - 1;

            /* 自动换行 */
            if (ADD_X_END > (TFT_LINE_NUMBER - 1))
            {
                ADD_Y_START = ADD_Y_START + (ADD_X_END / (TFT_LINE_NUMBER - 1)) * DIS_CHAR_MODE.WORD_HIGH;
                ADD_X_START = 0;
                ADD_X_END = DIS_CHAR_MODE.WORD_WIDE - 1;
                ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.WORD_HIGH;
                if (ADD_Y_END > TFT_COLUMN_NUMBER)
                {
                    ADD_Y_START = 0;
                    ADD_Y_END = DIS_CHAR_MODE.WORD_HIGH;
                }
            }
            TFT_SET_ADD(ADD_X_START, ADD_Y_START, ADD_X_END, ADD_Y_END);
            TFT_SEND_CMD(0x2C);     /* Memory Write */

            z = 0;
            for (x = 0; x < DIS_CHAR_MODE.WORD_HIGH; x++)
            {
                m = DIS_CHAR_MODE.WORD_WIDE / 8;
                f = DIS_CHAR_MODE.WORD_WIDE % 8;
                for (n = 0; n < m; n++)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < 8; times++)
                    {
                        if ((CACHE & 0x80) == 0)
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
                if (f != 0)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < f; times++)
                    {
                        if ((CACHE & 0x80) == 0)
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
            }
            ADD_X_START = ADD_X_END;
        }
        else
        {
            /* ============= ASCII (单字节, 退回 LSB) ============= */
            Address = (WORD_CODE_MSB) * DIS_CHAR_MODE.CHAR_DATA_SIZE + DIS_CHAR_MODE.BASE_CHAR_ADD;
            string--;
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.CHAR_WIDE - 1;
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.CHAR_DATA_SIZE));

            if (ADD_X_END > (TFT_COLUMN_NUMBER - 1))
            {
                ADD_Y_START = ADD_Y_START + (ADD_X_END / (TFT_COLUMN_NUMBER - 1)) * DIS_CHAR_MODE.CHAR_HIGH;
                ADD_X_START = 0;
                ADD_X_END = DIS_CHAR_MODE.CHAR_WIDE - 1;
                ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.CHAR_HIGH;
                if (ADD_Y_END > (TFT_LINE_NUMBER - 1))
                {
                    ADD_Y_START = 0;
                    ADD_Y_END = DIS_CHAR_MODE.CHAR_HIGH;
                }
            }
            TFT_SET_ADD(ADD_X_START, ADD_Y_START, ADD_X_END, ADD_Y_END);

            z = 0;
            for (x = 0; x < DIS_CHAR_MODE.CHAR_HIGH; x++)
            {
                m = DIS_CHAR_MODE.CHAR_WIDE / 8;
                f = DIS_CHAR_MODE.CHAR_WIDE % 8;
                for (n = 0; n < m; n++)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < 8; times++)
                    {
                        if ((CACHE & 0x80) == 0)
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
                if (f != 0)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < f; times++)
                    {
                        if ((CACHE & 0x80) == 0)
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
            }
            ADD_X_START = ADD_X_END;
        }
    }
}

/* ===================== 数字显示 ===================== */
void LCD_DisplayFloat(unsigned short int x, unsigned short int y,
                      double num, int len, int precision, type_of_font font)
{
    char buffer[15];
    char format[10];
    char clear_buffer[15];
    unsigned char original_font_color = DIS_CHAR_MODE.FONT_COLOR;
    unsigned char original_back_color = DIS_CHAR_MODE.BACK_COLOR;

    if (len > 14) len = 14;

    for (int i = 0; i < len; i++)
    {
        clear_buffer[i] = ' ';
    }
    clear_buffer[len] = '\0';

    /* 先用背景色刷一遍清除残影 */
    SET_FONT_STYLE(original_back_color, original_back_color, font);
    DIS_CHINESE(x, y, clear_buffer);

    /* 再用前景色写新值 */
    SET_FONT_STYLE(original_font_color, original_back_color, font);
    sprintf(format, "%%.%df", precision);
    sprintf(buffer, format, num);
    DIS_CHINESE(x, y, buffer);
}

void LCD_DisplayNumber(unsigned short int x, unsigned short int y,
                       double num, int len, type_of_font font)
{
    char buffer[15];
    char clear_buffer[15];
    unsigned char original_font_color = DIS_CHAR_MODE.FONT_COLOR;
    unsigned char original_back_color = DIS_CHAR_MODE.BACK_COLOR;

    if (len > 14) len = 14;

    for (int i = 0; i < len; i++)
    {
        clear_buffer[i] = ' ';
    }
    clear_buffer[len] = '\0';

    SET_FONT_STYLE(original_back_color, original_back_color, font);
    DIS_CHINESE(x, y, clear_buffer);

    SET_FONT_STYLE(original_font_color, original_back_color, font);
    sprintf(buffer, "%.0f", num);
    DIS_CHINESE(x, y, buffer);
}
