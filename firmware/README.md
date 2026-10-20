# ai m0改 —— type-C口功率计算器放大板 · MSPM0G3507 · Keil 工程（两个屏，各一个工程）

> **本目录里有 2 个可以独立打开、编译、烧录的 Keil 工程：**
>
> | 屏 | 工程文件（双击打开） | 编译结果（已实测） | 屏幕上显示 |
> |---|---|---|---|
> | **1.9 寸 TFT · ST7789V · 320×170 彩色** | `ai_m0_gai.uvprojx` | **0 Error / 0 Warning**，Code=13868 RO=13668 | **电压 / 电流 / 功率 / 开机秒数**（读 INA228） |
> | **0.96 寸 OLED · SSD1306 · 128×64 单色** | `OLED版_0.96寸\ai_m0_oled.uvprojx` | **0 Error / 0 Warning**，Code=4350 RO=2338 | 四行 1 / 2 / 3 / 4 |
>
> 两个工程：**显示屏引脚完全一样**（PA31/PA22/PA7/PB7/PB6），
> 按哪个屏接线就打开哪个工程，F7 编译 → F8 烧录即可。
>
> ⚠ **编辑/替换工程文件（.uvprojx/.uvoptx）前请先关闭 Keil**：uVision 在关闭工程时会把内存里的
> 工程状态写回文件，如果它开着旧版本，就会把外部改动覆盖掉（本次已经踩过一次，见 §11）。

