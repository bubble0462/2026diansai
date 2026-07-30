# HANDOFF.md — F407VGT6 失真度测量与波形重构工程交接

> 接手者必读：本文档记录了从 H7B0 失真度测量工程移植到 F407VGT6（天空星）的完整过程、
> 当前状态、已踩过的坑、以及待办事项。**先通读"当前状态"和"已知坑"，再动代码。**

工程路径：`D:\Users\bubble\Desktop\2026diansai`
参考工程（已验证可用）：`D:\Users\bubble\Downloads\32`（单 ADC + TIM1 触发方案）
H7 原工程：`D:\Users\bubble\Desktop\2026电赛\H7B0_失真度测量`
串口屏 HMI：`D:\Users\bubble\Desktop\2026电赛\diansai.HMI`

---

## 一、当前状态（截至交接时）

### 能工作的部分 ✅
- **系统时钟**：144MHz（PLLM=4/PLLN=144/PLLP=2/PLLQ=6），FLASH_LATENCY=**5**（必须！），CSS 开启
- **ADC 采集**：单 ADC1（PA1/IN1），TIM1_CC1 上升沿触发，2.4 MSPS，4096 点/帧，DMA2_Stream0 一次性传输，HALFWORD
- **采集稳定**：Start/Complete 1:1 配对，帧率约 20 帧/秒（受 FFT 耗时限制）
- **FFT 分析**：自实现 radix-2 FFT，Hann 窗，基波/谐波检测，THD 计算
- **波形重构**：H1 基波最小二乘拟合，合成纯净正弦显示（当前只拟合 H1）
- **串口屏通信**：USART1@115200，PA9/PA10，TJC 协议（ref_start/ref_stop/addt），控件名与 H7 一致
- **ADC 数据验证**：100kHz/250mVpp/1.65V偏置 正弦波采集准确（中心~1960码，峰峰值~311码≈250mV）

### 待解决的问题 ⚠️（接手者优先处理）
1. **屏幕仍有闪烁**：fit 频繁失效导致波形 cle/addt 反复（已部分缓解，见"已知坑#6"）
2. **峰峰值偏低**：当前用 ADC 直接峰峰值（folded），约 241mV vs 实际 250mV（标定系数未调）
3. **采样数据抖动**：需进一步平滑
4. **H1-H5 全谐波重构未启用**：当前是 H7 原版（仅 H1），备份在 `/tmp/result_output_H1H5.c`
5. **AD603 程控增益未接入**：框架已移植但 `APP_GAIN_AUTO_CONTROL_ENABLED=0`

---

## 二、硬件与引脚配置

| 引脚 | 功能 | 配置 |
|------|------|------|
| PA1 | ADC1_IN1 | 模拟输入，信号入口（100kHz/250mVpp/1.65V偏置正弦） |
| PA9 | USART1_TX | → TJC 串口屏 RX |
| PA10 | USART1_RX | ← TJC 串口屏 TX |
| PA4 | DAC_OUT1 | → AD603 GNEG（程控增益控制，电压越大增益越小） |
| PA5 | DAC_OUT2 | 备用 |
| PC13 | LED | 诊断心跳（慢闪=有ADC数据，快闪=无数据） |
| PE9 | TIM1_CH1 | ADC 触发 PWM 输出（2.4MHz，AF1） |

时钟树：HSE 8MHz → PLL → SYSCLK 144MHz → APB1=36MHz(APB1定时器72MHz) → APB2=72MHz(定时器144MHz)

---

## 三、软件架构与数据流

```
PA1信号 → ADC1(2.4MHz, TIM1_CC1触发) → DMA2_Stream0(4096点, HALFWORD, NORMAL)
  → bsp_adc_dma.c: TakeFrame() 返回 uint16_t[4096]
  → signal_preprocess.c: 去DC + 63抽头FIR低通 + Hann窗 → float[4096]
  → spectrum_analyzer.c: radix-2 FFT → 频谱幅度
  → distortion_analyzer.c: 基波寻峰 + H1-H5谐波 + THD → measurement_result_t
  → result_output.c:
      ├─ Upp: 多分量最小二乘重构后取峰峰值（失败时回退相位折叠 min/max）
      ├─ Urms: 真有效值 5点滑动平均
      └─ 波形重构: H1-H3最小二乘拟合 → 组合波形 → 799点 → TJC addt 上传
  → USART1 → TJC串口屏
```

