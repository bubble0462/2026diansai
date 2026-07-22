/**
 ******************************************************************************
 * @file    lcd_bus.c
 * @brief   3.2" TFT SPI2 总线层 (HAL 实现)
 ******************************************************************************
 */
#include "lcd_bus.h"
#include "spi.h"            /* MX_SPI2_Init() 生成的 hspi2 */
#include "stm32f4xx_hal.h"

/* 颜色填充时的本地块缓冲, 避免逐字节调用 HAL */
#define LCD_COLOR_BLOCK_BYTES  1024U
static uint8_t s_color_buffer[LCD_COLOR_BLOCK_BYTES];

void LCD_Bus_Init(void)
{
    /* SPI2 已由 MX_SPI2_Init() 在 main() 中初始化完成 (hspi2).
       这里只设置所有从机的片选为非激活 (高), 防止总线毛刺. */
    SPI_CS_1;       /* TFT 屏不选中 */
    SPI_CS2_1;      /* 字库 Flash 不选中 */
    SPI_DC_1;       /* DC 默认数据态 */
    BL_1;           /* 默认开背光 */
}

uint8_t LCD_Bus_TransferByte(uint8_t data)
{
    uint8_t rx = 0xFFU;
    /* 全双工: 同时发 1 字节收 1 字节. 字库 Flash 读取依赖 MISO 回传. */
    (void)HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1U, HAL_MAX_DELAY);
    return rx;
}

void LCD_Bus_WriteByte(uint8_t data)
{
    (void)LCD_Bus_TransferByte(data);
}

void LCD_Bus_WriteDMA(const uint8_t *data, uint32_t length)
{
    /* 原工程用 DMA2_Stream3 做 TX-only, 但全双工下会触发 OVR.
       这里改用 HAL 阻塞批量发送 (Mode0, 10.5MHz), 240x320 屏刷新足够.
       如需更高吞吐, 可改为 HAL_SPI_Transmit_DMA + RX 端开 DMA sink. */
    if ((data != NULL) && (length != 0U))
    {
        (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)data, length, HAL_MAX_DELAY);
    }
}

void LCD_Bus_FillColor(uint16_t rgb565, uint32_t pixel_count)
{
    uint32_t i;
    uint8_t  high = (uint8_t)(rgb565 >> 8);
    uint8_t  low  = (uint8_t)(rgb565 & 0xFFU);

    /* 预填一块 RGB565 (高字节在前, 与 ST7789 数据格式一致) */
    for (i = 0U; i < sizeof(s_color_buffer); i += 2U)
    {
        s_color_buffer[i]      = high;
        s_color_buffer[i + 1U] = low;
    }

    while (pixel_count != 0U)
    {
        uint32_t block_pixels = (pixel_count > (sizeof(s_color_buffer) / 2U)) ?
                                (sizeof(s_color_buffer) / 2U) : pixel_count;
        LCD_Bus_WriteDMA(s_color_buffer, block_pixels * 2U);
        pixel_count -= block_pixels;
    }
}