> **怎么用（三步）**
> 1. 双击本目录下的 `ai_m0_gai.uvprojx` 用 Keil uVision 打开
> 2. **F7 / Build**（已实测：**0 Error, 0 Warning**，Code=13868 / 字库等 RO=13668 字节）
> 3. **F8 / Download** 烧录（工程已配好 J-Link + MSPM0G1X0X_G3X0X_MAIN_128KB 烧录算法）
>
> ⚠ **接线前先看 §0**：1.9" 模块的第 8 脚 **BLK（背光）在板上那个 7 脚座上没有位置**，
> 不接的话背光不亮、**上电就是全黑**（这是本工程遇到过的最主要“黑屏”原因）。
>
> 显示屏：**1.9 寸 TFT，驱动 IC = ST7789V，320×170（横屏）**，4 线 SPI
> 板子：**U4 = MSPM0G3507SPMR（LQFP-64）**
> 引脚来源：**立创EDA API 实时读取** `D:\加力创\板子\type-C口功率计算器放大.eprj2`
> 的符号引脚 + PCB 焊盘网络，并与 TI SysConfig 器件数据 LQFP-64(PM) 物理脚号三方核对
> TFT 驱动来源：`C:\TI\mspm0_sdk_2_10_00_04\examples\nortos\LP_MSPM0G3507\driverlib\06_ui_display\keil\hardware\`
> （原 `hw_lcd.c/h` + `lcdfont.h`，本工程只改了底层通信与引脚）

---

## ★ INA228 电源检测（1.9" TFT 工程最新改动）

> 对应要求：**“通过读取电源检测芯片，在 1.9 寸屏上显示电流、电压，和开机后从 0 秒计时”**。
> 实现依据 **TI INA228 数据手册 ZHCSN47A（中文版）**，寄存器地址 / 位域 / 换算系数 / 复位值
> 全部逐条核对过（手册原文摘录见本节 E）。

### A. 上电后屏幕内容（320×170 白底）

| 行 | 位置 / 字号 | 显示内容 | 说明 |
|---|---|---|---|
| 第 0 行 | y=4，16 号小字，一行 7 段 | `M:5449` `D:2381` `40` `V:04FFF` `S:FFFFF` `TK:OK` `INA238` | **诊断行**（见 §I / §J）：<br>`M:` = 制造商 ID（应为 `5449`）<br>`D:` = 器件 ID（INA228 应为 `2281`，**INA238 为 `2381`**）<br>跟着的 2 位 = 实际 I²C 地址<br>`V:` = VBUS 寄存器**原始码**（十六进制）<br>`S:` = VSHUNT 寄存器原始码<br>`TK:` = SysTick 自检<br>最右 = 程序自动识别出的型号名 |
| 第 1 行 | y=26，32 号大字，红 | `U=  12.345 V` | **电压** = VBUS 原码 × 对应型号的 LSB |
| 第 2 行 | y=62，32 号大字，绿 | `I=   1.234 A` | **电流** = VSHUNT ÷ R5（R5 = 10 mΩ） |
| 第 3 行 | y=98，32 号大字，蓝 | `P=  15.200 W` | 功率 = 电压 × 电流（本板叫“功率计算器”，顺手加上） |
| 第 4 行 | y=134，32 号大字，黑 | `T=      12 s` | **开机以来秒数，从 0 开始**（1 ms 中断计数，见 §G/§H） |

数字是**固定 12 字符、右对齐**的画法：值变短时用空格（背景色）擦掉旧像素，
并且**只在内容变化时才重画那一行**，所以不会闪烁。

### B. 板上 INA228 的实际接法（立创EDA 的 PCB 焊盘网络读出）

| INA228 符号脚 | 板上网络 | 接到 U4 | 用途 |
|---|---|---|---|
| 1 — A1 | GND | — | 地址脚接地 |
| 2 — A0 | GND | — | 地址脚接地 ⇒ **7 位地址 = 100 0000b = 0x40** |
| 3 — ALERT | ALERT | PB12 / 物理 64 脚 | 报警输出（本工程用轮询，不配置它） |
| 4 — SDA | SDA | **PA16 / 物理 9 脚** | I²C 数据 |
| 5 — SCL | SCL | **PA17 / 物理 10 脚** | I²C 时钟 |
| 6 — VS | VCC | 3.3V | 供电 |
| 7 — GND | GND | — | 地 |
| 8 — VBUS | IN+ | — | 总线电压输入，接在采样电阻的**电源侧** |
| 9 — IN− | IN− | — | 采样电阻负载侧（去另一个 Type-C 的 VBUS） |
| 10 — IN+ | IN+ | — | 采样电阻电源侧 |

**采样电阻 R5 = 10 mΩ**，跨接在 IN+ / IN− 之间；I²C 上拉 R2/R3 = 10 kΩ 板上已有（见 §6）。

### C. 关键换算（10 mΩ 正好凑成最干净的一组数）

| 量 | 手册系数 | 本板数值 |
|---|---|---|
| 总线电压 VBUS | 195.3125 µV/LSB | 直接乘 |
| 分流电压 VSHUNT | 312.5 nV/LSB（ADCRANGE = 0，±163.84 mV） | 直接乘 |
| 电流 LSB | VSHUNT_LSB ÷ R5 | 312.5 nV ÷ 10 mΩ = **31.25 µA/LSB** |
| 可测最大电流 | 163.84 mV ÷ R5 | **16.384 A** |
| SHUNT_CAL | 13107.2×10⁶ × CURRENT_LSB × R5 | **4096 = 0x1000**（正好等于芯片复位值，10 mΩ 是个“天生匹配”的取值） |
| 芯片温度 | 7.8125 m°C/LSB | 直接乘 |
| 器件 ID | DEVICE_ID = 0x2281；MANUFACTURER_ID = 0x5449 | 用来判断通信是否真的通了 |

> 电流是**直接用 VSHUNT ÷ R5 算的**（在数学上等价于手册公式(4) 的
> `CURRENT × CURRENT_LSB`），好处是完全不依赖 SHUNT_CAL 写没写成功；
> 同时程序依然按手册公式(2) 把 `SHUNT_CAL = 4096` 写进芯片，
> 这样芯片自己的 CURRENT / POWER / ENERGY 寄存器也是准的，以后要用随时可读。

### D. 为什么要用**软件 I²C**（和屏的软件 SPI 一个道理）

- PA16 / PA17 确实是 I2C1 的 SDA / SCL（IOMUX **PINCM38 / PINCM39**），功能上没问题；
- 但本工程**不重新跑 SysConfig**（见 §7.4），在 `ina228.c` 里直接配 IOMUX 更省事，
  也不占用任何外设、不会和屏的引脚打架；
- 总线跑 **~100 kHz**，读一次 4 个寄存器**不到 2 ms**，对 200 ms 的显示刷新毫无影响；
- I²C 是“线与”总线，只能拉低不能推高。MSPM0 的 GPIO 不能直接切成开漏，
  所以驱动里用**输出使能位 DOE** 模拟：拉低 = 写 0 + 使能输出，释放 = 关掉输出让
  板上 10 kΩ 上拉到 3.3 V。这样绝不会出现主从对顶，完全符合 I²C 电气规范。

### E. 想改的地方都在这两个文件

| 你要改什么 | 改哪里 |
|---|---|
| **采样电阻换了**（不是 10 mΩ） | `ina228.h` → `INA228_R_SHUNT_MOHM`（改一个数，电流系数和 SHUNT_CAL 全部自动跟着变） |
| I²C 地址或引脚变了 | `ina228.h` → `INA228_I2C_ADDR`（其实程序会在 0x40~0x4F 自动找，一般不用改）、`INA228_SDA_*` / `INA228_SCL_*` |
| I²C 想更慢/更稳（线长、干扰大） | `ina228.h` → `INA228_I2C_HALF_CYCLES`（现在是 5 µs = 100 kHz，调大就更慢） |
| 刷新快慢 | `empty.c` 主循环末尾 `delay_ms(200)` |
| 采样平均次数/转换时间 | `ina228.c` → `INA228_Init()` 里写 `ADC_CONFIG` 的那一行（现为 MODE=Fh 连续、三路各 1052 µs、AVG=16 ≈ 50 ms 一次完整刷新） |
| 显示排版（位置/字号/颜色） | `empty.c` 顶部 `Y_LINE1..4` / `LINE_X` / `COLOR_x` 与 `make_line()` |
| 不要上电自检、上电更快 | `empty.c` → `TFT_SELFTEST_ON_BOOT` 改成 `0` |

### F. 如果屏幕上的状态行不是 `OK`

| 屏幕显示 | 含义 | 怎么办 |
|---|---|---|
| `INA228 NOT FOUND` | 总线上没有任何器件应答 | ① 量 U1 的 6 脚（VS）有没有 3.3 V；② 量 PA16/PA17 在不通信时是否被 R2/R3 拉到 3.3 V（不到就是上拉/虚焊问题）；③ 确认 U1 真的焊了、方向没错 |
| `D:xxxx@40` 是**红色** | 有器件应答，但器件 ID 不是 0x228x（或制造商 ID 不是 0x5449） | 芯片不是 INA228（可能被换成了 INA226 之类）。程序仍会继续读数，但寄存器含义不同，**数值不可信** |
| `D:2281@4x`（地址不是 40） | 实际 I²C 地址不是 0x40 | 说明板上 A0/A1 的接法和我读到的不一样 —— 不用改代码，程序自动认；但请告诉我一下 |
| `TK:NG`（红色） | SysTick 1 ms 中断没起来 | 秒表不可信。先检查有没有别处关中断；这一版已用 2 KB 栈 + 中断方案，正常应为绿色 `TK:OK` |
| `V:` 换算出来 ≠ `U=` 显示 | 唯一的可能就是换算代码被改过 | 正常版本这两个数一定自洽 |
| `V:` 换算出来 = `U=`，但和万用表不符 | **芯片读到的就是那个电压**，不是代码问题 | 万用表请直接量 **U1 的 8 脚（VBUS）对 7 脚（GND）**；并确认电源是从 `IN+` 那一侧的 Type-C 口喂进去的（见 §B 和 §I.1） |
| `OK` 但电流恒为 0 | 没有电流流过采样电阻（没插负载） | 正常。插上负载（手机/负载仪）就会有读数 |

### G. ★ 秒表（`T=` 那一行）修复：轮询 SysTick → 1 ms 中断

上一版报的现象是 **`T=` 显示一个很大而且不再变化的值**，这不是显示问题，是计时方案本身选错了。

**原因（可以算得很死）：**

- SysTick 是 **24 位递减计数器**。80 MHz 下它能数的最大值是 `0x01000000` 个周期
  = **209.7 ms**，超过就回绕。
- 而本工程主循环一轮 = `delay_ms(200)` **加上画屏时间**（4 行 32 号字走软件 SPI 约 100 ms），
  实际 **250~350 ms，比 209.7 ms 还长**。
- 轮询写法（读 `SysTick->VAL` 前后差值）只能识别“两次读之间最多回绕一次”，
  所以一旦循环周期超过 209.7 ms，算出来的其实是 **真实间隔 mod 209.7 ms**，
  而不是真实间隔 —— 秒数会严重偏慢；如果循环周期正好落在 209.7 ms 的整数倍附近，
  差值几乎恒为 0，**秒表看上去就完全不动**，与观察到的现象一致。

**改法：** 不再轮询，改成 **SysTick 每 1 ms 产生一次中断**，中断里给毫秒计数器加 1。
这样计数与主循环快慢完全无关，秒数 = 中断次数 ÷ 1000，既不会跑飞也不会停住。
用法与 TI SysConfig 自动生成的 `SYSCFG_DL_SYSTICK_init()` 完全一致
（`DL_SYSTICK_init` → `DL_SYSTICK_enableInterrupt` → `NVIC_SetPriority` → `DL_SYSTICK_enable`），
中断函数就是覆盖 `startup_mspm0g350x_uvision.s` 里那个 `[WEAK] SysTick_Handler`。

**顺带加的两点：**

1. 屏幕右上角显示 **`TICK OK` / `TICK FAIL`** —— 上电后先等 20 ms 自检一次，
   确认 1 ms 中断真的在跑。只要不是 `TICK OK`，就说明秒表不可信（红色），
   一眼就能把“计时坏了”和“读数坏了”分开。
2. `Tick_Init()` 挪到了 `SYSCFG_DL_init()` 之后、`lcd_init()` 之前，
   所以秒数是从 **MCU 启动那一刻**开始算的，不是从屏幕画好才开始。

### H. ★ 真正的病根：**栈溢出**（这一条同时解释了「乱数」和「一直是 0」）

改成中断后现象变成 **`TICK OK` 是绿的、但秒数一直是 0**。查 map 文件发现：

```
0x20200068   s_cache      （65 字节）  ← 显示行缓存
0x202000AC   s_tick_ms    （ 4 字节）  ← ★ 毫秒计数器
0x202000B8   STACK 起点，栈顶 = 0x202001B8，栈【向下】生长
```

- TI 官方启动文件里 `Stack_Size EQU 0x00000100` = **只有 256 字节**；
- 而 Keil 链接后给出的 **Maximum Stack Usage = 316 字节**（这还没算上中断帧）；
- 栈顶 0x202001B8 往下用 **268 字节** 就会碰到 `0x202000AC`，**正好是 `s_tick_ms`**。

所以：

| 版本 | 现象 | 原因 |
|---|---|---|
| 上一版（轮询） | `T=` 显示 1074415 这类乱数 | 栈把 `s_tick_ms` / `s_tick_last` 覆盖成了垃圾 |
| 这一版（中断） | `T=` 一直是 0 | 中断在 +1，栈在抹，永远到不了 1000 |

**修法：把 `startup_mspm0g350x_uvision.s` 里的 `Stack_Size` 由 `0x100` 改成 `0x800`（2 KB）。**
改完链接结果：`s_tick_ms`(0x202000AC) 到最低栈指针(0x202008B8−316 = 0x202007C4)
之间有 **1812 字节余量**，彻底不重叠。本芯片 RAM 有 32 KB，
改完 `ZI-data` 从 436 变成 2228，仍然只占 7%，毫无压力。

> ⚠ 如果你以后把这段代码搬到别的 TI 例程里，**记得同样检查该例程的 `Stack_Size`** ——
> TI 的 MSPM0 启动文件默认都是 256 字节，只要函数调用深一点、或者加了中断，就很容易踩到。

### I. 电压读数不准（例：万用表 3.24 V，屏上 4.00 V）—— 怎么定性 + 精度改进

#### I.1 先定性：这**不是**芯片精度问题

| 指标（INA228 数据手册 ZHCSN47A，直流精度表） | 数值 |
|---|---|
| **VBUS 电压增益误差 `G_BERR`** | 典型 **±0.01%**，最大 **±0.05%** |
| VBUS 增益误差漂移 `G_B_DRFT` | ±20 ppm/°C |
| 分流电压增益误差 `G_SERR` | 典型 ±0.01%，最大 ±0.05% |
| 功率总测量误差 TME（满量程） | ±0.5% |

你那个偏差是 **(4.00−3.24)/3.24 = 23.5%**，是**最大规格的约 470 倍**。
换算公式本身只有一步乘法（`原始码 × 195.3125 µV`），**不可能算错 23%**。
所以只可能是下面三类之一：

1. **接线/测点**：万用表量的那个点和 INA228 的 8 脚 VBUS 不是同一个节点。
   本板 VBUS 接在网络 `IN+`（见 §B），也就是**给板子供电的那个 Type-C 口那一侧**。
   如果电源是从另一个 Type-C 口（`IN−` 侧）喂进去的，`IN+` 就是悬空的，
   读出来的数字毫无意义 —— 这是最容易踩的一条。
2. **不是 INA228**：`MANUFACTURER_ID`(0x3E)=0x5449 在 INA226 等 TI 器件上也是这个值，
   光看它认不出型号。所以这一版**同时校验 `DEVICE_ID`(0x3F) 的高 12 位是否为 0x228**，
   并把结果直接显示在屏幕左上角（`D:2281@40`，绿色=全对，红色=型号不对）。
3. **换算/读取代码**：已用屏幕上的 `V:` 原始码彻底堵死这个可能（见 I.2）。

#### I.2 屏幕上的原始码就是判据

顶部诊断行里的 `V:040CD` 是 VBUS 寄存器的**原始 20 位码**（十六进制）。判定方法：

```
把 V: 后面的十六进制数转成十进制，乘以 195.3125 µV，就应该正好等于 U 行的读数。
  · 如果乘出来的电压 ≠ U 行显示   → 代码换算问题（这一版已不可能）
  · 如果乘出来的电压 = U 行显示，但和万用表不符 → 芯片读到的就是那个电压，
                                                 问题在接线/测点/芯片本身
