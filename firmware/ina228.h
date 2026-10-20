/* ============================================================================
 *  ina228.h —— 电源检测芯片 INA228 驱动（MSPM0G3507 / 本板）
 *  ---------------------------------------------------------------------------
 *  依据：TI INA228 数据手册 ZHCSN47A（2021-01 / 2022-05 修订，中文版）
 *        已按手册逐条核对：寄存器地址、位域、换算系数、复位值。
 *
 *  板上连接（由嘉立创EDA 工程 type-C口功率计算器放大.eprj2 的 PCB 网络读出）：
 *    U1 = INA228（DGS / VSSOP-10 封装）
 *      1 脚 A1     -> GND          ┐ 地址脚都接地
 *      2 脚 A0     -> GND          ┘ ⇒ I²C 地址 = 100 0000b = 0x40
 *      3 脚 ALERT  -> PB12 / U4 第 64 脚（本工程不用中断，靠轮询）
 *      4 脚 SDA    -> PA16 / U4 第  9 脚（网络 SDA）
 *      5 脚 SCL    -> PA17 / U4 第 10 脚（网络 SCL）
 *      6 脚 VS     -> VCC 3.3V
 *      7 脚 GND    -> GND
 *      8 脚 VBUS   -> 网络 IN+（本板把总线电压输入接在采样电阻的电源侧）
 *      9 脚 IN-    -> 网络 IN-（去负载侧的 Type-C VBUS）
 *     10 脚 IN+    -> 网络 IN+（电源侧 Type-C VBUS）
 *    R5 = 10 mΩ 采样电阻，跨接在 IN+ / IN- 之间
 *    R2/R3 = 10 kΩ I²C 上拉（板上已有，不用外加）
 *
 *  ★ 为什么用软件 I²C：
 *    PA16 / PA17 虽然有 I2C1 复用功能，但本工程不重新生成 SysConfig，
 *    在自己文件里直接配 IOMUX 更省事、也不会和显示屏的软件 SPI 抢资源。
 *    总线速度 ~100 kHz，一次读取不到 2 ms，对显示毫无影响。
 * ==========================================================================*/
#ifndef INA228_H
#define INA228_H

#include "ti_msp_dl_config.h"

/* ===========================================================================
 * 一、用户参数（换板/换采样电阻只改这 3 行）
 * =========================================================================*/
/* I²C 7 位地址。A1=A0=GND 时为 0x40；I2C_Init() 会自动在 0x40~0x4F 里找，
 * 所以即使地址脚接法和我读到的不一样也能自己认出来。 */
#define INA228_I2C_ADDR         0x40u

/* ★ R5：IN+/IN- 之间的采样电阻，单位 mΩ。
 *   满量程配合（ADCRANGE = 0，±163.84 mV）：
 *      可测最大电流 = 163.84 mV / R5 = 16.384 A   (R5 = 10 mΩ)
 *      电流分辨率   = VSHUNT LSB / R5 = 312.5 nV / 10 mΩ = 31.25 µA
 *   换成别的电阻时这个宏一改，下面的系数、SHUNT_CAL 全部自动跟着变。 */
#define INA228_R_SHUNT_MOHM     10.0f

/* I²C 半周期（CPU 周期数）：5 µs → ~100 kHz。
 * 板上上拉只有 10 kΩ，别为了快而缩短；线长/干扰大时可以再放大。 */
#define INA228_I2C_HALF_CYCLES  (CPUCLK_FREQ / 200000u)

/* 软件滑动平均深度：每次读回来的 VBUS / 电流先放进环形缓冲，对外给的是 N 次平均。
 * 芯片内部已经做了 64 次硬件平均，再叠这一层，显示会更稳（N 越大越稳、反应越慢）。 */
#define INA228_FILTER_N         4u

/* ===========================================================================
 * 二、引脚（PA16 = SDA / PA17 = SCL，见 board_pins.h 第六节）
 * =========================================================================*/
#define INA228_SDA_PORT         GPIOA
#define INA228_SDA_PIN          DL_GPIO_PIN_16
#define INA228_SDA_IOMUX        (IOMUX_PINCM38)   /* PA16 = PINCM38 */

#define INA228_SCL_PORT         GPIOA
#define INA228_SCL_PIN          DL_GPIO_PIN_17
#define INA228_SCL_IOMUX        (IOMUX_PINCM39)   /* PA17 = PINCM39 */

/* ===========================================================================
 * 三、寄存器地址（数据手册 7.6 节寄存器汇总表）
 * =========================================================================*/
