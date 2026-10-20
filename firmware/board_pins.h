#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "ti_msp_dl_config.h"

/* ============================================================================
 * 板级引脚定义（由嘉立创EDA工程 type-C口功率计算器放大.eprj2 读出）
 * ----------------------------------------------------------------------------
 * MCU : U4 = MSPM0G3507SPMR  (LQFP-64 / PM 封装, Cortex-M0+, 80MHz)
 * 引脚来源: 立创EDA API 实时读取 PCB 焊盘网络 + 原理图符号引脚名，
 *           并与 TI SysConfig 器件数据 LQFP-64(PM) 物理引脚号逐一核对。
 *
 * 显示屏: 1.9 寸 TFT，驱动 IC = ST7789V，分辨率 320x170（横屏）
 *         模块丝印一般是: GND VCC SCL SDA RES DC CS BLK  （8 脚）
 *         对应接法（走板上原来 OLED 的那组网络，即显示屏座同样可用）:
 *           SCL/SCK  <- C0-SCL    SDA/MOSI <- C0-SDA
 *           RES      <- OLEDRES   DC       <- DC        CS <- CS
 *           BLK      <- 见本文件第二节（⚠ 插座上没有 BLK 脚，必须自己引，
 *                       否则背光不亮 ⇒ 上电全黑）
 *
 * 本板显示屏走线（PCB 实测网络）:
 *   网络 C0-SCL  -> U4 pin39 (PA31) 与 U4 pin44 (PA4)   <== 两个引脚并联！⚠
 *   网络 C0-SDA  -> U4 pin18 (PA22) 与 U4 pin43 (PA3)   <== 两个引脚并联！⚠
 *   网络 OLEDRES -> U4 pin49 (PA7)    网络 DC -> U4 pin59 (PB7)
 *   网络 CS      -> U4 pin58 (PB6)
 *   注：这三个网络的丝印/网名是当年配 0.96" OLED 时取的，接 TFT 时一一对应即可。
 * ==========================================================================*/

/* ---------------------------------------------------------------------------
 * 一、TFT(ST7789) 主用引脚 —— 本工程实际驱动的一组
 * -------------------------------------------------------------------------*/
/* SPI 时钟 SCL/SCK  ->  PA31 / 物理 39 脚 / 网络 C0-SCL */
#define TFT_SCK_PORT          GPIOA
#define TFT_SCK_PIN           DL_GPIO_PIN_31
#define TFT_SCK_IOMUX         (IOMUX_PINCM6)

/* SPI 数据 SDA/MOSI ->  PA22 / 物理 18 脚 / 网络 C0-SDA */
#define TFT_SDA_PORT          GPIOA
#define TFT_SDA_PIN           DL_GPIO_PIN_22
#define TFT_SDA_IOMUX         (IOMUX_PINCM47)

/* TFT 复位 RES     ->  PA7  / 物理 49 脚 / 网络 OLEDRES */
#define TFT_RES_PORT          GPIOA
#define TFT_RES_PIN           DL_GPIO_PIN_7
#define TFT_RES_IOMUX         (IOMUX_PINCM14)

/* TFT 数据/命令 DC ->  PB7  / 物理 59 脚 / 网络 DC */
#define TFT_DC_PORT           GPIOB
#define TFT_DC_PIN            DL_GPIO_PIN_7
#define TFT_DC_IOMUX          (IOMUX_PINCM24)

/* TFT 片选 CS      ->  PB6  / 物理 58 脚 / 网络 CS */
#define TFT_CS_PORT           GPIOB
#define TFT_CS_PIN            DL_GPIO_PIN_6
#define TFT_CS_IOMUX          (IOMUX_PINCM23)

/* ---------------------------------------------------------------------------
 * 二、★背光 BLK —— 本板【必须处理】，否则屏完全正常也是“全黑”
 * ---------------------------------------------------------------------------
 * 板上显示屏座（PCB 位号 OLED1，7 脚插座）实际脚序（从立创EDA网络表读出）：
 *      1 = GND    2 = VCC    3 = C0-SCL   4 = C0-SDA
 *      5 = OLEDRES        6 = DC          7 = CS
 *
 * 而 1.9 寸 TFT 模块是 **8 脚**：在 CS 后面还多一个第 8 脚 **BLK（背光）**，
 * 所以把模块直接插到这个 7 脚座上时 —— **BLK 悬空、背光永远不亮，
 * 结果就是“上电后黑屏”，和程序、和 SPI 一点关系都没有。**
 *
 * 两种接法（任选一种，都是改这里/改线，驱动不用动）：
 *   A) BLK 接 3.3V（或插座第 2 脚 VCC）：背光常亮；此时本宏开着也无害
 *      （PB9 空着，输出高电平不影响任何东西）；
 *   B) BLK 接到空闲 MCU 脚 **PB9 = U4 第 61 脚 = PINCM26**（默认，由程序点亮）。
 *      ⚠ 已用 TI 官方 SysConfig 生成的配置头核对过映射：
 *        “GPIOB.9 with pinCMx 26 on package pin 61”，且本板第 61 脚无网络（悬空）。
 * -------------------------------------------------------------------------*/