```

举例：`V:040CD` → 0x40CD = 16589 → 16589 × 195.3125 µV = **3.2400 V**，
这时 U 行必须显示 `3.240`；若万用表量到的是 3.24 V，那就完全一致、一切正常。
如果屏上 `V:` 换算出来是 4.00 V，而万用表在 **INA228 的 8 脚（VBUS）对 7 脚（GND）**
上量到的是 3.24 V —— 那才轮到怀疑芯片/焊接。

#### I.3 这一版做的精度改进

| 改进 | 原来 | 现在 | 效果 |
|---|---|---|---|
| **等转换完成再读** | 固定延时 120 ms 后直接读，可能读到"半新半旧"的数据 | 轮询 `DIAG_ALRT` 的 **CNVRF 标志**（bit1）再读 | 每次读到的 VBUS/VSHUNT/温度属于**同一批**转换结果，杜绝撕裂值 |
| **硬件平均** | 1052 µs × **16** 次平均（≈50 ms 一批） | 540 µs × **64** 次平均（≈104 ms 一批） | 按手册表 8-2，这一档的"无噪声 ENOB"最高（17.7 位），噪声约为原来的 **1/4** |
| **软件滑动平均** | 无 | 最近 **4** 次结果求平均（`INA228_FILTER_N`） | 显示更安静；功率用平均值相乘，U/I/P 三者自洽 |
| **器件 ID 校验** | 只查 `MANUFACTURER_ID` | `MANUFACTURER_ID` + **`DEVICE_ID` 的 DIEID**（0x228x） | 能识别出"其实不是 INA228"，直接显示红色 `D:xxxx` |

> 想调平均深度：`ina228.h` 的 `INA228_FILTER_N`（4 → 8 更稳、反应更慢）。
> 想调硬件平均：`ina228.c` 里 `INA228_Init()` 写 `ADC_CONFIG` 那一行的 `AVG` 字段
> （3 = 64 次；往上 4=128、5=256、6=512、7=1024，越大越慢越稳）。

### J. ★★ 电压 3.24 V 显示成 4.00 V 的真正根因：**I²C 主机 ACK 极性写反了**

现场把诊断行读回来（这是定位的关键一步）：

```
M:54FF   D:22FF   40   V:04FFF   S:FFFFF   TK:OK   UNKNOWN
```

规律一眼就出来了 —— **每一次多字节读，只有第 1 个字节是对的，后面全是 `FF`**：

| 读的寄存器 | 应该是 | 实际读到 | 第 1 字节 | 第 2 字节起 |
|---|---|---|---|---|
| `M:` 0x3E 制造商 ID | `5449` | **`54FF`** | `54` ✅ | `FF` ❌ |
| `D:` 0x3F 器件 ID | `2281` | **`22FF`** | `22` ✅ | `FF` ❌ |
| `V:` 0x05 VBUS | `040CD0`（对应 3.24 V） | **`04FFFF`** | `04` ✅ | `FF` ❌ |
| `S:` 0x04 VSHUNT | ≈`FFFFF0`（约 0） | **`FFFFFF`** | `FF` ✅ | `FF` ❌ |

`FF` = SDA 没人驱动。原因就是 `ina228.c` 里这一行**写反了**：

```c
/* ✗ 原来的写法：读完一个字节就"释放"SDA —— 那其实是 NACK！ */
if (ack) { SDA_REL(); } else { SDA_LOW(); }

/* ✅ 正确写法：I²C 约定"主机还要继续读"时必须把 SDA【拉低】回答 ACK；
        最后一个字节才释放 SDA（NACK），从机收到 NACK 才会交出总线 */