#define INA228_REG_CONFIG        0x00u  /* 16 位：RST/RSTACC/CONVDLY/TEMPCOMP/ADCRANGE */
#define INA228_REG_ADC_CONFIG    0x01u  /* 16 位：MODE/VBUSCT/VSHCT/VTCT/AVG          */
#define INA228_REG_SHUNT_CAL     0x02u  /* 16 位，只有 bit14-0 有效，复位 0x1000     */
#define INA228_REG_SHUNT_TEMPCO  0x03u
#define INA228_REG_VSHUNT        0x04u  /* 24 位，数值在 bit23-4（20 位补码）        */
#define INA228_REG_VBUS          0x05u  /* 24 位，数值在 bit23-4（20 位，恒正）      */
#define INA228_REG_DIETEMP       0x06u  /* 16 位补码，7.8125 m°C/LSB                 */
#define INA228_REG_CURRENT       0x07u  /* 24 位，数值在 bit23-4（由 SHUNT_CAL 决定）*/
#define INA228_REG_POWER         0x08u  /* 24 位无符号，LSB = 3.2 × CURRENT_LSB      */
#define INA228_REG_ENERGY        0x09u  /* 40 位 */
#define INA228_REG_CHARGE        0x0Au  /* 40 位 */
#define INA228_REG_DIAG_ALRT     0x0Bu  /* 16 位：bit1 = 转换就绪标志 CNVRF          */
#define INA228_REG_MANUFACTURER  0x3Eu  /* 复位值 0x5449 = ASCII "TI"                */
#define INA228_REG_DEVICE_ID     0x3Fu  /* 复位值 0x2281（DIEID=0x228, REV=1）       */

/* ===========================================================================
 * 四、换算系数（数据手册表 8-1 + 8.1.2 节公式）
 * =========================================================================*/
#define INA228_VSHUNT_LSB_V     (312.5e-9f)     /* ADCRANGE=0：312.5 nV/LSB  */
#define INA228_VBUS_LSB_V       (195.3125e-6f)  /* 195.3125 µV/LSB           */
#define INA228_TEMP_LSB_C       (7.8125e-3f)    /* 7.8125 m°C/LSB            */

#define INA228_SHUNT_OHM        (INA228_R_SHUNT_MOHM / 1000.0f)
/* 手册公式(3)：CURRENT_LSB = 最大预期电流 / 2^19。
 * 本板直接让 VSHUNT 满量程对应满量程电流，于是
 *   CURRENT_LSB = VSHUNT_LSB / R_SHUNT = 3.125e-7 / 0.01 = 31.25 µA
 * 再按公式(2)：SHUNT_CAL = 13107.2e6 × CURRENT_LSB × R_SHUNT = 4096 */
#define INA228_CURRENT_LSB_A    (INA228_VSHUNT_LSB_V / INA228_SHUNT_OHM)

/* ===========================================================================
 * 五、对外结构体与函数
 * =========================================================================*/
typedef struct {
    float    vbus;       /* 总线电压 [V]（已滑动平均）           */
    float    vshunt;     /* 采样电阻压差 [V]（带符号，已平均）    */
    float    current;    /* 电流 [A] = vshunt / R_SHUNT（已平均） */
    float    power;      /* 功率 [W] = vbus × current            */
    float    temp;       /* 芯片结温 [°C]                        */
    uint32_t raw_vbus;   /* ★ VBUS 寄存器的原始 24 位值（诊断用） */
    uint32_t raw_vshunt; /* ★ VSHUNT 寄存器的原始 24 位值        */
    uint8_t  addr;       /* 实际应答的 I²C 地址                  */
    uint16_t man_id;     /* 制造商 ID，应为 0x5449 ("TI")        */
    uint16_t dev_id;     /* 器件 ID，应为 0x2281                 */
    uint8_t  id_ok;      /* 1 = 制造商 ID + 器件 ID 都匹配      */
    uint8_t  ok;         /* 1 = 本次所有读取都收到 ACK           */
} INA228_Data;

/* 初始化：配置引脚 → 找地址 → 读 MFG_ID/DEVICE_ID 识别型号 → 写 CONFIG/ADC_CONFIG/SHUNT_CAL
 * 返回 1 = 找到 INA228 且 ID 校验通过；0 = 没找到或不是 INA228 */
uint8_t  INA228_Init(void);

/* 诊断用：识别出来的型号名（"INA228" / "INA238" / "INA237" / "UNKNOWN"）
 * 以及芯片回的原始 ID 值，方便直接显示到屏幕上判断"是不是这颗芯片" */
const char *INA228_ChipName(void);
uint16_t    INA228_ManId(void);
uint16_t    INA228_DevId(void);
uint8_t     INA228_IsIdOk(void);

/* 读一次全部数据（会先等芯片的“转换完成”标志 CNVRF，再做滑动平均）；
 * 返回 1 = 本次读取成功（全部 ACK） */
uint8_t  INA228_Read(INA228_Data *d);

/* 诊断用：把原始寄存器值换算成电压，用来判断“是芯片报的数不对，还是换算不对”
 *   raw = (INA228_ReadReg24(...) >> 4) 的结果 */
float    INA228_RawToVolt(uint32_t raw20);

/* 底层寄存器访问（调试用） */
uint16_t INA228_ReadReg16(uint8_t reg);
uint32_t INA228_ReadReg24(uint8_t reg, uint8_t *ok);
void     INA228_WriteReg(uint8_t reg, uint16_t val);

#endif /* INA228_H */