#define TFT_BLK_ENABLE                                  /* A) 方案时可注释掉本行 */
#define TFT_BLK_PORT          GPIOB
#define TFT_BLK_PIN           DL_GPIO_PIN_9           /* PB9 / U4 第 61 脚 */
#define TFT_BLK_IOMUX         (IOMUX_PINCM26)

/* ---------------------------------------------------------------------------
 * 三、引脚重合（硬件并联）—— 对应的驱动代码必须注释掉，只留一组输出
 * ---------------------------------------------------------------------------
 * PA4 (物理 44 脚) 与 PA31 同时接在 C0-SCL 上；
 * PA3 (物理 43 脚) 与 PA22 同时接在 C0-SDA 上。
 * 若两个引脚都配成推挽输出，一旦电平相反就是“输出对顶”，会大电流发热甚至
 * 打坏 IO；所以本工程把这一组【只做高阻输入】，绝不去翻转它。
 * 下面的宏保留下来仅用于“换组驱动”时切换，默认被注释掉。
 * -------------------------------------------------------------------------*/
#define TFT_ALT_SCK_PORT      GPIOA
#define TFT_ALT_SCK_PIN       DL_GPIO_PIN_4      /* PA4 / 44 脚 / C0-SCL（重合）*/
#define TFT_ALT_SCK_IOMUX     (IOMUX_PINCM9)

#define TFT_ALT_SDA_PORT      GPIOA
#define TFT_ALT_SDA_PIN       DL_GPIO_PIN_3      /* PA3 / 43 脚 / C0-SDA（重合）*/
#define TFT_ALT_SDA_IOMUX     (IOMUX_PINCM8)

/* ---------------------------------------------------------------------------
 * 四、本板上与“蓝本工程”(empty_LP_MSPM0G3507_nortos_ticlang(okoled)) 冲突的引脚
 *     下列代码在本工程中一律【注释掉/未使用】，原因见每行说明。
 *     蓝本 empty.syscfg 里对应的 Pin Group / 外设也要一并删除，否则
 *     SYSCFG_DL_init() 会把本板显示屏、INA228、按键的引脚抢走。
 * -------------------------------------------------------------------------*/
/* // GPIO_OLED (蓝本软件I2C屏)   -> PA0/PA1 : 本板屏是 SPI 屏，且已换成 ST7789
 * #define GPIO_OLED_PORT        GPIOA
 * #define GPIO_OLED_SDA_PIN     DL_GPIO_PIN_0
 * #define GPIO_OLED_SCL_PIN     DL_GPIO_PIN_1
 */
/* // 06_ui_display 例程的 SPI_LCD = SPI1 -> PB8/PB9 : PB8=NET2按键，且本板屏不在 PB8/PB9 上
 * #define SPI_LCD_SCLK_PIN      DL_GPIO_PIN_9
 * #define SPI_LCD_MOSI_PIN      DL_GPIO_PIN_8
 */
/* // 06_ui_display 例程的 GPIO_LCD 控制脚 -> PB10/PB11/PB14/PB26 : 与接 TFT 的 PB6/PB7 不是一组
 * #define GPIO_LCD_PIN_RES_PIN  DL_GPIO_PIN_10
 * #define GPIO_LCD_PIN_DC_PIN   DL_GPIO_PIN_11
 * #define GPIO_LCD_PIN_CS_PIN   DL_GPIO_PIN_14
 * #define GPIO_LCD_PIN_BLK_PIN  DL_GPIO_PIN_26
 */
/* // GPIO_IMU 片选               -> PB6 : 本板 PB6 = 屏 CS，冲突
 * #define GPIO_IMU_CS_PIN       DL_GPIO_PIN_6
 */
/* // GPIO_KEY S2                 -> PA31 : 本板 PA31 = 屏 SCL，冲突
 * #define GPIO_KEY_S2_PIN       DL_GPIO_PIN_31
 */
/* // GPIO_GRAY OUT8              -> PA22 : 本板 PA22 = 屏 SDA(MOSI)，冲突
 * #define GPIO_GRAY_OUT8_PIN    DL_GPIO_PIN_22
 */