if (ack) { SDA_LOW(); } else { SDA_REL(); }
```

**连锁反应（把所有怪现象一次解释完）：**

1. 读完第 1 个字节就发 NACK → 从机停发 → 第 2 个字节起全读成 `0xFF`；
2. `MANUFACTURER_ID` 读成 `0x54FF`（应 `0x5449`）、`DEVICE_ID` 读成 `0x22FF`（应 `0x2281`）
   → **ID 校验必然变红**（芯片其实一直是正品 INA228）；
3. VBUS 读成 `0x04FFFF`（应 `0x040CD0`）→ `>>4` 得 `0x04FFF` → **3.24 V 被算成 4.000 V**；
4. VSHUNT 本来约 0（`0xFFFFF0`），被补成 `0xFFFFFF` → 看着也“正常”，更难发现。

> 也正因如此，我当时根据 `54FF`/`22FF` 猜成了"板上装的是 16 位那一族的 INA238"——
> **那个结论是错的**，是被 ACK 这个 bug 误导的。实测确认：芯片就是 **INA228**
> （修好后 `M:5449 D:2281` 会变绿，型号名显示 `INA228`）。

**为什么写的时候一直没暴露：** 写寄存器不需要主机发 ACK（只有从机 ACK），
所以参数配置全部正常；而读寄存器"第 1 个字节总是对的"，看起来数值也像那么回事。
这是个典型的"只在多字节读、且只看整体效果时才露馅"的 bug。

**本次改动：**

1. **改正 `i2c_read_byte()` 的 ACK 极性**（上面那两行）—— 这是本次唯一真正影响读数的修改。
2. 顺带保留并按 `DEVICE_ID` 自动识别型号（`0x228x` INA228/229 读 3 字节；
   `0x238x`/`0x237x` 读 2 字节、LSB 用 5 µV / 3.125 mV、SHUNT_CAL 系数 `819.2e6`）——
   现在识别结果会正确显示 `INA228`。
3. 诊断行改成 `M:` 制造商 ID + `D:` 器件 ID + 地址 + `V:`/`S:` 原码 + `TK:` + 型号名：
   **以后凡是"数值不对"，先看这一行** —— 只要 `M:5449 D:2281` 是绿的、`V:` 换算出来又等于 U 行，
   那就一定是芯片/接线的问题，不用再怀疑代码。

---

## 0. ★ 上电黑屏（1.9" TFT）—— 硬件问题，接线前先看

针对“**1.9 寸 TFT 上电后不显示（黑屏）**”，代码侧修了 2 处、并加了上电自检；
但**黑屏最常见的原因是硬件：背光（BLK）那根线**，请先照 §0.1 查。

### 0.1 最可能的原因：模块第 8 脚 **BLK 悬空**（背光不亮 ⇒ 全黑）

板上显示屏座（PCB 位号 **OLED1**，7 脚）从立创EDA网络表读出的**实际脚序**是：

| 座子脚 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| 网络 | **GND** | **VCC** | C0-SCL | C0-SDA | OLEDRES | DC | CS |

而 1.9 寸 TFT 模块是 **8 脚**：在 CS 后面还有第 8 脚 **BLK（背光）**。
⇒ 把模块直接插进这个 7 脚座时，**BLK 悬空、背光永远不亮，屏幕必然是黑的**
（这时程序、SPI、复位统统没问题，屏也是好的）。

**解决办法（任选一种，改完立刻就能亮）：**

- **A**：BLK 接到 3.3V（或座上第 **2** 脚 VCC）—— 背光常亮，与程序无关；
- **B**：BLK 接到 **PB9 = U4 第 61 脚 = PINCM26**，由程序点亮 —— **本工程现在是这个方案**
  （`board_pins.h` 里 `TFT_BLK_ENABLE` 已默认打开，驱动一上电就把它拉高）。
  映射已用 TI 官方生成的配置头核对：`GPIOB.9 with pinCMx 26 on package pin 61`，
  且 EDA 网络表里本板第 61 脚**没有网络**（悬空），所以拿它当 BLK 是安全的。

### 0.2 代码侧确实有问题的 2 处（已修）

| # | 原来 | 现在 | 为什么 |
|---|---|---|---|
| 1 | `LCD_WR_REG(0x11)` 之后**没有等待**，紧接着灌了上百条参数 | 补上数据手册要求的 **≥120 ms** 等待；另加 `0x01` 软复位 + 规范硬件复位时序（RES 低 20ms → 高 120ms） | ST7789 规定 SLPOUT 后 120ms 内发的命令**可能被忽略**，参数丢了就可能黑屏/花屏 |
| 2 | 软件 SPI **零延时**，SDA 建立时间只有十几 ns（偏紧） | 每位插入 `LCD_SPI_DELAY_NOP`（默认 2）条 NOP，SCL 降到约 3~5 MHz，并把 5 根线配成**高驱动能力** | 建立/保持时间余量增大，杜邦线/排线长一点也不会丢位；嫌慢可把这个宏改成 0/1，花屏就加到 4/8 |

### 0.3 上电自检：一眼看出黑屏卡在哪一层

现在上电会依次显示 **红 → 绿 → 蓝 → 白 → 黑 →（白底黑框）“TFT OK”**，停 1 秒后才是 1/2/3/4：

| 你看到的现象 | 结论 / 下一步 |
|---|---|
| 全程全黑，**背光也不亮** | 背光/供电问题 → 按 §0.1 接好 BLK；顺便量模块 **VCC–GND 是不是 3.3V** |
| **背光亮**但一直黑、也没有 “TFT OK” | SPI/控制线接线问题 → 对着 §3 的表逐根核对 SCL/SDA/RES/DC/CS 有没有接反、CS 有没有漏接 |
| 颜色不对（红显示成蓝） | 面板要 BGR 顺序：`hw_lcd.h` 里 `LCD_MADCTL` 的 bit3 置 1（即 0x70 → **0x78**） |
| 画面偏移 / 上下反了 | 改 `hw_lcd.h` 的 `LCD_MADCTL` / `LCD_X_OFFSET` / `LCD_Y_OFFSET`（注释里写了每种取值） |
| 正常显示“TFT OK”和 1/2/3/4 | ✅ 硬件、接线、时序全对 |
| 想确认 MCU 到底有没有在刷屏 | 万用表量 **PA31 / PA22 对地电压**：刷屏时约 **1.5~1.7V**（约一半时间在高）；一直是 0V 或 3.3V 就说明没在发数据 |

> 自检确认没问题后，把 `empty.c` 里的 `TFT_SELFTEST_ON_BOOT` 改成 **0**，上电就直接显示 1/2/3/4。

**本次修改后的编译验证**：Keil MDK 5.40 / ArmClang V6.22 → **0 Error, 0 Warning**，
`Program Size: Code=8626 RO-data=13558 RW-data=0 ZI-data=352`，
`Objects\ai_m0_gai.hex` 已重新生成（数据范围 `0x00000000~0x000056B0`，在 128KB MAIN 区内），
**打开工程按 F8 即可直接烧录**。

---

## 1. 本次改动（0.96" OLED → 1.9" TFT ST7789）

| 项目 | 原来（0.96" OLED） | 现在（1.9" TFT ST7789） |
|---|---|---|
| 驱动 IC / 分辨率 | SSD1306 / 128×64 单色 | **ST7789V / 320×170 彩色(65K)** |
| 接口 | 4 线 SPI（软件模拟） | **4 线 SPI（软件模拟）**，同样是 SCL + SDA + DC + CS + RES |
| 驱动文件 | `OLED.c/.h` + `OLED_Data.c/.h` | **`hw_lcd.c/.h` + `lcdfont.h`**（沿用 06_ui_display 例程的驱动，API 不变） |
| 显示内容 | 四行 1/2/3/4（8×16 单色字） | **上电自检（红/绿/蓝/白/黑→“TFT OK”）+ 四行 1/2/3/4（32 号彩色字，白底，四个不同颜色）** |
| 主函数 | 仅 OLED 显示 | **仅 TFT 显示**（其它外设一律不初始化） |
| 引脚 | PA31/PA22/PA7/PB7/PB6 | **完全一样**（老网络名 C0-SCL / C0-SDA / OLEDRES / DC / CS 直接复用） |
| 背光 BLK | 无此脚 | **★ 板上的 7 脚座没有 BLK，必须自己引**（默认由 PB9 / U4 第 61 脚点亮，见 §0.1） |
| 旧代码 | — | 已移到 `OLED_旧版备份\`（不参与编译，可随时对照/回退） |

编译验证：**Keil MDK 5.40 / ArmClang V6.22 → `0 Error(s), 0 Warning(s)`**
`Program Size: Code=8626 RO-data=13558 RW-data=0 ZI-data=352`（RO 里主要是字库；
Code 比原来大是因为加了上电自检图形、复位/睡眠等待和软件 SPI 时序空指令）。

---

## 2. 从立创EDA读出的完整引脚表（U4 · MSPM0G3507SPMR · LQFP-64）

物理脚号 = 原理图符号脚号 = PCB 焊盘号（三方核对一致）。

| 物理脚 | 端口 | 网络名 | 板上功能 | 本工程处理 |
|---:|---|---|---|---|
| 9 | PA16 | SDA | INA228 的 I²C 数据（具备 I2C1_SDA 功能） | 不使用 |
| 10 | PA17 | SCL | INA228 的 I²C 时钟（具备 I2C1_SCL 功能） | 不使用 |
| 12 | PA19 | SWDIO | 调试口 U2 | 调试器占用 |
| 13 | PA20 | SWCLK | 调试口 U2 | 调试器占用 |
| 18 | PA22 | C0-SDA | **TFT SDA(MOSI)**，与 43 脚并联 ⚠ | ✅ 软件 SPI MOSI |
| 38 | NRST | NRST | 复位按键 SW7 | 系统复位脚 |
| 39 | PA31 | C0-SCL | **TFT SCL(SCK)**，与 44 脚并联 ⚠ | ✅ 软件 SPI SCLK |
| 40 | VDD | VCC | 3.3V | 电源 |
| 41 | VSS | GND | 地 | 电源 |
| 43 | PA3 | C0-SDA | **与 18 脚并联的重复脚** ⚠ | ⛔ 高阻输入，驱动代码已注释 |
| 44 | PA4 | C0-SCL | **与 39 脚并联的重复脚** ⚠ | ⛔ 高阻输入，驱动代码已注释 |
| 49 | PA7 | OLEDRES | **TFT 复位 RES** | ✅ GPIO 输出 |
| 50 | PB2 | NET1 | 按键 SW5 | 不使用 |
| 52 | PB4 | NET3 | 按键 SW9 | 不使用 |
| 53 | PB5 | NET4 | 按键 SW8 | 不使用 |
| 58 | PB6 | CS | **TFT 片选 CS** | ✅ GPIO 输出 |
| 59 | PB7 | DC | **TFT 数据/命令 DC** | ✅ GPIO 输出 |
| 60 | PB8 | NET2 | 按键 SW6 | 不使用 |
| 64 | PB12 | ALERT | INA228 报警输出 | 不使用 |

其余 U4 引脚在本板**均未连接网络**（悬空），所以如果 BLK 想用 GPIO 控制，随便挑一个空脚即可
（本工程默认示例用 **PB9 / 物理 61 脚**）。

### 板上引出的两个排针（可直接插 TFT 模块）

| 排针 | 脚位定义 |
|---|---|
| **H1**（1×7，2.54mm） | 1=CS，2=DC，3=OLEDRES，4=C0-SDA，5=C0-SCL，6=VCC，7=GND ← **正好是 TFT 的 5 根信号线 + 电源** |
| **H2**（1×5，2.0mm） | 1=NET1，2=NET2，3=NET3，4=NET4，5=NRST |

> 说明：网络名 `OLEDRES` 是当年配 0.96" OLED 时取的，接 TFT 时它就是 TFT 的 RES，一一对应即可。

---

## 3. TFT 接线

```
         MSPM0G3507                        1.9" TFT (ST7789V, 320x170, 8 脚)
   PA31 / 39脚  C0-SCL  ────────────────►  SCL  (SPI 时钟)       座子第 3 脚
   PA22 / 18脚  C0-SDA  ────────────────►  SDA  (SPI 数据 MOSI) 座子第 4 脚
   PA7  / 49脚  OLEDRES ────────────────►  RES  (复位，低有效)   座子第 5 脚
   PB7  / 59脚  DC      ────────────────►  DC   (1=数据 0=命令)  座子第 6 脚
   PB6  / 58脚  CS      ────────────────►  CS   (片选，低有效)   座子第 7 脚
   ──────── 3.3V / GND ─────────────────►  VCC  /  GND          座子第 2 / 1 脚
   PB9  / 61脚 ─────────────────────────►  BLK  (背光)  ★ 或直接把 BLK 接 3.3V
   PA4  / 44脚 ──┐（与 PA31 并联，本工程置高阻）
   PA3  / 43脚 ──┘（与 PA22 并联，本工程置高阻）
