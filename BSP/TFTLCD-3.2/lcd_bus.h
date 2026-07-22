/**
 ******************************************************************************
 * @file    lcd_bus.h
 * @brief   3.2" TFT (ST7789VW) + W25Qxx 字库 Flash 共享 SPI2 总线层
 *
 * 引脚分配 (STM32F407VGT6):
 *   PB13 = SPI2_SCK    PB14 = SPI2_MISO    PB15 = SPI2_MOSI
 *   PD3  = SPI_RST     PD4 = SPI_DC
 *   PD5  = SPI_CS  (TFT 屏片选)
 *   PD6  = SPI_CS2 (W25Qxx 字库 Flash 片选)
 *   PD7  = BL      (背光)
 *
 * SPI2 由 CubeMX 在 spi.c 中通过 MX_SPI2_Init() 初始化 (hspi2),
 * 本层只提供字节传输 / 批量发送 / 控制脚操作。
 ******************************************************************************
 */
#ifndef __LCD_BUS_H
#define __LCD_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"          /* 拿到 GPIO_Pin / GPIO_Port 宏 (CubeMX 生成于 main.h) */
#include <stdint.h>

/* ===================== 控制脚操作宏 ===================== */
/* RST -> PD3 */
#define SPI_RST_1   HAL_GPIO_WritePin(SPI_RST_GPIO_Port, SPI_RST_Pin, GPIO_PIN_SET)
#define SPI_RST_0   HAL_GPIO_WritePin(SPI_RST_GPIO_Port, SPI_RST_Pin, GPIO_PIN_RESET)

/* DC  -> PD4 */
#define SPI_DC_1    HAL_GPIO_WritePin(SPI_DC_GPIO_Port,  SPI_DC_Pin,  GPIO_PIN_SET)
#define SPI_DC_0    HAL_GPIO_WritePin(SPI_DC_GPIO_Port,  SPI_DC_Pin,  GPIO_PIN_RESET)

/* CS  -> PD5 (TFT 屏片选, 低有效) */
#define SPI_CS_1    HAL_GPIO_WritePin(SPI_CS_GPIO_Port,  SPI_CS_Pin,  GPIO_PIN_SET)
#define SPI_CS_0    HAL_GPIO_WritePin(SPI_CS_GPIO_Port,  SPI_CS_Pin,  GPIO_PIN_RESET)

/* CS2 -> PD6 (字库 Flash 片选, 低有效) */
#define SPI_CS2_1   HAL_GPIO_WritePin(SPI_CS2_GPIO_Port, SPI_CS2_Pin, GPIO_PIN_SET)
#define SPI_CS2_0   HAL_GPIO_WritePin(SPI_CS2_GPIO_Port, SPI_CS2_Pin, GPIO_PIN_RESET)

/* BL  -> PD7 (背光, 高有效) */
#define BL_1        HAL_GPIO_WritePin(BL_GPIO_Port,      BL_Pin,      GPIO_PIN_SET)
#define BL_0        HAL_GPIO_WritePin(BL_GPIO_Port,      BL_Pin,      GPIO_PIN_RESET)

/* ===================== API ===================== */
/**
 * @brief  LCD 控制脚 (RST/DC/CS/CS2/BL) 上电默认态设置.
 *         SPI2 外设本身由 MX_SPI2_Init() 初始化, 此处只把 CS/CS2 拉高置闲.
 */
void     LCD_Bus_Init(void);

/** @brief 全双工收发一个字节 (用于读字库 Flash) */
uint8_t  LCD_Bus_TransferByte(uint8_t data);

/** @brief 只写一个字节到 SPI 总线 */
void     LCD_Bus_WriteByte(uint8_t data);

/** @brief 批量发送 (HAL 阻塞), 内部封装 HAL_SPI_Transmit */
void     LCD_Bus_WriteDMA(const uint8_t *data, uint32_t length);

/** @brief 用同一颜色填充 pixel_count 个像素 (RGB565) */
void     LCD_Bus_FillColor(uint16_t rgb565, uint32_t pixel_count);

#ifdef __cplusplus
}
#endif
#endif /* __LCD_BUS_H */
