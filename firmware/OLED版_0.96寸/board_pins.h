#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "ti_msp_dl_config.h"

/* ============================================================================
 * 板级引脚定义（0.96 寸 OLED 版）—— 由嘉立创EDA工程
 * type-C口功率计算器放大.eprj2 读出
 * ----------------------------------------------------------------------------
 * MCU : U4 = MSPM0G3507SPMR  (LQFP-64 / PM 封装, Cortex-M0+, 80MHz)
 * 引脚来源: 立创EDA API 实时读取 PCB 焊盘网络 + 原理图符号引脚名，
 *           并与 TI SysConfig 器件数据 LQFP-64(PM) 物理引脚号逐一核对。
 *
 * OLED: OLED1 = HS96L01W4S03 (汉昇 0.96" 128x64, 7 脚 4 线 SPI 模块)
 *       模块引脚: 1=GND 2=VCC 3=SCL(D0/SCK) 4=SDA(D1/MOSI) 5=RES 6=DC 7=CS
 *       手册 1.5 节 Pin Definition；3.3.1 节 SPI 时序(最大约 10MHz)
 *
 * 本板 OLED 走线（PCB 实测网络）:
 *   网络 C0-SCL  -> U4 pin39 (PA31) 与 U4 pin44 (PA4)   <== 两个引脚并联！⚠
 *   网络 C0-SDA  -> U4 pin18 (PA22) 与 U4 pin43 (PA3)   <== 两个引脚并联！⚠
 *   网络 OLEDRES -> U4 pin49 (PA7)
 *   网络 DC      -> U4 pin59 (PB7)
 *   网络 CS      -> U4 pin58 (PB6)
 * ==========================================================================*/

/* ---------------------------------------------------------------------------
 * 一、OLED 主用引脚（本工程实际驱动的一组）
 * -------------------------------------------------------------------------*/
/* SPI 时钟 SCL/SCK  ->  PA31 / 物理 39 脚 / 网络 C0-SCL */
#define OLED_SCLK_PORT        GPIOA
#define OLED_SCLK_PIN         DL_GPIO_PIN_31
#define OLED_SCLK_IOMUX       (IOMUX_PINCM6)

/* SPI 数据 SDA/MOSI ->  PA22 / 物理 18 脚 / 网络 C0-SDA */
#define OLED_MOSI_PORT        GPIOA
#define OLED_MOSI_PIN         DL_GPIO_PIN_22
#define OLED_MOSI_IOMUX       (IOMUX_PINCM47)

/* OLED 复位 RES     ->  PA7  / 物理 49 脚 / 网络 OLEDRES */
#define OLED_RES_PORT         GPIOA
#define OLED_RES_PIN          DL_GPIO_PIN_7
#define OLED_RES_IOMUX        (IOMUX_PINCM14)

/* OLED 数据/命令 DC ->  PB7  / 物理 59 脚 / 网络 DC */
#define OLED_DC_PORT          GPIOB
#define OLED_DC_PIN           DL_GPIO_PIN_7
#define OLED_DC_IOMUX         (IOMUX_PINCM24)

/* OLED 片选 CS      ->  PB6  / 物理 58 脚 / 网络 CS */
#define OLED_CS_PORT          GPIOB
#define OLED_CS_PIN           DL_GPIO_PIN_6
#define OLED_CS_IOMUX         (IOMUX_PINCM23)

/* ---------------------------------------------------------------------------
 * 二、引脚重合（硬件并联）—— 对应的驱动代码必须注释掉，只留一组输出
 * ---------------------------------------------------------------------------
 * PA4 (物理 44 脚) 与 PA31 同时接在 C0-SCL 上；
 * PA3 (物理 43 脚) 与 PA22 同时接在 C0-SDA 上。
 * 若两个引脚都配成推挽输出，一旦电平相反就是“输出对顶”，会大电流发热甚至
 * 打坏 IO；所以本工程把这一组【只做高阻输入】，绝不去翻转它。
 * 下面的宏保留下来仅用于“换组驱动”时切换，默认被注释掉。
 * -------------------------------------------------------------------------*/