主循环（`main.c`）：
```c
while (1) {
    AppMeasurement_Process();   // 采集+FFT+分析
    ResultOutput_Process();     // 串口屏状态机
    LED心跳诊断();              // PC13 慢闪/快闪
}
```

---

## 四、关键配置参数（`Core/Inc/app_config.h`）

```c
#define APP_ADC_SAMPLE_RATE_HZ       2400000.0f   // 2.4 MSPS
#define APP_FFT_SIZE                 4096U         // FFT点数(频率分辨率586Hz)
#define APP_ADC_REFERENCE_V          3.300f        // 实际需标定
#define APP_ADC_FULL_SCALE           4095.0f
#define APP_ADC_VOLTAGE_GAIN         1.0f          // 前端增益，需按AD603实际填
#define APP_TJC_WAVEFORM_ID           1U           // ★ HMI波形控件id=1(非0!)
#define APP_TJC_WAVEFORM_POINTS       799U         // 屏幕宽度
#define APP_TJC_VALUE_PERIOD_MS       200U         // 数值刷新周期
#define APP_VOLTAGE_GLOBAL_SCALE      1.0f         // 全局标定，调这个修Upp偏差
#define APP_UPP_SCALE                 1.0f
#define APP_GAIN_AUTO_CONTROL_ENABLED 0            // AD603自动增益，标定后改1
```

---

## 五、踩过的坑（必读！每个都是血泪）

### 坑1：FLASH_LATENCY 必须是 5（致命）
- **现象**：烧录后芯片完全无法启动，ST-Link 连不上
- **原因**：144MHz 必须 `FLASH_LATENCY_5`，用 LATENCY_4 会导致 Flash 取指错误，CPU 跑飞
- **修复**：`SystemClock_Config` 里 `HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5)`
- **教训**：时钟配置错误会让芯片锁死，SWD 都连不上

### 坑2：TIM1 时钟没使能（致命）
- **现象**：ADC ADON=1 但 DMA 全0，TIM1 所有寄存器都是0
- **原因**：`MX_TIM1_Init` 用 `HAL_TIM_PWM_Init`，它调用 `HAL_TIM_PWM_MspInit`（不是 `HAL_TIM_Base_MspInit`）。而 tim.c 里只有 `HAL_TIM_Base_MspInit` 使能了 `__HAL_RCC_TIM1_CLK_ENABLE()`，PWM 版的 MspInit 不存在（用了弱定义空函数），导致 **TIM1 时钟从未使能，所有寄存器写入静默丢弃**
- **修复**：在 `MX_TIM1_Init` 开头显式 `__HAL_RCC_TIM1_CLK_ENABLE();`
- **教训**：CubeMX 生成的 MspInit 按外设模式分（Base/PWM），改模式要同步改 MspInit

### 坑3：烧录 DLL 错误
- **现象**：Keil 烧录报 "Internal DLL Error / Target DLL has been cancelled"
- **原因**：`.uvprojx` 里 `<Flash2>BIN\UL2CM3.DLL</Flash2>`（旧驱动）+ openocd 后台占用 ST-Link
- **修复**：改为 `<Flash2>BIN\UL2V8M.DLL</Flash2>`；烧录前 `taskkill openocd.exe`
- **备选烧录**：openocd 命令行（见"工具命令"章节）

### 坑4：F4 HAL 双模 ADC 坑（已放弃双模）
- **放弃原因**：F4 HAL 的 `HAL_ADCEx_MultiModeStart_DMA` 不会自动使能 slave ADC2；DDS/DMA 模式组合容易触发错误；调试困难
- **最终方案**：改用参考工程的**单 ADC1 + TIM1_CC1 触发**（验证可用），放弃交错采样
- **教训**：F4 双模 ADC 的 HAL 支持远不如 H7，能用单 ADC 就别用双模