/* // GPIO_HC05 STATE             -> PA16 : 本板 PA16 = INA228 的 I2C SDA，冲突
 * #define GPIO_HC05_HC05_STATE_PIN  DL_GPIO_PIN_16
 */
/* // ENCODER_B LB                -> PB12 : 本板 PB12 = INA228 ALERT，冲突
 * #define ENCODER_B_LB_PIN      DL_GPIO_PIN_12
 */
/* // UART_1 (蓝牙)               -> PB4/PB5 : 本板 PB4=NET3、PB5=NET4 按键，冲突
 * #define GPIO_UART_1_TX_PIN    DL_GPIO_PIN_4
 * #define GPIO_UART_1_RX_PIN    DL_GPIO_PIN_5
 */

/* ---------------------------------------------------------------------------
 * 五、本板其余已知连接（本工程主函数只用屏，下列仅作记录，未配置）
 * ---------------------------------------------------------------------------
 *  INA228 (U1)     : 见下方第六节 —— 本工程【已经使用】，用软件 I²C 读取
 *  按键 SW5~SW9    : PB2=NET1, PB8=NET2, PB4=NET3, PB5=NET4, NRST(RST 脚+SW7)
 *  调试口 U2       : PA19 = SWDIO (12 脚) / PA20 = SWCLK (13 脚)
 *  电源            : pin40 = VDD(VCC) / pin41 = VSS(GND) / pin32 = VCORE(内核 1.35V，接电容)
 *  I2C 上拉        : R2 = 10kΩ 上拉到 VCC (SCL)，R3 = 10kΩ 上拉到 VCC (SDA) —— 板上已有
 *  显示屏座 OLED1  : 1=GND 2=VCC 3=C0-SCL 4=C0-SDA 5=OLEDRES 6=DC 7=CS（7 脚，无 BLK）
 *  排针 H2         : 1=NET1 2=NET2 3=NET3 4=NET4 5=NRST
 * -------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
 * 六、INA228 电源检测芯片（U1，DGS / VSSOP-10）—— 本工程【已使用】
 * ---------------------------------------------------------------------------
 *  符号脚   板上网络    接到 U4              说明
 *  ---------------------------------------------------------------------------
 *   1 A1    GND         —                   地址脚接地
 *   2 A0    GND         —                   地址脚接地
 *                                           ⇒ 7 位 I²C 地址 = 100 0000b = 0x40
 *   3 ALERT ALERT       PB12 / 物理 64 脚    报警输出（本工程用轮询，不配置它）
 *   4 SDA   SDA         PA16 / 物理  9 脚    I²C 数据（软件 I²C）
 *   5 SCL   SCL         PA17 / 物理 10 脚    I²C 时钟（软件 I²C）
 *   6 VS    VCC         3.3V
 *   7 GND   GND         —
 *   8 VBUS  IN+         —                   总线电压输入，接在采样电阻的电源侧
 *   9 IN-   IN-         —                   采样电阻的负载侧
 *  10 IN+   IN+         —                   采样电阻的电源侧
 *
 *  采样电阻 R5 = 10 mΩ，跨接在 IN+ / IN- 之间。
 *  换板/换电阻时只需同步改 ina228.h 里的 INA228_R_SHUNT_MOHM。
 *
 *  I²C 上拉：R2 = 10 kΩ(SCL→VCC)、R3 = 10 kΩ(SDA→VCC)，板上已有，无需外加。
 *  ⚠ 为什么用软件 I²C：PA16 / PA17 确实是 I2C1 的 SDA/SCL，但本工程不重新跑
 *    SysConfig，直接在 ina228.c 里配 IOMUX 更简单，也不占用外设资源；
 *    总线 100 kHz，读一次不到 2 ms，对 200 ms 的显示刷新毫无影响。
 * -------------------------------------------------------------------------*/
#define INA228_SDA_PORT      GPIOA
#define INA228_SDA_PIN       DL_GPIO_PIN_16
#define INA228_SDA_IOMUX     (IOMUX_PINCM38)   /* PA16 / 物理  9 脚 / 网络 SDA */

#define INA228_SCL_PORT      GPIOA
#define INA228_SCL_PIN       DL_GPIO_PIN_17
#define INA228_SCL_IOMUX     (IOMUX_PINCM39)   /* PA17 / 物理 10 脚 / 网络 SCL */

#define INA228_ALERT_PORT    GPIOB
#define INA228_ALERT_PIN     DL_GPIO_PIN_12    /* PB12 / 物理 64 脚（本工程未用） */

#endif /* BOARD_PINS_H */
