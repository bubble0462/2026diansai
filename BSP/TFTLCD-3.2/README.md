# TFTLCD-3.2 BSP — 3.2 寸 TFT (ST7789VW) + W25Qxx 字库驱动

## 硬件信息

- **屏幕**：3.2 寸 TFT, 240×320, RGB565, 驱动 IC = ST7789VW
- **字库**：W25Qxx SPI Flash (4MB, 厂家预烧 GB2312 字库, 签名 `"JYC-4MbByte-FONT-FLASH"`)
- **接口**：10 针 4 线 SPI（SCK/MISO/MOSI + DC + CS + CS2 + RST + BL）

## 引脚分配（STM32F407VGT6）

| 屏幕引脚 | STM32 | CubeMX 标签 | 说明 |
|---|---|---|---|
| SCL/SCK | PA5 | SPI1_SCK | SPI1 时钟 |
| SDO/MISO | PA6 | SPI1_MISO | SPI1 主机输入（读字库用） |
| SDI/MOSI | PA7 | SPI1_MOSI | SPI1 主机输出 |
| RST | PD3 | SPI_RST | 屏幕硬复位（低有效） |
| DC | PD4 | SPI_DC | 数据/命令（0=命令, 1=数据） |
| CS1 | PD5 | SPI_CS | **TFT 屏片选**（低有效） |
| CS2 | PD6 | SPI_CS2 | **字库 Flash 片选**（低有效） |
| BL | PD7 | BL | 背光（高=开） |
| VCC | 3.3V | — | — |
| GND | GND | — | — |

## SPI 配置（CubeMX 在 spi.c 中由 MX_SPI1_Init() 完成）

- Master, Full-Duplex, 8-bit, MSB First
- **CPOL=Low, CPHA=1 Edge（SPI Mode 0）**
- NSS: Software
- BaudRate: APB2(84MHz) / 8 = **10.5 MHz**（ST7789VW 写时钟上限 15.4MHz，安全）

## 分层架构

```
┌─────────────────────────────────────────────┐
│  应用层 (main.c)                            │
│  TFT_init / TFT_full / DIS_CHINESE 等       │
├─────────────────────────────────────────────┤
│  lcd_font.c  字库 Flash + GB2312 渲染       │
│  W25QXX_Read / SET_FONT_STYLE / DIS_CHINESE │
├─────────────────────────────────────────────┤
│  lcd.c       ST7789 驱动 (命令/GRAM/绘图)    │
│  TFT_init / TFT_SET_ADD / TFT_Fill_Rectangle│
├─────────────────────────────────────────────┤
│  lcd_bus.c   SPI1 总线 (HAL 封装)           │
│  LCD_Bus_WriteByte / LCD_Bus_FillColor      │
│  + 控制脚宏 (RST/DC/CS/CS2/BL)              │
├─────────────────────────────────────────────┤
│  spi.c (CubeMX 生成)  MX_SPI1_Init / hspi1 │
└─────────────────────────────────────────────┘
```

## 快速使用

```c
#include "lcd.h"
#include "lcd_font.h"

/* main() 中, MX_SPI1_Init() 之后 */
LCD_Bus_Init();          // 设置 CS/CS2/DC/BL 默认态
TFT_init();              // ST7789VW 初始化序列
BL_1;                    // 开背光

if (CHECK_FALSH())       // 字库自检
{
    TFT_full(BLACK);     // 全屏黑底

    SET_FONT_STYLE(WHITE, BLACK, SONG_STYLE24);
    DIS_CHINESE(10, 10, "\xC6\xC1\xC4\xBB\xB2\xE2\xCA\xD4");  /* "屏幕测试" */

    SET_FONT_STYLE(GREEN, BLACK, SONG_STYLE20);
    LCD_DisplayFloat(10, 60, 3.14159, 8, 4, SONG_STYLE20);     /* π 值 */
}
```

## 中文文本编码说明

字库 Flash 使用 **GB2312** 编码。Keil 编辑器源码文件默认 GB2312 时可直接写中文字面量；
为防止编辑器编码错乱，建议用 `\xXX` 转义形式写中文，例如：

```c
/* "屏幕测试" 的 GB2312 字节 */
"\xC6\xC1\xC4\xBB\xB2\xE2\xCA\xD4"
```

## 文件清单

| 文件 | 内容 |
|---|---|
| `lcd_bus.h/.c` | SPI1 总线层 + 控制脚宏（RST/DC/CS/CS2/BL）|
| `lcd.h/.c` | ST7789VW 驱动（init/clear/full/矩形/画点/设窗口）|
| `lcd_font.h/.c` | W25Qxx 字库读取 + GB2312 中英文/数字渲染 |

## 移植自

`D:\Users\Project-Keil\PRJ\F407VGT6\diansai-basic\Core\Src\main.c`（原代码堆叠于 main.c，已按分层抽离）