### 坑5：F4 HAL API 差异（移植必改）
H7→F4 移植时这些 API 不同：
- `ADC_MultiModeTypeDef`：F4 没有 `DualModeData` 字段（已移除双模，不涉及）
- `HAL_ADCEx_Calibration_Start`：**F4 没有此函数**（F4 ADC 硬件自校准），调用会编译错，需删除
- UART 清除标志：H7 用 `__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF|...)`，**F4 不存在这些宏**，改用 `__HAL_UART_CLEAR_OREFLAG/NEFLAG/PEFLAG/FEFLAG`
- DAC 句柄：H7 是 `hdac1`，F4 是 `hdac`（无数字后缀）

### 坑6：屏幕闪烁（fit 失败反复 cle）
- **现象**：波形/参数显示但屏幕一直闪
- **原因**：F4 上 FFT 拟合结果不稳定，`fit_valid` 频繁在 true/false 切换 → `s_graph_available` 反复 0/1 → 每次失效都 `cle`（清屏）→ 下一帧又 `addt`（画波形）→ 反复刷屏
- **已做缓解**：`prepare_active_snapshot` 里 fit 失败时**保留上一次波形**不清零，只有 `component_count==0`（完全无信号）才 cle
- **未根治**：闪烁仍存在，可能还需（a）增加 fit 成功的滞后判断（连续N帧成功才更新）（b）降低刷新频率（c）排查 ORE 溢出导致的通信错误

### 坑7：Upp 偏低（FFT 加窗增益损失）
- **现象**：250mVpp 信号显示 241mV
- **原因**：`calculated_upp` 用 FFT 拟合的 RMS×√2 算峰峰值，FFT 加窗的相干增益补偿有 ~3.5% 系统损失
- **已修复**：Upp 改用 `folded_peak_to_peak_codes`（ADC 原始波形按相位折叠取 min/max），不走 FFT
- **残留**：仍有 ~3.6% 偏差，需调 `APP_VOLTAGE_GLOBAL_SCALE` 或 `APP_ADC_REFERENCE_V` 标定（实测 ADC 参考电压可能 3.28V 而非 3.3V）

### 坑8：USART1 ORE 溢出
- **现象**：RX 错误计数持续增长（dbg_rx_error），addt 回传丢失
- **原因**：`HAL_UART_Receive_IT` 单字节接收，屏幕密集回传时中断响应慢导致 ORE
- **已缓解**：USART1 中断优先级提到最高(0,0)，ADC DMA 降到(1,0)
- **备选**：改用 DMA 接收或环形缓冲

### 坑9：scatter file 与 CCM（8192点FFT时才需要）
- **背景**：曾尝试 8192 点 FFT（RAM 不够），用自定义 scatter 把 FFT 数组放 CCM
- **当前**：已改回 4096 点，恢复默认 scatter（`umfTarg=1`，`ScatterFile` 为空），移除了 `.ccm` section 标记
- **若要恢复8192点**：需重新配置 CCM，参考 git 历史的 `f407_demo_fft.sct`

---

## 六、工具命令速查

### 编译（Keil 命令行）
```bash
cd "D:/Users/bubble/Desktop/2026diansai/MDK-ARM"
/c/Keil_v5/UV4/UV4.exe -j0 -b f407demo-mdk5.uvprojx -o build_log.txt
# 看结果
grep -iE "Error\(s\)|Warning\(s\)" build_log.txt | tail -2
```

### 烧录（openocd，比 Keil 更稳）
```bash
# 先杀掉占用 ST-Link 的进程
taskkill //F //IM openocd.exe 2>/dev/null

cd /d/app/CLion-dependency/openocd
./bin/openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "adapter speed 1000; reset_config none" \
  -c "init; halt; program D:/Users/bubble/Desktop/2026diansai/MDK-ARM/f407demo-mdk5/f407demo-mdk5.hex verify; reset run; exit"
```

### 在线调试（openocd 读寄存器/内存）
```bash
cd /d/app/CLion-dependency/openocd
./bin/openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "adapter speed 1000; reset_config none" \
  -c "init; reset run; sleep 1000; halt" \
  -c "mdw 0x40023C00 1" \   # Flash ACR (LATENCY)
  -c "mdw 0x40012000 3" \   # ADC1 SR/CR1/CR2
  -c "mdw 0x40026010 2" \   # DMA2_Stream0 CR/NDTR
  -c "resume; exit"
```