```

> ⚠ **BLK 一定要接**：板上那个 7 脚座子没有 BLK 脚位，模块的第 8 脚会悬空 ⇒ 背光不亮 ⇒ 全黑。
> 接到 **PB9 / U4 第 61 脚**（本工程默认，程序点亮）或直接接 **3.3V** 都行。详见 §0.1。

**背光 BLK** 两种接法（`board_pins.h` 第二节里说明，代码不用动）：

- **A**：BLK 直接接 3.3V —— 背光常亮；此时把 `board_pins.h` 里的 `TFT_BLK_ENABLE` 注释掉也可以
  （开着也无害：PB9 本来悬空，输出高电平不影响任何东西）；
- **B（默认）**：BLK 接到空闲脚 **PB9 / U4 第 61 脚 / PINCM26** —— `TFT_BLK_ENABLE` 默认已打开，
  驱动在 `LCD_GPIO_Init()` 里**一上电就把它拉高**点亮背光，`lcd_init()` 结束时再确认一次；
  想临时关背光可以调用 `LCD_Backlight(0)`。

> ⚠ 注意：不同厂家的 1.9" 模块丝印顺序不完全一样（有的模块把 BLK 放在最边上、有的是 GND/VCC 在前），
> **接线前务必对着模块背面的丝印**，VCC 接 3.3V（这块屏逻辑和背光都按 3.3V 用）。

---

## 4. SPI 引脚检查（结论：只能用软件 SPI，方案正确）

用 TI 官方器件数据（SysConfig `MSPM0G350X.json` + SDK `mspm0g350x.h` 的 IOMUX 定义）核对：

| 引脚 | PINCM | 该脚全部可用复用功能 | 有无 SPI？ |
|---|---|---|---|
| **PA31**（39，TFT SCL） | PINCM6 | GPIO / UART0_RX / **I2C0_SCL** / TIMA0_CCP3_CMPL / TIMG12_CCP1 / SYSCTL_CLK_OUT / TIMG7_CCP1 / TIMA1_CCP1 | ❌ **没有** |
| **PA22**（18，TFT SDA） | PINCM47 | GPIO / UART2_RX / TIMG8_CCP1 / UART1_RTS / TIMA0_CCP1 / SYSCTL_CLK_OUT / TIMA0_CCP0_CMPL / TIMG6_CCP1 | ❌ **没有** |
| PA4（44，重复 SCL 脚） | PINCM9 | GPIO / TIMG8_CCP1 / **SPI0_POCI** / UART2_RTS / TIMA0_CCP3 / LFCLKIN / TIMG7_CCP1 / TIMA0_CCP1_CMPL / **I2C1_SCL** | ⚠ 只是 SPI0 的 **POCI(MISO)**，不是 SCLK |
| PA3（43，重复 SDA 脚） | PINCM8 | GPIO / TIMG8_CCP0 / **SPI0_CS1_POCI1** / UART2_CTS / TIMA0_CCP2 / COMP1_OUT / TIMG7_CCP0 / TIMA0_CCP1 / **I2C1_SDA** | ⚠ 只是 SPI0 的 **CS1/POCI1**，不是 PICO(MOSI) |

1. 屏的 SCL/SDA 落在 PA31/PA22，**这两脚没有任何 SPI 复用功能** ⇒ 硬件 SPI 用不了，
   只能 GPIO 翻转模拟（本工程已实现，`hw_lcd.c` 里的 `spi_write_bus()`）。
2. 06_ui_display 例程用的是 **SPI1 = PB9(SCLK)/PB8(MOSI)** + PB10/PB11/PB14/PB26 控制脚，
   那是 LaunchPad 的接法；本板 **PB8 是按键 NET2**，且屏根本不在 PB8/PB9 上 ⇒
   原来的 `DL_SPI_transmitData8(SPI_LCD_INST, ...)` 代码在本板上不能直接照抄，已替换掉。
3. 软件 SPI 速度：80 MHz 下每位约 0.2 µs（≈2~4 MHz），远低于 ST7789 写周期上限（约 62.5 MHz），
   刷满 320×170 全屏约 0.1~0.2 秒，静态显示无压力。

---

## 5. 引脚冲突 / 引脚重合的处理（“对应代码注释”）

### 5.1 引脚重合（同一网络挂了两个 MCU 脚）

| 网络 | 并联的两个脚 | 风险 | 处理 |
|---|---|---|---|
| C0-SCL | PA31(39) + PA4(44) | 两个推挽输出电平相反即“对顶”，大电流、可能损坏 IO | 只用 **PA31** 驱动；PA4 显式配成**无上下拉高阻输入**，绝不翻转；备用代码在 `hw_lcd.c` 中**整块注释** |
| C0-SDA | PA22(18) + PA3(43) | 同上 | 只用 **PA22**；PA3 置高阻输入，代码注释 |

### 5.2 与蓝本 / 例程冲突的引脚（已全部剔除）

| 蓝本或例程配置 | 占用脚 | 本板该脚实际是 | 冲突 |
|---|---|---|---|
| 蓝本 `GPIO_KEY.S2` | PA31 | **屏 SCL** | ⛔ 严重 |
| 蓝本 `GPIO_GRAY.OUT8` | PA22 | **屏 SDA** | ⛔ 严重 |
| 蓝本 `GPIO_IMU.CS` / 例程无所谓 | PB6 | **屏 CS** | ⛔ 严重 |
| 蓝本 `SPI_0(SPI1).misoPin` | PB7 | **屏 DC** | ⛔ 严重 |
| 例程 `SPI_LCD(SPI1).sclkPin` | PB9 | 未连接（本板屏不在 PB9） | ⛔ 接法不同 |
| 例程 `SPI_LCD(SPI1).mosiPin` | PB8 | 按键 NET2(SW6) | ⛔ 冲突 |
| 例程 `GPIO_LCD` 控制脚 | PB10 / PB11 / PB14 / PB26 | 未连接 | ⛔ 接法不同 |
| 蓝本 `GPIO_KEY.S4` | PB2 | 按键 NET1(SW5) | ⛔ 冲突 |
| 蓝本 `UART_1`(蓝牙) | PB4 / PB5 | 按键 NET3 / NET4 | ⛔ 冲突 |
| 蓝本 `ENCODER_B.LB` | PB12 | INA228 ALERT | ⛔ 冲突 |
| 蓝本 `GPIO_HC05.STATE` | PA16 | INA228 I²C SDA | ⛔ 冲突 |
| 蓝本 `GPIO_OLED`(软件 I²C 屏) | PA0 / PA1 | 未连接 | 已废弃（换成 ST7789 驱动） |

处理方式：

1. **主函数只保留屏幕显示**，蓝本的 `Encoder_Init()/App_Tasks_Init()/App_Run()` 等全部不调用；
2. 这些模块的源文件**没有加进 Keil 工程**（工程只有 Source + User + Doc 三组）；
3. `board_pins.h` 里把冲突引脚以**注释形式**保留并逐条写明原因，方便对照；
4. 本工程 `empty.syscfg` 只留 **SYSCTL（80MHz 时钟树）+ SWD 调试口**，**一个 GPIO 组都不建**，
   屏的 5 根线由 `LCD_GPIO_Init()` 直接用 `DL_GPIO_*` 配置 ⇒ 从根本上消除“SysConfig 抢脚”。

---

## 6. I²C 是否需要外接上拉电阻

### 6.1 板上的 I²C 总线（INA228）—— 已有上拉，**不需要再加**

```
VCC ── R2(10kΩ) ── 网络 SCL ── U1 pin5(SCL) ── U4 pin10 (PA17)
VCC ── R3(10kΩ) ── 网络 SDA ── U1 pin4(SDA) ── U4 pin9  (PA16)
VCC ── R1(10kΩ) ── 网络 ALERT ─ U1 pin3(ALERT) ─ U4 pin64 (PB12)
```

⇒ INA228 的 I²C 已有 **10 kΩ 外部上拉（R2/R3）**，满足 I²C“必须外部上拉”的要求，**无需另加**。
（3.3V、总线电容不大时可跑 100 kHz；要稳跑 400 kHz 可考虑并成 4.7 kΩ。）

### 6.2 为什么 I²C 必须外部上拉

- I²C 是**开漏**总线，只能拉低不能拉高，高电平完全靠上拉电阻；
- MSPM0G3507 的 GPIO 内部上拉是**几十 kΩ 量级且默认关闭**，做 I²C 时上升沿非常慢，
  3.3V / 400 kHz（要求 tr ≤ 300 ns）下不可靠；
- 所以**硬件 I²C 必须依赖外部上拉**，本板已经给了（R2/R3）。

### 6.3 本工程用的 SPI 不需要任何上拉

SPI 是推挽输出，SCL/SDA/DC/CS/RES 都**不需要上拉电阻**；
`C0-SCL`/`C0-SDA` 这两个网络上也确实没有任何上拉（实测确认），直接接屏即可。
（若哪天改用 I²C 屏，才需要在 SCL/SDA 上外接 4.7 k~10 k 上拉。）

---

## 7. Keil 工程使用说明

### 7.1 打开、编译、烧录

| 步骤 | 操作 |
|---|---|
| 打开 | 双击 `ai_m0_gai.uvprojx` |
| 编译 | F7（Build）。**已实测 0 Error / 0 Warning**，输出 `Objects\ai_m0_gai.axf` + `ai_m0_gai.hex` |
| 烧录 | F8（Download）。已配好 **J-Link + `MSPM0G1X0X_G3X0X_MAIN_128KB.FLM`**（0x00000000，128KB） |
| 调试 | Ctrl+F5；SWD 引脚 = 板上 U2 排针（PA19=SWDIO / PA20=SWCLK / VCC / GND） |

### 7.2 换调试器 / 换探头

- 默认 **J-Link**（继承你现有工程的配置，含 `-U602722571` 序列号）。换探头时打开
  **Options for Target → Debug → Settings** 重新选中即可。
- 用 **CMSIS-DAP / DAP-Link**：Debug 下拉改选 **CMSIS-DAP Debugger**，烧录算法保持
  `MSPM0G1X0X_G3X0X_MAIN_128KB`（0x00000000，0x20000）不变。
- 也可直接烧 `Objects\ai_m0_gai.hex`。

### 7.3 工程里的 SDK 路径

- 头文件：`C:\ti\mspm0_sdk_2_10_00_04\source`、`...\source\third_party\CMSIS\Core\Include`
- 预编译驱动库：`C:\ti\mspm0_sdk_2_10_00_04\source\ti\driverlib\lib\keil\m0p\mspm0g1x0x_g3x0x\driverlib.a`
- 器件宏 `__MSPM0G3507__`；器件包 `TexasInstruments.MSPM0G1X0X_G3X0X_DFP 1.3.1`
- 编译选项里加了 **`-Wno-invalid-source-encoding`** —— 原因见 7.5，别删。

### 7.4 `ti_msp_dl_config.c/.h` 与 `empty.syscfg`

已由 SysConfig（`--compiler keil`）生成好放进工程，**不依赖 SysConfig 也能直接编译**
（“Before Make → Run SysConfig” 钩子已关闭，因为 SDK 自带 `syscfg.bat` 只在工程位于 SDK
目录树内时才能找到 `product.json`）。若你改了 `empty.syscfg`，手动重生成一次：

```bat
"C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" -o "本目录" ^
  -s "C:\ti\mspm0_sdk_2_10_00_04\.metadata\product.json" ^
  --compiler keil "本目录\empty.syscfg"
