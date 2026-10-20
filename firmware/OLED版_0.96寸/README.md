# OLED版_0.96寸 —— type-C口功率计算器放大板 · MSPM0G3507 · 0.96" OLED · Keil 工程

这是 `ai m0改` 的**姊妹工程**：同一块板、同一套引脚，只是显示屏驱动换回 **0.96 寸 OLED（SSD1306，128×64，7 脚 4 线 SPI）**。

> **怎么用（三步）**
> 1. 双击本目录下的 `ai_m0_oled.uvprojx` 用 Keil uVision 打开
> 2. **F7 / Build**（已实测：**0 Error, 0 Warning**，Code=4350 / RO=2338 / ZI=1376）
> 3. **F8 / Download** 烧录（J-Link + `MSPM0G1X0X_G3X0X_MAIN_128KB` 算法已配好）

| | 本工程（OLED） | 姊妹工程（TFT） |
|---|---|---|
| 工程文件 | `ai_m0_oled.uvprojx` | `..\ai_m0_gai.uvprojx` |
| 屏 | 0.96" OLED，SSD1306，128×64 单色 | 1.9" TFT，ST7789V，320×170 彩色 |
| 驱动文件 | `OLED.c/.h` + `OLED_Data.c/.h` | `..\hw_lcd.c/.h` + `..\lcdfont.h` |
| 显示内容 | 四行 1/2/3/4（8×16 单色字，居中） | 四行 1/2/3/4（32 号彩色字，白底） |
| 引脚 | PA31/PA22/PA7/PB7/PB6 —— **两块屏完全一样** | 同左 |

## 接线（与 TFT 版完全相同的一组网络）

```
        MSPM0G3507                        0.96" OLED (7pin)
  PA31 / 39脚  C0-SCL  ────────────────►  3  SCL  (D0/SCK)
  PA22 / 18脚  C0-SDA  ────────────────►  4  SDA  (D1/MOSI)
  PA7  / 49脚  OLEDRES ────────────────►  5  RES
  PB7  / 59脚  DC      ────────────────►  6  DC
  PB6  / 58脚  CS      ────────────────►  7  CS
  ──────── 3.3V / GND ─────────────────►  2  VCC  /  1  GND
```

板上 **H1 排针**（1=CS 2=DC 3=OLEDRES 4=C0-SDA 5=C0-SCL 6=VCC 7=GND）与这块屏的 7 个脚一一对应，可直接插。

## 关键点（与 TFT 版一致的结论）

- 屏的 SCL/SDA 接在 **PA31 / PA22**，这两个脚**没有任何 SPI 复用功能**（TI IOMUX 表已核对），
  所以必须用 **软件 SPI**（`OLED.c` 里的 `OLED_W_SCLK/OLED_W_MOSI` 位翻转），驱动已按 4 线 SPI 实现。
- **引脚重合**：PA31 与 PA4 并接在 C0-SCL、PA22 与 PA3 并接在 C0-SDA ⇒ 只驱动 PA31/PA22，
  PA4/PA3 显式配成**高阻输入**，备用驱动代码在 `OLED.c` 中**整块注释**。
- 蓝本工程与本板冲突的引脚（KEY S2=PA31、GRAY OUT8=PA22、IMU CS=PB6、SPI1 MISO=PB7、
  KEY S4=PB2、ENCODER_B=PB12、HC05 STATE=PA16、UART_1=PB4/PB5）在本工程里**一律未使用**，
  `board_pins.h` 中以注释形式逐条写明原因。
- **主函数只保留 OLED 显示**，其它外设一律不初始化。
- I²C 上拉问题：INA228 的 I²C 总线板上已有 **R2/R3 = 10 kΩ 上拉**，无需再加；
  SPI 屏的这几根线**不需要上拉**。

## 文件清单

```
OLED版_0.96寸/
├─ ai_m0_oled.uvprojx / .uvoptx   ★ Keil 工程（双击打开）
├─ empty.c                        主函数：仅 OLED 初始化 + 显示 1/2/3/4
├─ OLED.c / OLED.h                软件 4 线 SPI 的 SSD1306 驱动
├─ OLED_Data.c / OLED_Data.h      8x16 / 6x8 ASCII 字模
├─ board_pins.h                   板级引脚定义 + 冲突/重合说明（改引脚只改这里）
├─ Delay.c / Delay.h              独立延时（备用）
├─ empty.syscfg + ti_msp_dl_config.c/.h   最小 SysConfig（SYSCTL 80MHz + SWD）
├─ startup_mspm0g350x_uvision.s / mspm0g3507.sct
└─ Objects\ai_m0_oled.hex         已编译验证的固件
```

> SDK 路径用的是绝对路径 `C:\ti\mspm0_sdk_2_10_00_04\...`（Options → C/C++ → Include Paths 和
> Linker → Misc controls 两处），换 SDK 版本时改这两处即可。

常见改动：`empty.c` 里 `OLED_ShowString(60, 0, "1", OLED_8X16)` 四行就是显示屏内容；
`OLED_LINE_X` 改 0 即左对齐。屏幕反色/翻转改 `OLED.c` 的 `OLED_Init()` 里 `0xA1`→`0xA0`、`0xC8`→`0xC0`。

## 下载失败怎么办（`Flash Download failed - "Cortex-M0+"`）

跟代码无关（已确认固件没有占用/关闭 SWD 引脚，镜像地址、Flash 算法也都正确）。请看主目录
`..\README.md` 的 **§10 下载失败排查**，里面写了：

- 从你机器上 J-Link 日志里查到的线索：本工程原来写死了 `-U602722571`（你的 J-Link V9），
  **如果你换用另一个探头（如那个 ARM-OB 克隆版 S/N 20090928）就会下载失败** ——
  本工程已经把这行去掉，并把 SWD 速率从 5 MHz 降到 1 MHz；
- 分步排查（探头识别 → Keil Debug 设置 → U2 排针接线 → U4 是否焊了 → 擦除恢复）；
- 现成工具：主目录里的 `..\jlink_test.bat`（双击即测探头到 MCU 的连接，只读不擦除）。