### 常用寄存器地址
| 地址 | 寄存器 | 用途 |
|------|--------|------|
| 0x40023C00 | FLASH_ACR | bit[2:0]=LATENCY（144MHz必须=5） |
| 0x40023874 | RCC_CSR | 复位原因（看门狗/POR/软复位） |
| 0x40012000 | ADC1_SR | bit4=ORE溢出, bit2=EOC |
| 0x40012008 | ADC1_CR2 | bit0=ADON使能 |
| 0x40010000 | TIM1_CR1 | bit0=CEN使能 |
| 0x40026010 | DMA2_S0_CR | bit0=EN, bit25=TCIE |
| 0x40026014 | DMA2_S0_NDTR | 剩余传输数 |
| 0x40011008 | USART1_BRR | 波特率（115200→0x271） |

### 诊断变量地址（map文件查最新值）
```
dbg_start_calls     BSP_ADC_DMA_Start 调用次数
dbg_complete_calls  DMA完成回调次数（应≈start_calls）
dbg_start_fail      Start失败次数
s_sequence          已处理帧数（app_measurement.c）
s_dropped_count     显示丢弃帧数（result_output.c，生产>消费时正常）
dbg_rx_complete     UART RX完成次数
dbg_rx_error        UART RX错误次数（ORE/NE，应接近0）
```
用 `grep 变量名 f407demo-mdk5/f407demo-mdk5.map | grep 0x2000` 查地址。

---

## 七、待办优先级

### P0（闪烁根治）
1. **增加 fit 滞后**：`prepare_active_snapshot` 里，fit 失败时连续 N 帧（如3帧）才判定无效清屏，避免单帧抖动触发 cle
2. **验证 ORE**：烧录后读 `dbg_rx_error`，若仍增长，考虑改 UART 为 DMA 接收
3. **降低波形刷新频率**：波形 addt 不必每200ms一次，可降到400ms减少通信压力

### P1（幅值精度）
1. **标定 ADC 参考电压**：实测 PA1 接已知电压，反推 `APP_ADC_REFERENCE_V`（可能3.28）
2. **标定 Upp**：用 250mVpp 信号，调 `APP_VOLTAGE_GLOBAL_SCALE` 使显示=250
3. **5点平滑验证**：Upp 已用 `update_trimmed_average`（去极值），Urms 用 `update_average`，确认跳动是否消除

### P2（功能完善）
1. **H1-H5 全谐波重构**：备份在 `/tmp/result_output_H1H5.c`，恢复后改 `TJC_MAX_COMPONENTS=5, SYNTH_MAX_ORDER=5`，但注意会增大最小二乘矩阵（11阶），需验证 F4@144MHz 计算耗时和稳定性
2. **AD603 自动增益**：`gain_control.c` 已移植，标定各频段 DAC 码后设 `APP_GAIN_AUTO_CONTROL_ENABLED=1`
3. **8192点 FFT**（若需≤500Hz分辨率）：恢复 CCM scatter，但当前 4096点=586Hz 略超目标

### P3（性能）
1. **FFT 优化**：当前自实现 radix-2，可换 CMSIS-DSP 的 `arm_rfft_fast_f32`（4096点有预编译表），省CPU
2. **帧率提升**：当前~20fps，受 FFT 耗时限制，优化后可提升

---

## 八、关键文件说明

### 核心移植文件（从H7复制，F4适配）
| 文件 | 来源 | F4适配点 |
|------|------|----------|
| `Core/Src/result_output.c` | H7原版 | UART错误清除宏改F4版；Upp改用ADC直接峰峰值；fit失败保留波形 |
| `Core/Src/spectrum_analyzer.c` | H7原版 | 移除了.ccm section（4096点不需要） |
| `Core/Src/signal_preprocess.c` | H7原版 | 移除了s_centered缓冲（FIR实时重算省32KB） |
| `Core/Src/distortion_analyzer.c` | H7原版 | 无改动 |
| `Core/Src/app_measurement.c` | H7原版+适配 | 删除F4无有的ADC校准；FFT输入复用spectrum缓冲 |