```

### 7.5 ⚠ `lcdfont.h` 必须保持 **GBK(ANSI)** 编码，不要转成 UTF-8

`lcdfont.h` 里的汉字字模索引是 `unsigned char Index[2]`，用 `{"中",...}` 这种 **2 字节 GBK**
中文字面量初始化。若把该文件转成 UTF-8，一个汉字变成 3 字节，会报
`initializer-string for char array is too long`，**汉字取模索引会错位、中文显示不出来**。

所以：本工程把 `lcdfont.h` **按原始字节（GBK）原样保留**（和你 06_ui_display 例程里那份完全一致），
并在编译选项里加了 `-Wno-invalid-source-encoding` 关掉“源码里有非 UTF-8 字节”的告警，
从而做到 **0 Warning**。你自己新写的 `.c/.h` 用 UTF-8 没问题（本工程其它文件都是 UTF-8）。

---

## 8. 文件清单

```
ai m0改/
├─ ai_m0_gai.uvprojx      ★ Keil 工程（1.9寸 TFT/ST7789 + INA228 版）
├─ ai_m0_gai.uvoptx         Keil 选项（J-Link + 烧录算法 + 窗口布局）
├─ mspm0g3507.sct           分散加载文件（Flash 128K @0x0，RAM @0x20200000）
├─ startup_mspm0g350x_uvision.s  TI 官方启动文件
├─ empty.c                  ★ 主函数：TFT 初始化 + 毫秒时基 + INA228 初始化
│                              + 循环显示 U / I / P / T（固定宽度右对齐，变化才重画）
├─ ina228.c / ina228.h      ★ INA228 驱动：软件 I²C(开漏模拟) + 寄存器读写 + 物理量换算
│                              （寄存器地址、位域、换算系数均按数据手册 ZHCSN47A 核对）
├─ empty.syscfg             最小 SysConfig（SYSCTL 80MHz + SWD）
├─ ti_msp_dl_config.c/.h    SysConfig 生成的时钟/引脚初始化
├─ hw_lcd.c / hw_lcd.h      ★ ST7789 驱动（源：06_ui_display 例程，已改为本板软件 SPI + 引脚；
│                              含 SWRESET/SLPOUT 等待、上电自检 LCD_TestPattern、背光 LCD_Backlight）
├─ lcdfont.h                ASCII 12/16/24/32 + 汉字 12/16/24/32 字库（GBK，勿转码）
├─ Delay.c / Delay.h        独立延时（备用，不依赖 FreeRTOS）
├─ board_pins.h             板级引脚定义 + 冲突/重合说明 + INA228 接法（改引脚只改这里）
├─ Objects\ai_m0_gai.hex    已编译验证的固件（可直接烧录，0x0~0x6B90，约 25 KB）
├─ jlink_test.bat / .jlink  只读的 J-Link 探头/芯片连接自检（不擦芯片）
├─ OLED版_0.96寸\           ★ 姊妹工程：0.96寸 OLED 版（双击里面的 ai_m0_oled.uvprojx）
│   ├─ ai_m0_oled.uvprojx / .uvoptx
│   ├─ empty.c / OLED.c / OLED.h / OLED_Data.c / OLED_Data.h
│   ├─ board_pins.h（OLED_* 引脚宏）/ Delay.c/.h / empty.syscfg / ti_msp_dl_config.c/.h
│   ├─ startup_mspm0g350x_uvision.s / mspm0g3507.sct
│   ├─ Objects\ai_m0_oled.hex   已编译验证的 OLED 版固件（显示 1/2/3/4）
│   └─ README.md
└─ README.md                本文件
```

> 想换回 OLED：直接打开 `OLED版_0.96寸\ai_m0_oled.uvprojx`；两块屏的接线是同一组网络（H1 排针），
> 不用改任何跳线，只换模块、换工程即可。

### 显示内容与常见改动（`empty.c`）

```c
#define TFT_SELFTEST_ON_BOOT 0        /* 0=直接显示测量画面（默认）；1=先跑红/绿/蓝/白/黑自检 */

