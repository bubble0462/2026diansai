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

typedef struct
{
    uint16_t unicode;
    uint8_t gb2312_msb;
    uint8_t gb2312_lsb;
} UTF8_GB2312_MAP_t;

/* Extend this table when a new UTF-8 Chinese character is needed by the UI. */
static const UTF8_GB2312_MAP_t UTF8_GB2312_MAP[] =
{
    {0x4EA4, 0xBD, 0xBB}, {0x4EEA, 0xD2, 0xC7}, {0x4F0F, 0xB7, 0xFC},
    {0x4F4D, 0xCE, 0xBB}, {0x503C, 0xD6, 0xB5}, {0x5146, 0xD5, 0xD7},
    {0x5165, 0xC8, 0xEB}, {0x5179, 0xD7, 0xC8}, {0x51FA, 0xB3, 0xF6},
    {0x529F, 0xB9, 0xA6}, {0x5343, 0xC7, 0xA7}, {0x5355, 0xB5, 0xA5},
    {0x538B, 0xD1, 0xB9}, {0x5668, 0xC6, 0xF7}, {0x56E0, 0xD2, 0xF2},
    {0x5747, 0xBE, 0xF9}, {0x5B89, 0xB0, 0xB2}, {0x5C4F, 0xC6, 0xC1},
    {0x5E45, 0xB7, 0xF9}, {0x5E55, 0xC4, 0xBB}, {0x5E73, 0xC6, 0xBD},
    {0x5EA6, 0xB6, 0xC8}, {0x5FAE, 0xCE, 0xA2}, {0x5F26, 0xCF, 0xD2},
    {0x5F62, 0xD0, 0xCE}, {0x611F, 0xB8, 0xD0}, {0x65B9, 0xB7, 0xBD},
    {0x65E0, 0xCE, 0xDE}, {0x65F6, 0xCA, 0xB1}, {0x6570, 0xCA, 0xFD},
    {0x663E, 0xCF, 0xD4}, {0x6700, 0xD7, 0xEE}, {0x6709, 0xD3, 0xD0},
    {0x671F, 0xC6, 0xDA}, {0x6BEB, 0xBA, 0xC1}, {0x6D41, 0xC1, 0xF7},
    {0x6D4B, 0xB2, 0xE2}, {0x6E29, 0xCE, 0xC2}, {0x6CE2, 0xB2, 0xA8},
    {0x70B9, 0xB5, 0xE3}, {0x7387, 0xC2, 0xCA}, {0x74E6, 0xCD, 0xDF},
    {0x7535, 0xB5, 0xE7}, {0x76F4, 0xD6, 0xB1}, {0x76F8, 0xCF, 0xE0},
    {0x793A, 0xCA, 0xBE}, {0x79D2, 0xC3, 0xEB}, {0x7A0B, 0xB3, 0xCC},
    {0x7A33, 0xCE, 0xC8}, {0x7C7B, 0xC0, 0xE0}, {0x7CBE, 0xBE, 0xAB},
    {0x80FD, 0xC4, 0xDC}, {0x8868, 0xB1, 0xED}, {0x89C6, 0xCA, 0xD3},
    {0x89D2, 0xBD, 0xC7}, {0x8BA1, 0xBC, 0xC6}, {0x8BBE, 0xC9, 0xE8},
    {0x8BD5, 0xCA, 0xD4}, {0x8BEF, 0xCE, 0xF3}, {0x8F93, 0xCA, 0xE4},
    {0x91CF, 0xC1, 0xBF}, {0x963B, 0xD7, 0xE8}, {0x9891, 0xC6, 0xB5},
    {0x989D, 0xB6, 0xEE}, {0xFF08, 0xA3, 0xA8}, {0xFF09, 0xA3, 0xA9},
    {0xFF0C, 0xA3, 0xAC}, {0xFF1A, 0xA3, 0xBA}
};

static uint8_t LCD_IsValidUtf8(const uint8_t *string)
{
    while (*string != 0U)
    {
        if (*string < 0x80U)
        {
            string++;
        }
        else if (((string[0] & 0xE0U) == 0xC0U) &&
                 ((string[1] & 0xC0U) == 0x80U))
        {
            string += 2;
        }
        else if (((string[0] & 0xF0U) == 0xE0U) &&
                 ((string[1] & 0xC0U) == 0x80U) &&
                 ((string[2] & 0xC0U) == 0x80U))
        {
            string += 3;
        }
        else
        {
            return FALSE;
        }
    }
    return TRUE;
}

static uint8_t LCD_UnicodeToGb2312(uint16_t unicode, uint8_t *gb2312)
{
    uint16_t i;

    for (i = 0U; i < (sizeof(UTF8_GB2312_MAP) / sizeof(UTF8_GB2312_MAP[0])); i++)
    {
        if (UTF8_GB2312_MAP[i].unicode == unicode)
        {
            gb2312[0] = UTF8_GB2312_MAP[i].gb2312_msb;
            gb2312[1] = UTF8_GB2312_MAP[i].gb2312_lsb;
            return TRUE;
        }
    }
    return FALSE;
}

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

void DIS_CHINESE_AUTO(unsigned short int x_start, unsigned short int y_start,
                      const char *string)
{
    uint8_t converted[256];
    uint16_t input_pos = 0U;
    uint16_t output_pos = 0U;
    uint16_t unicode;
    const uint8_t *input = (const uint8_t *)string;

    if (string == NULL)
    {
        return;
    }

    if (LCD_IsValidUtf8(input) == FALSE)
    {
        DIS_CHINESE(x_start, y_start, (char *)string);
        return;
    }

    while ((input[input_pos] != 0U) && (output_pos < (sizeof(converted) - 2U)))
    {
        if (input[input_pos] < 0x80U)
        {
            converted[output_pos++] = input[input_pos++];
        }
        else if ((input[input_pos] & 0xE0U) == 0xC0U)
        {
            /* The GB2312 font has no two-byte UTF-8 symbols in this small map. */
            converted[output_pos++] = '?';
            input_pos += 2U;
        }
        else
        {
            unicode = (uint16_t)(((uint16_t)(input[input_pos] & 0x0FU) << 12) |
                                 ((uint16_t)(input[input_pos + 1U] & 0x3FU) << 6) |
                                 (uint16_t)(input[input_pos + 2U] & 0x3FU));
            if (LCD_UnicodeToGb2312(unicode, &converted[output_pos]) != FALSE)
            {
                output_pos += 2U;
            }
            else
            {
                converted[output_pos++] = '?';
            }
            input_pos += 3U;
        }
    }
    converted[output_pos] = '\0';
    DIS_CHINESE(x_start, y_start, (char *)converted);
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