### F4底层重写文件
| 文件 | 说明 |
|------|------|
| `Core/Src/adc.c` | 单ADC1，TIM1_CC1触发，参考工程配置 |
| `Core/Src/tim.c` | TIM1 PWM(ARR=59,Pulse=30)触发ADC；**开头显式使能TIM1时钟** |
| `BSP/SignalAcquisition/bsp_adc_dma.c` | 单ADC+TIM1触发采集层；**TIM1用寄存器启动(绕过HAL)** |
| `Core/Src/main.c` | 精简为采集+显示循环；FLASH_LATENCY_5；LED诊断 |

### 已移除/不用的文件
- `BSP/My_ADC/ad.c`：旧采集逻辑（已从工程移除，改用 bsp_adc_dma）
- `App/` 目录：旧 signal_measurement 等（未加入编译）
- `MX_TIM2_Init`/`MX_ADC2_Init`/`MX_ADC3_Init`：不再调用（单ADC方案）

---

## 九、信号与测试条件

当前测试信号（信号发生器输出）：
- 频率：100 kHz
- 幅度：250 mVpp
- 直流偏置：1.65 V
- 波形：正弦

ADC 预期读数：
- 中心码：1.65V × 4095/3.3 ≈ 2048（实测~1960，说明偏置或参考电压略偏）
- 峰峰码：250mV × 4095/3.3 ≈ 310（实测~311 ✓）

---

## 十、Git 与备份

- 当前分支：`bubble`
- H1-H5 改造版备份：`/tmp/result_output_H1H5.c`（恢复全谐波重构时用）
- 参考工程（金标准）：`D:\Users\bubble\Downloads\32`，其 `HardeWare/signal_analysis.c` 是验证可用的 FFT+拟合实现，遇到问题随时对比

---

## 接手者检查清单

- [ ] 先烧录当前固件，确认 ADC 数据流通（LED 慢闪 = OK）
- [ ] 读 `dbg_complete_calls` 确认采集正常
- [ ] 读 `dbg_rx_error` 确认 UART 错误是否仍增长
- [ ] 看屏幕闪烁现象，决定先攻 P0 哪一项
- [ ] 改代码前先 `grep 变量名 f407demo-mdk5.map` 查地址，方便 openocd 调试
- [ ] **改完必须 openocd 烧录验证**（Keil 烧录不稳）

---

## 十一、2026-07-30 接手优化记录

### 已完成

- 修复 `result_output.c` 的旧波形重复翻转问题：拟合失败保留上一帧时不再调用
  `reverse_graph()`，也不再重复上传旧波形。
- 增加连续 3 帧无可用图形才清屏的滞回，避免单帧 FFT/拟合抖动触发 `cle/addt`。
- 将屏幕数值与波形更新周期由 200 ms 调整为 500 ms，降低串口与屏幕刷新压力。
- USART1 RX 从单字节中断接收改为 DMA2 Stream2/Channel4、512 字节环形 DMA；
  主循环、半传输和全传输回调共同推进协议解析。
- 增加可直接从 map/OpenOCD 读取的诊断量：
  `dbg_rx_complete`（DMA 已消费字节数）和 `dbg_rx_error`（UART 错误回调次数）。

### 验证结果

- Keil ARMClang 6.22：0 Error、0 Warning。
- 固件大小：Code=39236、RO-data=896、RW-data=16、ZI-data=79584。
- DMA 改造前在线基线（3 秒）：`dbg_start_calls=68`、
  `dbg_complete_calls=68`、`dbg_start_fail=0`、`dbg_rx_error=40`。
- DMA 改造后的固件已通过 OpenOCD 写入并 `Verified OK`。
- 写入后 ST-Link USB 设备从系统中消失，暂时无法读取改造后的 5 秒计数；
  下次连接后优先确认 `dbg_rx_error` 是否保持为 0，并目测屏幕是否还闪烁。

### 当前诊断地址（以本次 map 为准）

| 变量 | 地址 |
|------|------|
| `dbg_start_calls` | `0x20000074` |
| `dbg_start_fail` | `0x20000078` |
| `dbg_complete_calls` | `0x20000080` |
| `s_sequence` | `0x20000090` |
| `s_dropped_count` | `0x200000BC` |
| `s_uart_recovery_count` | `0x200000C0` |
| `dbg_rx_complete` | `0x200000C4` |
| `dbg_rx_error` | `0x200000C8` |

### 幅值标定补充