lcd_init();                           /* 点亮背光 + ST7789 复位 + 寄存器初始化 */
LCD_TestPattern();                    /* 红/绿/蓝/白/黑 → 白底黑框“TFT OK”（自检时才跑）*/
LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);  /* 清屏：整屏白底 */
Tick_Init();                          /* SysTick 毫秒时基：计时从这里从 0 开始 */
INA228_Init();                        /* 配 I²C 引脚 → 自动找地址 → 写 CONFIG/ADC_CONFIG/SHUNT_CAL */

while (1) {
    INA228_Read(&d);                              /* 读 VBUS / VSHUNT / 温度 */
    make_line(line, 'U', d.vbus,    3, 'V');      /* 电压 */
    make_line(line, 'I', d.current, 3, 'A');      /* 电流 */
    make_line(line, 'P', d.power,   3, 'W');      /* 功率 */
    make_line(line, 'T', Tick_ms()/1000, 0, 's'); /* 开机秒数 */
    show_line(...);                               /* 内容变了才重画那一行 */
    delay_ms(200);
}
```

> OLED 版工程（`OLED版_0.96寸\`）没有跟着改，仍然是原来的四行 1/2/3/4 —— 它只是
> 用来单独验证屏幕的备用工程，需要也可以照这套思路加 INA228。

面板/时序的可调参数**全部集中在 `hw_lcd.h` 顶部**（换屏、换方向、花屏都改这里）：

| 宏 | 默认 | 作用 |
|---|---|---|
| `LCD_MADCTL` | `0x70` | 扫描方向：`0x70` 横屏 320x170（与 TI 例程、你的 02_screen_driver 一致）；`0x60` 上下镜像；`0x00/0xA0` 竖屏；颜色反了就把 bit3 置 1（`0x78`） |
| `LCD_X_OFFSET` / `LCD_Y_OFFSET` | `0` / `35` | GRAM 240x320、面板 170x320 ⇒ 横屏时行偏移 (240-170)/2=35 |
| `LCD_SPI_DELAY_NOP` | `2` | 软件 SPI 每位插入的 NOP 条数（约 3~5MHz）；花屏调大到 4/8，想更快调成 0/1 |

驱动还提供了这些现成接口（都在 `hw_lcd.h`）：

```c
LCD_Fill / LCD_DrawPoint / LCD_DrawLine / LCD_DrawRectangle / Draw_Circle / LCD_ArcRect
LCD_ShowChar(x,y,ch,fc,bc,sizey,mode)          /* sizey = 12 / 16 / 24 / 32 */
LCD_ShowString(x,y,"ABC",fc,bc,sizey,mode)     /* mode: 0=非叠加 1=叠加 */
LCD_ShowIntNum(x,y,num,len,fc,bc,sizey)        /* 显示整数 */
LCD_ShowFloatNum1(x,y,3.14,len,fc,bc,sizey)    /* 显示两位小数 */
LCD_ShowChinese(x,y,"中文",fc,bc,sizey,mode)   /* 常用汉字（字库在 lcdfont.h） */
LCD_ShowPicture(x,y,length,width,pic)          /* 显示 RGB565 图片 */
```

颜色宏：`WHITE BLACK BLUE RED GREEN CYAN YELLOW GRAY LGRAY …`（RGB565）。

### 排障

> **上电黑屏**（本工程最常见的问题）⇒ 直接看 **§0**：十有八九是模块第 8 脚 BLK 悬空、背光不亮，
> 以及 SLPOUT 少等 120ms / 软件 SPI 建立时间偏紧这两处（都已修）。

- **全白/全黑没内容**：先确认 DC/CS/RES 与 SCL/SDA 没接反（见 §3 表），再确认 **BLK 是否点亮**
  （模块 BLK 需接 3.3V 或接 PB9 由 `TFT_BLK_ENABLE` 驱动，见 §0.1）。
- **画面上下/左右反了**：改 `hw_lcd.h` 的 `LCD_MADCTL`（`0x70` ⇄ `0x60` ⇄ `0x00` ⇄ `0xA0`）。
- **画面有偏移**：改 `hw_lcd.h` 的 `LCD_X_OFFSET` / `LCD_Y_OFFSET`（`35` 是针对 1.9" 170 高面板；
  换成 240×320 的 1.3"/1.14"/2.0" 屏时，这两个偏移和 `LCD_W`/`LCD_H` 都要改）。
- **花屏/闪烁**：把 `hw_lcd.h` 的 `LCD_SPI_DELAY_NOP` 从 2 加到 4 或 8（软件 SPI 放慢），
  并检查 SCL/SDA 两根线是不是太长、有没有和别的线并在一起。
- **颜色反相（该红的显示成蓝）**：`LCD_MADCTL` 的 bit3 置 1（`0x70` → `0x78`）。

---

## 9. 需要你确认的一点

本次按“**板上 U4 直焊 MSPM0G3507SPMR**”给出引脚（PCB 实测唯一与屏/INA228/按键相连的器件）。
如果你的实际玩法是“**把立创·天猛星 MSPM0G3507 开发板插在 U5 上，再用杜邦线从 H1/H2 接到开发板**”，
那么屏实际落在天猛星板的哪几个脚由你的接线决定 —— 告诉我接线（或让我再读一次 EDA），
改 `board_pins.h` 一处即可（驱动 `.c` 一行都不用动）。

---

## 10. 下载失败排查：`Flash Download failed - "Cortex-M0+"`

先把话说清楚：**这个报错跟代码无关**。我逐项确认过：

| 检查项 | 结论 |
|---|---|
| 固件有没有占用/关闭 SWD 引脚（PA19/PA20）？ | **没有**。`board_pins.h`/`hw_lcd.c`/`ti_msp_dl_config.c` 里没有任何 PIN_19/PIN_20 的操作，不含 `DL_SYSCTL_disableSWD()` 之类调用 |
| 烧录镜像地址对不对？ | **对**。两个 hex 只有 `0x00000000..0x00004DB0`（TFT）/`0x00000000..0x00001A20`（OLED）一段，128KB Flash 里；没有误带 0x41C0xxxx(BCR/BSL) 配置区（那个区域 MAIN 算法烧不了，会导致下载失败） |
| Flash 算法对不对？ | **对**。`MSPM0G1X0X_G3X0X_MAIN_128KB`，Start `0x00000000`，Size `0x00020000` |

所以问题一定出在 **调试器 / 接线 / 芯片** 这一侧。

### 10.1 根因已定位：**你现在插的这个探头（ARM-OB 克隆版）驱动不了 MSPM0G3507**

我把你机器上三份 J-Link 日志对比了一下，结论非常明确：

| 日志（时间） | 探头 | 结果 |
|---|---|---|
| `empty\keil\JLinkLog.txt`（3-23） | **J-Link V9.70，S/N `602722571`**，固件 2023-12 | ✅ 干净：`Found Cortex-M0 r0p1`，无任何 Error |
| `06_ui_display\keil\JLinkLog.txt`（7-26） | J-Link **ARM-OB STM32（克隆版），S/N `20090928`**，固件 **2012-08** | ❌ `Error: Failed to initialized DAP.` + `Old FW that does not support reading DPIDR via DAP jobs` |
| **`ai m0改\JLinkLog.txt`（9-21 21:09，就是这次下载）** | 同一个 **ARM-OB 克隆版 `20090928`** | ❌ 同样 DAP 报错，随后：`CPU could not be halted`(×5)、`Could not read memory location 0x20200000 when trying to clear soft RAM BP`(×3)、`Failed to clear soft BP in RAM @ 0x20200000`(×3)、最后 `Error: Connection to emulator lost!` |

**为什么这会导致"Flash Download failed - Cortex-M0+"**：
Keil 下载分两步：① SWD 连上内核 → ② 把 Flash 算法（几十字节的 loader）放到 **RAM 0x20200000** 跑起来，由它擦写 Flash。
你这个克隆探头是 **2012 年的 J-Link OB 固件**，缺 "DAP jobs" 支持，**连内核都 halt 不住**，J-Link 也就写不进/读不出
0x20200000 那段 loader，于是第 ② 步直接失败 —— 报错文本就变成 Keil 那句笼统的
`Flash Download failed - "Cortex-M0+"`。**这跟你的工程、代码、接线都无关。**

> 附加佐证：我本机装的 **J-Link V9.28 软件甚至已经认不到这个克隆探头了**
> （`JLink.exe ShowEmuList USB` 返回空、连接一直等到超时）—— SEGGER 新版本已经不再支持这种老固件的 OB。

### 10.2 解决办法（按推荐顺序）

| # | 做法 | 说明 |
|---|---|---|
| **1（推荐）** | **换回你 3 月份用过的那个 J-Link V9（S/N `602722571`）** | 那份日志里它对 MSPM0G3507 是**完全正常**的。本工程已把 J-Link 设置恢复成指定这台探头，插上就能下载 |
| **2** | 用 **CMSIS-DAP / DAP-Link** | 便宜好用且对 MSPM0 支持完整。Keil：`Options for Target → Debug` 下拉改选 **CMSIS-DAP Debugger**；`Utilities → Settings` 里烧录算法保持 `MSPM0G1X0X_G3X0X_MAIN_128KB`（0x00000000，0x20000）不变。<br>注意：**天猛星板载的 DAP-Link 只能烧天猛星自己的 MCU**（U5 的 CLK/DIO/RST 与 U4 的 SWD 不相连），要烧 U4 得拿一个独立 DAP-Link 接到 U2 排针 |
| **3** | 想继续救这个克隆探头 | 可试：`JLinkConfig.exe` 看能不能升级它的固件（**有一定变砖风险，SEGGER 对非正品可能拒绝升级**）；把 SWD 降到 **1 MHz**（本工程已默认）；杜邦线尽量短；**板子单独供电**（别只靠 OB 的 3.3V）；Keil 里把 Reset 设为 `SYSRESETREQ` / 勾 **Connect under Reset**。但基于上面日志，成功率不高 |
| **4** | 换用 **Keil 之外**的烧录方式 | 例如另一台电脑/J-Flash + 正品探头；或让嘉立创朋友帮忙烧。 |

**本工程已经为你调整的两处设置**（两个工程都改了，且验证过工程仍能 0 Error/0 Warning 编译）：

1. J-Link 设置里的探头序列号恢复为 **`-U602722571`**（你验证过可用的那台 V9）。
   如果你手上只有克隆探头，请在 `Options for Target → Debug → Settings` 里的探头列表里重新选你自己的那个；
2. SWD 速率 **5 MHz → 1 MHz**（`-ZTIFSpeedSel1000`）—— 兼容性优先，下载 20KB 也就多几十毫秒。

### 10.3 换探头后仍失败时，按这个顺序自查

**① Keil 里选对探头**
`Options for Target → Debug` 右侧下拉选 **J-Link / J-Trace Cortex**（DAP-Link 选 **CMSIS-DAP Debugger**）
→ 点右边 **Settings**：
- **Port: SW**，**Max Clock: 1 MHz**
- 看下面 **SW Device** 框：正常应显示 `ARM CoreSight SW-DP` + `IDCODE 0x6BA02477`
  - 空着 / 报错 ⇒ 接线或供电问题（见 ②③）

**② 接线（板上 U2 排针，4 根线）**

| U2 排针 | 接到探头 |
|---|---|
| SWDIO（U4 的 **PA19 / 物理 12 脚**） | SWDIO / DIO |
| SWCLK（U4 的 **PA20 / 物理 13 脚**） | SWCLK / CLK |
| VCC（3.3V） | 3.3V（或只接 VTref，板子单独供电） |
| GND | GND |

最常见的低级错误：**DIO/CLK 接反**、**只接了 SWDIO/SWCLK 忘了 GND**、**板子没供电**。
先量一下 U4 的 **pin40(VDD)=3.3V、pin41(VSS)=0V** 再谈下载。

**③ 芯片是否在线**：双击本目录 `jlink_test.bat`（只读，不擦除）能读出 `60 01 20 20 ...` 就说明
探头→接线→供电→芯片这条链路是通的，问题就只在 Keil 设置里。
（注：用那个老克隆探头时，J-Link V9.28 的 `JLink.exe` 可能根本认不到它，此时以 Keil 的 `SW Device` 框为准。）

**④ 芯片到底在不在**
这块"放大"板上有**两个 MCU 位置**：
- **U4**：直焊的 `MSPM0G3507SPMR`（LQFP-64）—— 我们两个工程都是按 **U4** 的引脚写的；
- **U5**：立创·天猛星 MSPM0G3507 开发板插座。

实测 PCB 确认：**U5 的调试信号（CLK/DIO/RST）与 U4 的 SWD（PA19/PA20）没有任何连接**。
所以：如果你用的是"天猛星开发板插在 U5 上"，那你只能通过**天猛星板自己的 DAP-Link** 下载到**它自己的 MCU**
（此时 Keil 里要选 **CMSIS-DAP**），而且**屏/按键的引脚也不在 U4 那组网络上**（见 README §9）；
反过来，**如果 U4 没焊芯片，用 J-Link 接 U2 排针必然下载失败**。

**⑤ 芯片处于"跑飞/低功耗/被锁"状态**
如果你之前烧过其它程序（比如让 MCU 进了 STOP/STANDBY、或把 PA19/PA20 改成了普通 IO），
SWD 会连不上。恢复方法任选其一：
- Keil：`Options → Debug → Settings`，把 **Reset** 设为 `SYSRESETREQ`（或勾选 **Connect under Reset**），再下载；
- J-Link Commander 里执行：`connect` → `erase`（整片擦除）→ 然后 Keil 再下载。

**⑥ 换根 USB 线 / 换探头交叉验证**
你手上有 V9（`602722571`）和 ARM-OB 克隆两个探头 —— 用 V9 那个试，它的日志里是**连成功过 MSPM0G3507 的**。

### 10.4 Flash Download 里的正确参数（对照检查）

`Options for Target → Utilities → 勾选 Use Target Driver → Settings → Flash Download`：

| 项 | 正确值 |
|---|---|
| Programming Algorithm | `MSPM0G1X0X_G3X0X_MAIN_128KB`，Start `0x00000000`，Size `0x00020000` |
| RAM for Algorithm | Start `0x20200000`，Size `0x00000800`（用 `0x00008000` 也行，TI 默认） |
| 勾选 | ☑ Reset and Run |

> 说明：本工程只往 `0x00000000~0x00004DB0` 写数据（不到 20KB），**不需要** BCR/BSL 那些算法；
> 如果列表里同时挂着 `MSPM0G1X0X_G3X0X_NONMAIN` 之类的算法，把它去掉再试。

### 10.5 我这边还需要知道的一件事

**你手上有哪个探头可以用？**（3 月份那台 J-Link V9 `602722571` 还在不在？有没有独立的 DAP-Link？）
- 有 V9 → 插上直接下载，工程已按它配好；
- 只有那个 ARM-OB 克隆 → 建议换探头（或按 §10.2 第 3 条试升级固件），它在本芯片上基本无解；
- 只有天猛星板载 DAP-Link → 那它只能烧天猛星自己的 MCU，要烧 U4 需要另配一个 DAP-Link 接到 U2 排针。

如果你愿意，把 Keil **Build Output** 窗口那几行**完整原文**发我（`Flash Download failed` 上面通常还有
更具体的一行，如 `Programming Failed!` / `Cannot access target` / `No Algorithm found for: ...` /
`Failed to initialize DAP`），我可以再帮你确认一遍。



uVision 在**关闭工程**（或某些编辑动作）时会把自己内存里的 `.uvprojx/.uvoptx` 写回磁盘。
本次就发生了：你在 Keil 里打开着**旧版本**（当时还是 OLED 工程），我这边把工程改成 TFT 版之后，
Keil 一回写就把文件列表和编译开关都覆盖回去了 —— 表现就是 Keil 工程树里还列着
`OLED.c / OLED.h / OLED_Data.c / OLED_Data.h`，但这几个文件已经被移走，**编译会报找不到文件**。

已经修好（现在两个工程的清单都是对的），以后请注意：

## 11. 一个坑：Keil 会回写工程文件

1. **要我改工程文件时，先把 Keil 关掉**；改完再打开（或打开后按提示 Reload）。
2. 如果打开后发现工程树里的文件名不对，选 **Project → Close Project** 再重新双击 `.uvprojx` 打开。
3. 另：uVision 对 `.uvprojx` 里 `<Files>` 节点的**换行格式敏感** —— 把它压成一行会导致
   `UV4 -b` 直接返回错误码 15（工程打不开）。不要用脚本“压缩”工程 XML。