#define OLED_ALT_SCLK_PORT    GPIOA
#define OLED_ALT_SCLK_PIN     DL_GPIO_PIN_4      /* PA4 / 44 脚 / C0-SCL（重合）*/
#define OLED_ALT_SCLK_IOMUX   (IOMUX_PINCM9)

#define OLED_ALT_MOSI_PORT    GPIOA
#define OLED_ALT_MOSI_PIN     DL_GPIO_PIN_3      /* PA3 / 43 脚 / C0-SDA（重合）*/
#define OLED_ALT_MOSI_IOMUX   (IOMUX_PINCM8)

/* ---------------------------------------------------------------------------
 * 三、本板上与“蓝本工程”(empty_LP_MSPM0G3507_nortos_ticlang(okoled)) 冲突的引脚
 *     下列代码在本工程中一律【注释掉/未使用】，原因见每行说明。
 *     蓝本 empty.syscfg 里对应的 Pin Group / 外设也要一并删除，否则
 *     SYSCFG_DL_init() 会把本板 OLED、INA228、按键的引脚抢走。
 * -------------------------------------------------------------------------*/
/* // GPIO_OLED (蓝本软件I2C屏)   -> PA0/PA1 : 本板 OLED 不是 I2C 屏，改用软件 SPI
 * #define GPIO_OLED_PORT        GPIOA
 * #define GPIO_OLED_SDA_PIN     DL_GPIO_PIN_0
 * #define GPIO_OLED_SCL_PIN     DL_GPIO_PIN_1
 */
/* // SPI_0 = SPI1 (蓝本 IMU 用)  -> PB7/PB8/PB9 : PB7=OLED DC、PB8=NET2按键，冲突
 * #define GPIO_SPI_0_PICO_PIN   DL_GPIO_PIN_8
 * #define GPIO_SPI_0_POCI_PIN   DL_GPIO_PIN_7
 * #define GPIO_SPI_0_SCLK_PIN   DL_GPIO_PIN_9
 */
/* // GPIO_IMU 片选               -> PB6 : 本板 PB6 = OLED CS，冲突
 * #define GPIO_IMU_CS_PIN       DL_GPIO_PIN_6
 */
/* // GPIO_KEY S2                 -> PA31 : 本板 PA31 = OLED SCL，冲突
 * #define GPIO_KEY_S2_PIN       DL_GPIO_PIN_31
 */
/* // GPIO_GRAY OUT8              -> PA22 : 本板 PA22 = OLED SDA(MOSI)，冲突
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
 * 四、本板其余已知连接（本工程主函数只用 OLED，下列仅作记录，未配置）
 * ---------------------------------------------------------------------------
 *  INA228 (U1) I2C : PA16 = SDA / 物理 9 脚 / 网络 SDA   (I2C1_SDA 功能)
 *                    PA17 = SCL / 物理 10 脚 / 网络 SCL  (I2C1_SCL 功能)
 *                    PB12 = ALERT / 物理 64 脚 / 网络 ALERT
 *  按键 SW5~SW9    : PB2=NET1, PB8=NET2, PB4=NET3, PB5=NET4, NRST(RST 脚+SW7)
 *  调试口 U2       : PA19 = SWDIO (12 脚) / PA20 = SWCLK (13 脚)
 *  电源            : pin40 = VDD(VCC) / pin41 = VSS(GND) / pin32 = VCORE(内核 1.35V，接电容)
 *  I2C 上拉        : R2 = 10kΩ 上拉到 VCC (SCL)，R3 = 10kΩ 上拉到 VCC (SDA) —— 板上已有
 *  排针 H1         : 1=CS 2=DC 3=OLEDRES 4=C0-SDA 5=C0-SCL 6=VCC 7=GND
 *  排针 H2         : 1=NET1 2=NET2 3=NET3 4=NET4 5=NRST
 * -------------------------------------------------------------------------*/

#endif /* BOARD_PINS_H */
