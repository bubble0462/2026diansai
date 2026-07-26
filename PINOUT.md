# STM32F407VGT6 Final Pinout

This is the shared 2026 electronic-design-contest base project for the
STM32F407VGT6 in LQFP100. The authoritative configuration is
`f407demo-mdk5.ioc`; generated HAL code must remain synchronized with it.

## Fixed Public Pins

| Pin | Function / label | Configuration | Initial state |
|---|---|---|---|
| PH0 | RCC_OSC_IN | 8 MHz HSE crystal | N/A |
| PH1 | RCC_OSC_OUT | 8 MHz HSE crystal | N/A |
| PA13 | SWDIO | SYS_JTMS-SWDIO | N/A |
| PA14 | SWCLK | SYS_JTCK-SWCLK | N/A |
| PC13 | LED | Output, push-pull, no pull | High, active low |

## LCD And Font Flash (SPI2)

| Pin | Function / label | Configuration | Initial state |
|---|---|---|---|
| PB13 | SPI2_SCK | AF5, master SPI2 | N/A |
| PB14 | SPI2_MISO | AF5, master SPI2 | N/A |
| PB15 | SPI2_MOSI | AF5, master SPI2 | N/A |
| PD3 | SPI_RST | Output, push-pull, no pull | High |
| PD4 | SPI_DC | Output, push-pull, no pull | High |
| PD5 | SPI_CS | Output, push-pull, no pull | High |
| PD6 | SPI_CS2 | Output, push-pull, no pull | High |
| PD7 | BL | Output, push-pull, no pull | High |

SPI2 is master, full duplex, 8-bit, MSB first, CPOL low, CPHA first edge,
software NSS, APB1 clock divided by 4. With PCLK1 at 42 MHz, SCK is 10.5 MHz.
SPI2 DMA and interrupt are disabled. PB12 is not SPI2_NSS.

## AD9959 Software Serial GPIO

| Pin | Label | Initial state |
|---|---|---|
| PC0 | AD9959_SCLK | Low |
| PC1 | AD9959_CS | High |
| PC2 | AD9959_IO_UPDATE | Low |
| PC3 | AD9959_SDIO0 | Low |
| PC5 | AD9959_PS1 | Low |
| PC6 | AD9959_PS2 | Low |
| PC7 | AD9959_PS3 | Low |
| PC8 | AD9959_PS0 | Low |
| PC9 | AD9959_SDIO1 | Low |
| PC10 | AD9959_SDIO2 | Low |
| PC11 | AD9959_SDIO3 | Low |
| PC12 | AD9959_PWR | Low (normal operation, not power-down) |
| PB12 | AD9959_RESET | Low |

All AD9959 pins are push-pull outputs with no pull and very-high speed. CubeMX
generates safe `HAL_GPIO_WritePin` levels before `HAL_GPIO_Init`, reducing
configuration-time output glitches. The software-serial register driver under
`BSP/AD9959` is adapted from the verified
`D:\Users\Project-Keil\PRJ\F407VGT6\diansai-basic` implementation and uses its
25 MHz reference clock, PLL x20, and 500 MHz system-clock settings. The base
firmware initializes the delay timer and resets the AD9959 after GPIO setup;
applications select channels and configure output frequency/amplitude as needed.

## USART1 Debug Port

| Pin | Function | Configuration |
|---|---|---|
| PA9 | USART1_TX | AF7 |
| PA10 | USART1_RX | AF7 |

USART1 is asynchronous 115200 baud, 8 data bits, 1 stop bit, no parity, no
hardware flow control, oversampling 16. DMA and RX interrupts are disabled.

## Keys

| Pin | Label | Configuration |
|---|---|---|
| PE0 | KEY0 | Input, pull-up, active low |
| PE1 | KEY1 | Input, pull-up, active low |
| PE2 | KEY2 | Input, pull-up, active low |
| PE3 | KEY3 | Input, pull-up, active low |
| PE4 | KEY4 | Input, pull-up, active low |
| PE5 | KEY5 | Input, pull-up, active low |
| PE6 | KEY6 | Input, pull-up, active low |
| PE7 | KEY7 | Input, pull-up, active low |

No verified Tian-Kong-Xing board schematic was available in this workspace, so
the requested internal pull-ups are used explicitly rather than assuming an
external resistor network. Keys use polling and software debounce; EXTI is not
enabled.

## Analog And Timer Resources

| Pin | Peripheral function | Base configuration |
|---|---|---|
| PA1 | ADC1_IN1 | 12-bit, single conversion, software trigger |
| PA2 | ADC2_IN2 | 12-bit, single conversion, software trigger |
| PA3 | ADC3_IN3 | 12-bit, single conversion, software trigger |
| PA4 | DAC_OUT1 | No trigger, output buffer enabled |
| PA5 | DAC_OUT2 | No trigger, output buffer enabled |
| PA0 | TIM2_CH1 | PWM-output placeholder, not started |
| PA6 | TIM3_CH1 | PWM-output placeholder, not started |
| PA7 | TIM3_CH2 | PWM-output placeholder, not started |
| PB0 | TIM3_CH3 | PWM-output placeholder, not started |
| PB1 | TIM3_CH4 | PWM-output placeholder, not started |
| PE9 | TIM1_CH1 | PWM-output placeholder, not started |
| PE11 | TIM1_CH2 | PWM-output placeholder, not started |
| PD12 | TIM4_CH1 | PWM-output placeholder, not started |
| PD13 | TIM4_CH2 | PWM-output placeholder, not started |
| PD14 | TIM4_CH3 | PWM-output placeholder, not started |
| PD15 | TIM4_CH4 | PWM-output placeholder, not started |

The ADC, DAC and timer settings are safe base placeholders only. This project
does not implement ADC DMA, synchronized ADC operation, measurement state
machines, application PWM parameters or frequency counting. Topic-specific
clones may change those operating modes while retaining non-conflicting shared
interfaces.

## SPI3 Conflict

SPI3 is deliberately disabled and must not be documented as freely available:

- PC10 is fixed as AD9959_SDIO2.
- PC11 is fixed as AD9959_SDIO3.
- PC12 is fixed as AD9959_PWR.
- PB3 is not externally available on the target Tian-Kong-Xing board.

Therefore no complete SPI3 pin group can coexist with the fixed AD9959 wiring.

## Clock Tree

| Clock item | Value |
|---|---|
| HSE | 8 MHz crystal/resonator |
| PLL source | HSE |
| PLLM / PLLN / PLLP / PLLQ | 8 / 336 / 2 / 7 |
| SYSCLK / HCLK | 168 MHz / 168 MHz |
| APB1 | /4, PCLK1 42 MHz, timer clock 84 MHz |
| APB2 | /2, PCLK2 84 MHz, timer clock 168 MHz |
| 48 MHz domain | 48 MHz |

## Clone-Project Policy

Fixed public pins are HSE, SWD, LED, SPI2 LCD/font Flash, AD9959, USART1 and the
eight keys. ADC/DAC/timer pins form the common instrument resource pool and
should remain available in the base project. A cloned topic project may modify
ADC/DAC/timer modes and parameters when required. For example, a clone may
reassign PA0 to ADC1_IN0, but that change must stay in the clone's own `.ioc`
and must not be merged into this base pinout.