- 屏幕闪烁最终确认由供电不稳定引起，供电问题已解决；软件侧滞回继续保留作为容错。
- 250 mVpp 标准输入实测显示 245 mVpp，因此仅将 `APP_UPP_SCALE` 标定为
  `250 / 245 = 1.020408`；该修正只作用于峰峰值，不影响 RMS 和谐波幅值。

### 2026-07-30 双 ADC 实测幅频补偿

- 100 kHz、250 mVpp 输入显示 247.00 mVpp / 85.45 mVrms；500 kHz
  显示 199.43 mVpp / 68.88 mVrms。
- 原 `APP_WEAK_SIGNAL_RMS_V=20mV` 会直接拒绝 50mVpp 正弦
  （理论仅 17.68mVrms），现降为 5mVrms。
- 后续确认测试时没有连接模拟前端，因此撤销由两个频点反推的 663kHz
  低通补偿；该补偿会错误放大 1MHz/1.5MHz 伪谐波，造成波形和 Upp
  在约 80mVpp 以上突变。
- 100kHz 标定更新为 `APP_UPP_SCALE=1.032803`、
  `APP_RMS_SCALE=1.034407`。
- 新增 ADC1/ADC2 奇偶样本逐帧均值与 RMS 对齐，减小低幅信号上的交错
  锯齿和假高频分量，并排除启动后的首个无效采样对。
- 双 ADC 改为精度优先配置：每路采样时间 15 周期、交错延迟 13 周期。
  DWT 实测 2048 点约 139264 个 144MHz 周期，合并采样率约
  2.117647MSPS，FFT 分辨率约 517Hz，仍满足题目要求；需复测
  500kHz 波形失真。

### 多谐波联合拟合准备

- 原题中频谱分量幅值定义为正弦表达式 `Ui*sin(...)` 中的峰值 `Ui`；
  整体有效值必须为真有效值，整体 Upp 必须保留各分量相位关系。
- 拟合采样范围由 8 周期增加到 32 周期，改善 500kHz 附近仅有少量
  采样点时 H1 + 两个谐波的七参数联合拟合条件。
- 联合拟合成功后，分量幅值、真 RMS、合成 Upp 与定性频谱统一使用
  同一组正弦/余弦系数，避免 FFT 幅值与时域重构结果不一致。
- 500 kHz 输入可能因 FFT 插值略高于上限而被波形截取逻辑拒绝，现已加入
  `APP_EDGE_TOLERANCE_BINS` 边界容差。
- 题面明确被测信号由基波与 1 个或 2 个谐波组成，因此最小二乘拟合已从仅 H1
  扩展为最多 3 个频率分量；波形显示和 Upp 均按各分量拟合幅值、相位重构。
- Upp 优先从致密重构的组合波形取峰峰值，避免 2.4 MSPS 在 500 kHz 仅有
  4.8 点/周期时漏采波峰；拟合失败时才回退到相位折叠 min/max。

---

## 十二、双 ADC 实验分支

- 分支：`codex/dual-adc-experiment`，稳定基线为 `bubble@23cf3ff`。
- ADC1 与 ADC2 使用共享的 PA1/ADC12_IN1，双规则交错模式、DMA Access Mode 2。
- ADC 时钟 36 MHz、12 bit、3 周期采样、7 周期交错延迟。
- DMA2 Stream0 按 32 bit 从 ADC_CDR 读取；小端内存中可直接解释为
  `ADC1, ADC2, ADC1, ADC2...` 的 `uint16_t` 序列。
- 每次 NORMAL DMA 结束后必须先将 ADC 公共 MULTI/DMA/DDS/DELAY 位归零，
  再恢复交错配置，否则实测会出现 Start:Complete=2:1。修复后 2 秒在线计数为
  Start=43、Complete=43、Sequence=42。
- 实测 ADC1/ADC2 平均码差约 1.9 LSB，单帧范围分别为 1822–2099 和
  1824–2099，初步匹配良好。
- DWT 实测后半帧 2048 样本耗时 61512 个 144 MHz CPU 周期，对应采样率
  约 4.794382 MSPS，实验版使用该值计算频率。
- 当前仍使用 4096 点 FFT，频率栅格约 1.17 kHz；下一阶段需要评估分档采样、
  扩展帧长或更精细频率估计，以满足题面 500 Hz 分辨率要求。
