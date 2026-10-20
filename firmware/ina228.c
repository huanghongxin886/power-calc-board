/* ============================================================================
 *  ina228.c —— INA228 驱动实现（软件 I²C + 寄存器读写 + 物理量换算）
 *  ---------------------------------------------------------------------------
 *  手册依据：TI INA228 ZHCSN47A
 *    · 7.6.1.1  CONFIG       bit4 = ADCRANGE（0 = ±163.84mV）
 *    · 7.6.1.2  ADC_CONFIG   MODE/DLY/转换时间/平均次数
 *    · 7.6.1.3  SHUNT_CAL    bit14-0，复位 0x1000
 *    · 7.6.1.5  VSHUNT       bit23-4，20 位补码
 *    · 7.6.1.6  VBUS         bit23-4，20 位恒正
 *    · 7.6.1.7  DIETEMP      16 位补码，7.8125 m°C/LSB
 *    · 7.6.1.8  CURRENT      bit23-4，20 位补码 ×CURRENT_LSB
 *    · 8.1.2    公式(2)(3)(4)(5)
 * ==========================================================================*/

#include "ina228.h"

/* ---------------------------------------------------------------------------
 * 一、软件 I²C（开漏模拟）
 * ---------------------------------------------------------------------------
 * I²C 是“线与”总线，只能拉低不能推高。MSPM0 的 GPIO 没法直接切成开漏，
 * 所以这里用“输出使能位 DOE”来模拟：
 *   拉低  = 先把数据寄存器写 0，再使能输出  → 强拉低
 *   释放  = 关掉输出使能                  → 引脚变高阻，由板上 10 kΩ 上拉到 3.3V
 * 这样不会出现“主从同时推高/推低”的对顶，完全符合 I²C 电气规范。
 * -------------------------------------------------------------------------*/
#define SDA_LOW()   do { DL_GPIO_clearPins(INA228_SDA_PORT, INA228_SDA_PIN); \
                         DL_GPIO_enableOutput(INA228_SDA_PORT, INA228_SDA_PIN); } while (0)
#define SDA_REL()   do { DL_GPIO_disableOutput(INA228_SDA_PORT, INA228_SDA_PIN); } while (0)
#define SDA_READ()  ((DL_GPIO_readPins(INA228_SDA_PORT, INA228_SDA_PIN) != 0u) ? 1u : 0u)

#define SCL_LOW()   do { DL_GPIO_clearPins(INA228_SCL_PORT, INA228_SCL_PIN); \
                         DL_GPIO_enableOutput(INA228_SCL_PORT, INA228_SCL_PIN); } while (0)
#define SCL_REL()   do { DL_GPIO_disableOutput(INA228_SCL_PORT, INA228_SCL_PIN); } while (0)

#define I2C_HALF()  delay_cycles(INA228_I2C_HALF_CYCLES)

static uint8_t s_addr   = INA228_I2C_ADDR; /* 初始化时自动探测出来的地址   */
static uint8_t s_id_ok  = 0;               /* 1 = 制造商 ID (0x5449) 通过  */
static uint8_t s_dev_ok = 0;               /* 1 = 器件 ID (0x228x) 通过    */

/* ---------------------------------------------------------------------------
 * ★ 型号自适应
 * ---------------------------------------------------------------------------
 * INA228 不是这一族里唯一的型号，而且【引脚完全一样、寄存器地址也完全一样】，
 * 只有位宽和 LSB 不同，所以插错了照样能通信、数值看着也"像那么回事"：
 *
 *   型号    位宽    VSHUNT LSB   VBUS LSB      DIETEMP      SHUNT_CAL 系数
 *   INA228  20 位   312.5 nV     195.3125 µV   7.8125 m°C   13107.2e6
 *   INA238  16 位   5 µV         3.125 mV      125 m°C      819.2e6
 *   INA237  16 位   5 µV         3.125 mV      125 m°C      819.2e6
 *
 * 有意思的是 16 位那一族的 LSB 正好是 20 位族的 **16 倍**，所以只要按
 * "读 2 字节" 还是 "读 3 字节再右移 4 位" 取对，两族算出来的物理量是完全一致的
 * —— 也就是说这不会让电压显示错，但会让 SHUNT_CAL（芯片内部 CURRENT/POWER）
 * 差 16 倍，必须按型号分开写。
 * -------------------------------------------------------------------------*/
static const char *s_chip_name  = "UNKNOWN";
static uint8_t     s_two_byte   = 0;              /* 1 = 16 位器件，寄存器只有 2 字节 */
static float       s_vbus_lsb   = INA228_VBUS_LSB_V;
static float       s_vshunt_lsb = INA228_VSHUNT_LSB_V;
static uint16_t    s_man_id     = 0;
static uint16_t    s_dev_id     = 0;

static void chip_detect(void)
{
    s_two_byte   = 0;
    s_vbus_lsb   = INA228_VBUS_LSB_V;
    s_vshunt_lsb = INA228_VSHUNT_LSB_V;

    if ((s_dev_id & 0xFFF0u) == 0x2280u) {          /* INA228 / INA229：20 位 */
        s_chip_name = "INA228";
    } else if ((s_dev_id & 0xFFF0u) == 0x2380u) {   /* INA238：16 位          */
        s_chip_name  = "INA238";
        s_two_byte   = 1;
        s_vbus_lsb   = 3.125e-3f;
        s_vshunt_lsb = 5.0e-6f;
    } else if ((s_dev_id & 0xFFF0u) == 0x2370u) {   /* INA237：16 位          */
        s_chip_name  = "INA237";
        s_two_byte   = 1;
        s_vbus_lsb   = 3.125e-3f;
        s_vshunt_lsb = 5.0e-6f;
    } else {
        s_chip_name = "UNKNOWN";                    /* 认不出来：按 INA228 处理 */
    }
}

const char *INA228_ChipName(void) { return s_chip_name; }
uint16_t    INA228_ManId(void)    { return s_man_id; }
uint16_t    INA228_DevId(void)    { return s_dev_id; }
uint8_t     INA228_IsIdOk(void)   { return (uint8_t)(s_id_ok && s_dev_ok); }

/* 软件滑动平均用的环形缓冲（VBUS 与电流各自一份） */
static float   s_flt_v[INA228_FILTER_N];
static float   s_flt_i[INA228_FILTER_N];
static uint8_t s_flt_idx = 0;
static uint8_t s_flt_cnt = 0;

/* 把本次读数压进环形缓冲并算出平均值 */
static void filter_push(float vbus, float cur, float *avg_v, float *avg_i)
{
    uint8_t i;
    float   sv = 0.0f;
    float   si = 0.0f;

    s_flt_v[s_flt_idx] = vbus;
    s_flt_i[s_flt_idx] = cur;
    s_flt_idx++;
    if (s_flt_idx >= INA228_FILTER_N) { s_flt_idx = 0u; }
    if (s_flt_cnt < INA228_FILTER_N)  { s_flt_cnt++; }

    for (i = 0u; i < s_flt_cnt; i++) {
        sv += s_flt_v[i];
        si += s_flt_i[i];
    }
    *avg_v = sv / (float)s_flt_cnt;
    *avg_i = si / (float)s_flt_cnt;
}

/* ---------------------------------------------------------------------------
 * 二、引脚初始化
 * -------------------------------------------------------------------------*/
static void i2c_gpio_init(void)
{
    /* INENA 打开（要回读 SDA 的 ACK），不开内部上下拉（板上已有 10 kΩ） */
    DL_GPIO_initDigitalInputFeatures(INA228_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(INA228_SCL_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    /* 先写 1 再关输出，保证使能输出的瞬间不会误拉低总线 */
    DL_GPIO_setPins(INA228_SDA_PORT, INA228_SDA_PIN);
    DL_GPIO_setPins(INA228_SCL_PORT, INA228_SCL_PIN);
    DL_GPIO_disableOutput(INA228_SDA_PORT, INA228_SDA_PIN);
    DL_GPIO_disableOutput(INA228_SCL_PORT, INA228_SCL_PIN);
}

/* ---------------------------------------------------------------------------
 * 三、I²C 基本时序
 * -------------------------------------------------------------------------*/
static void i2c_start(void)
{
    SDA_REL();
    SCL_REL();
    I2C_HALF();
    SDA_LOW();          /* SCL 高时 SDA 下降沿 = 起始位 */
    I2C_HALF();
    SCL_LOW();
    I2C_HALF();
}

static void i2c_stop(void)
{
    SDA_LOW();
    I2C_HALF();
    SCL_REL();
    I2C_HALF();
    SDA_REL();          /* SCL 高时 SDA 上升沿 = 停止位 */
    I2C_HALF();
}

/* 写 1 字节，返回 0 = 从机 ACK，1 = NACK */
static uint8_t i2c_write_byte(uint8_t b)
{
    uint8_t i, ack;
    for (i = 0; i < 8u; i++) {
        if (b & 0x80u) { SDA_REL(); } else { SDA_LOW(); }
        I2C_HALF();
        SCL_REL();      /* 从机在 SCL 上升沿采样 */
        I2C_HALF();
        SCL_LOW();
        I2C_HALF();
        b = (uint8_t)(b << 1);
    }
    SDA_REL();          /* 第 9 个时钟：主机释放 SDA，读从机 ACK */
    I2C_HALF();
    SCL_REL();
    I2C_HALF();
    ack = SDA_READ();
    SCL_LOW();
    I2C_HALF();
    return ack;
}

/* 读 1 字节；ack = 1 时主机回 ACK（还要继续读），0 时回 NACK（最后一个字节）
 *
 * ★★ 这里曾经有个致命笔误（就是"屏上 4.00 V 而实际 3.24 V"的真正原因）★★
 *   I²C 的约定是：主机把 SDA **拉低** = ACK（我还要继续读）；
 *                主机 **释放** SDA（被上拉成高）= NACK（到此为止）。
 *   原来写成了 `if (ack) SDA_REL(); else SDA_LOW();` —— 极性刚好反了：
 *   读完第 1 个字节就发 NACK，从机立刻交出总线，于是第 2 个字节起全部读成 0xFF。
 *   后果就是：MANUFACTURER_ID 读成 0x54FF（应为 0x5449）、
 *             DEVICE_ID 读成 0x22FF（应为 0x2281）→ ID 校验变红，
 *             VBUS 读成 0x04FFFF（应为 0x040CD0）→ 3.24 V 被算成 4.00 V。
 *   前几个字节"看着差不多"是因为每次都只有最高字节是对的。 */
static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t i, b = 0;
    SDA_REL();
    for (i = 0; i < 8u; i++) {
        I2C_HALF();
        SCL_REL();
        I2C_HALF();
        b = (uint8_t)((b << 1) | SDA_READ());
        SCL_LOW();
        I2C_HALF();
    }
    if (ack) { SDA_LOW(); } else { SDA_REL(); }   /* ACK = 拉低；NACK = 释放 */
    I2C_HALF();
    SCL_REL();          /* 从机在第 9 个 SCL 高电平期间采样这个位 */
    I2C_HALF();
    SCL_LOW();
    I2C_HALF();
    SDA_REL();          /* 放开总线，准备下一个字节 */
    return b;
}

/* 有器件应答该地址吗？返回 1 = 应答 */
static uint8_t i2c_probe(uint8_t addr7)
{
    uint8_t nack;
    i2c_start();
    nack = i2c_write_byte((uint8_t)(addr7 << 1));
    i2c_stop();
    return (uint8_t)(nack == 0u);
}

/* 写寄存器（16 位数据，高字节在前） */
static uint8_t i2c_reg_write(uint8_t addr7, uint8_t reg, uint16_t val)
{
    uint8_t err = 0;
    i2c_start();
    err |= i2c_write_byte((uint8_t)(addr7 << 1));
    err |= i2c_write_byte(reg);
    err |= i2c_write_byte((uint8_t)(val >> 8));
    err |= i2c_write_byte((uint8_t)(val & 0xFFu));
    i2c_stop();
    return err;
}

/* 连续读寄存器 */
static uint8_t i2c_reg_read(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i, err;
    i2c_start();
    err  = i2c_write_byte((uint8_t)(addr7 << 1));   /* 写地址 */
    err |= i2c_write_byte(reg);                     /* 寄存器指针 */
    if (err) { i2c_stop(); return 1u; }

    i2c_start();                                    /* 重复起始位 */
    err = i2c_write_byte((uint8_t)((addr7 << 1) | 1u));  /* 读地址 */
    if (err) { i2c_stop(); return 1u; }

    for (i = 0; i < len; i++) {
        buf[i] = i2c_read_byte((uint8_t)(((uint16_t)i + 1u) < (uint16_t)len ? 1u : 0u));
    }
    i2c_stop();
    return 0u;
}

/* ---------------------------------------------------------------------------
 * 四、寄存器封装
 * -------------------------------------------------------------------------*/
/* 指定地址读 16 位（扫描阶段用，因为那时 s_addr 还没定下来） */
static uint16_t read16_at(uint8_t addr7, uint8_t reg)
{
    uint8_t b[2] = {0u, 0u};
    (void)i2c_reg_read(addr7, reg, b, 2u);
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

uint16_t INA228_ReadReg16(uint8_t reg)
{
    return read16_at(s_addr, reg);
}

uint32_t INA228_ReadReg24(uint8_t reg, uint8_t *ok)
{
    uint8_t b[3] = {0u, 0u, 0u};
    uint8_t e = i2c_reg_read(s_addr, reg, b, 3u);
    if (ok) { *ok = (uint8_t)(e == 0u); }
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}

void INA228_WriteReg(uint8_t reg, uint16_t val)
{
    (void)i2c_reg_write(s_addr, reg, val);
}

/* 按型号读一个"数据码"：
 *   16 位器件（INA237/238）→ 直接读 2 字节，就是它的 16 位原码
 *   20 位器件（INA228/229）→ 读 3 字节，数值在 bit23-4，右移 4 位
 * 这样两族拿到的都是"寄存器里那个数"，再乘各自的 LSB 即可。 */
static uint32_t read_code(uint8_t reg, uint8_t *ok)
{
    uint8_t b[3] = {0u, 0u, 0u};
    uint8_t e;

    if (s_two_byte) {
        e = i2c_reg_read(s_addr, reg, b, 2u);
        if (ok) { *ok = (uint8_t)(e == 0u); }
        return ((uint32_t)b[0] << 8) | (uint32_t)b[1];
    }
    e = i2c_reg_read(s_addr, reg, b, 3u);
    if (ok) { *ok = (uint8_t)(e == 0u); }
    return (((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2]) >> 4;
}

/* 数据码 → 有符号（VSHUNT / CURRENT 用），按型号决定符号位在哪一位 */
static int32_t sign_ext(uint32_t code)
{
    if (s_two_byte) {
        return (int32_t)(int16_t)code;          /* 16 位补码 */
    }
    if (code & 0x80000u) {                       /* 20 位补码 */
        return (int32_t)code - 0x100000L;
    }
    return (int32_t)code;
}

/* ---------------------------------------------------------------------------
 * 五、等待一次完整转换结束
 * ---------------------------------------------------------------------------
 * DIAG_ALRT (0Bh) 的 bit1 = CNVRF（转换就绪标志，数据手册 7.6.1.12 表 7-16）。
 * 每次完整转换结束时由硬件置 1，【读取该寄存器时清 0】。
 * 所以先读一次把标志清掉，再轮询等它变 1，就能确保后面读到的 VBUS / VSHUNT
 * 是同一批刚转换完的新数据，而不是"读到一半被刷新"或"读到上一批"。
 * 超时（timeout_ms 毫秒）就直接返回，绝不把主循环卡死。
 * -------------------------------------------------------------------------*/
static void wait_conv_ready(uint32_t timeout_ms)
{
    uint32_t t = 0u;

    (void)read16_at(s_addr, INA228_REG_DIAG_ALRT);      /* 先清 CNVRF */
    while (t < timeout_ms) {
        if ((read16_at(s_addr, INA228_REG_DIAG_ALRT) & 0x0002u) != 0u) {
            return;                                     /* 这批数据已经就绪 */
        }
        delay_cycles(CPUCLK_FREQ / 1000u);              /* 1 ms */
        t++;
    }
}

/* 原始码 → 电压（对外公开，方便在屏幕上直接显示原始值做对比） */
float INA228_RawToVolt(uint32_t raw_code)
{
    return (float)raw_code * s_vbus_lsb;
}

/* ---------------------------------------------------------------------------
 * 五、初始化
 * -------------------------------------------------------------------------*/
uint8_t INA228_Init(void)
{
    uint8_t a;
    uint16_t man;
    uint16_t dev;
    uint16_t shunt_cal;

    i2c_gpio_init();

    /* INA228 上电到 I²C 可用只需几百 µs，这里留 5 ms 余量 */
    delay_cycles((CPUCLK_FREQ / 1000u) * 5u);

    /* ---- 1) 在 0x40~0x4F 里找芯片（A1/A0 的接法决定地址） ----
     * ★ 只核对 MANUFACTURER_ID 是不够的：它在 INA226 等 TI 器件上同样是 0x5449，
     *   认不出型号。必须再核对 DEVICE_ID (0x3F) 的 DIEID 是不是 0x228。
     *   这一步就是用来回答“读数是芯片问题还是代码问题”的第一个证据。 */
    s_addr   = 0u;
    s_id_ok  = 0u;
    s_dev_ok = 0u;
    s_man_id = 0u;
    s_dev_id = 0u;
    for (a = 0x40u; a <= 0x4Fu; a++) {
        if (!i2c_probe(a)) { continue; }
        /* 谁先应答先记下来，保证“有器件但 ID 不对”时也能继续读 */
        if (s_addr == 0u) {
            s_addr   = a;
            s_man_id = read16_at(a, INA228_REG_MANUFACTURER);
            s_dev_id = read16_at(a, INA228_REG_DEVICE_ID);
        }
        man = read16_at(a, INA228_REG_MANUFACTURER);
        dev = read16_at(a, INA228_REG_DEVICE_ID);
        s_id_ok  = (uint8_t)(man == 0x5449u);              /* "TI"   */
        s_dev_ok = (uint8_t)((dev & 0xFFF0u) == 0x2280u);  /* 0x228x */
        if (s_id_ok && s_dev_ok) {
            s_addr   = a;
            s_man_id = man;
            s_dev_id = dev;
            break;
        }
    }
    if (s_addr == 0u) {
        return 0u;                      /* 总线上没有任何器件 */
    }

    /* ---- 1.5) 按 DEVICE_ID 识别型号，决定位宽和 LSB ---- */
    chip_detect();

    /* ---- 2) CONFIG：ADCRANGE = 0（±163.84 mV），其余保持复位值 ---- */
    INA228_WriteReg(INA228_REG_CONFIG, 0x0000u);

    /* ---- 3) ADC_CONFIG：连续转换 总线电压+分流电压+温度 ----
     *   MODE   = Fh  → 连续测量 VBUS / VSHUNT / 温度
     *   VBUSCT / VSHCT / VTCT = 4h = 540 µs
     *   AVG    = 3h  → 每次平均 64 个采样
     *   ——按手册表 8-2，540 µs + 64 次平均时“无噪声 ENOB”最高（17.7 位）。
     *     一次完整刷新 ≈ 3 × 540 µs × 64 ≈ 104 ms，仍快于 200 ms 的显示周期。
     *     （原来用的是 1052 µs + 16 次平均 = 50 ms，噪声约为本配置的 4 倍） */
    INA228_WriteReg(INA228_REG_ADC_CONFIG,
                    (uint16_t)(0xF000u | (4u << 9) | (4u << 6) | (4u << 3) | 3u));

    /* ---- 4) SHUNT_CAL：手册公式(2)
     *        INA228/229：SHUNT_CAL = 13107.2e6 × CURRENT_LSB × R_SHUNT
     *        INA237/238：SHUNT_CAL =   819.2e6 × CURRENT_LSB × R_SHUNT   ← 系数不同！
     *      （ADCRANGE = 1 时要再乘 4，本工程用 ADCRANGE = 0 所以不乘）
     *      本板 10 mΩ 代入，两个型号都正好得 4096 = 复位值 0x1000。
     *      这个寄存器只影响芯片内部的 CURRENT / POWER / ENERGY 寄存器；
     *      本工程显示用的电流是直接用 VSHUNT ÷ R5 算的，不依赖它。 */
    {
        float k       = s_two_byte ? 819.2e6f : 13107.2e6f;
        float cur_lsb = s_vshunt_lsb / INA228_SHUNT_OHM;
        shunt_cal = (uint16_t)(k * cur_lsb * INA228_SHUNT_OHM + 0.5f);
    }
    INA228_WriteReg(INA228_REG_SHUNT_CAL, shunt_cal);

    /* ---- 5) 等第一次完整转换：改成等 CNVRF 标志，不再死等固定时间 ---- */
    wait_conv_ready(500u);

    return (uint8_t)(s_id_ok && s_dev_ok);
}

/* ---------------------------------------------------------------------------
 * 六、读一次数据
 * -------------------------------------------------------------------------*/
uint8_t INA228_Read(INA228_Data *d)
{
    uint8_t  ok1, ok2, ok3;
    float    avg_v, avg_i;

    if (!d) { return 0u; }

    /* 初始化时没找到任何器件：直接返回全 0，不要拿地址 0 去总线上乱发 */
    if (s_addr == 0u) {
        d->vbus      = 0.0f;
        d->vshunt    = 0.0f;
        d->current   = 0.0f;
        d->power     = 0.0f;
        d->temp      = 0.0f;
        d->raw_vbus  = 0u;
        d->raw_vshunt = 0u;
        d->addr      = 0u;
        d->man_id    = 0u;
        d->dev_id    = 0u;
        d->id_ok     = 0u;
        d->ok        = 0u;
        return 0u;
    }

    /* 0) 先等芯片把这一批（VBUS + VSHUNT + 温度）转换完，避免读到"半新半旧"的数据 */
    wait_conv_ready(400u);

    /* 总线电压：按型号用 16 位或 20 位原码，再乘对应 LSB */
    d->raw_vbus = read_code(INA228_REG_VBUS, &ok1);
    d->vbus     = (float)d->raw_vbus * s_vbus_lsb;

    /* 分流电压：同样按型号取原码，再按型号做符号扩展 */
    d->raw_vshunt = read_code(INA228_REG_VSHUNT, &ok2);
    d->vshunt     = (float)sign_ext(d->raw_vshunt) * s_vshunt_lsb;

    /* 电流 = 分流电压 ÷ 采样电阻（等效于手册公式(4) 的 CURRENT × CURRENT_LSB） */
    d->current = d->vshunt / INA228_SHUNT_OHM;

    /* 软件滑动平均：芯片内部已经平均 64 次，这里再对最近 N 次结果求平均，
     * 显示值会更安静；功率用平均值相乘，保证 U/I/P 三者自洽 */
    filter_push(d->vbus, d->current, &avg_v, &avg_i);
    d->vbus    = avg_v;
    d->current = avg_i;
    d->power   = avg_v * avg_i;

    /* 芯片温度：两族的寄存器都读 2 字节，且"每个原码 = 7.8125 m°C"
     * （INA228 是 16 位 × 7.8125 m°C；INA238 是 bit15-4 的 12 位 × 125 m°C，
     *   125 ÷ 16 = 7.8125，结果完全一致） */
    d->temp = (float)((int16_t)INA228_ReadReg16(INA228_REG_DIETEMP)) * INA228_TEMP_LSB_C;

    /* 顺带把芯片自己算的 CURRENT 寄存器读出来做交叉验证（不参与显示换算） */
    (void)read_code(INA228_REG_CURRENT, &ok3);

    d->addr   = s_addr;
    d->man_id = s_man_id;
    d->dev_id = s_dev_id;
    d->id_ok  = (uint8_t)(s_id_ok && s_dev_ok);
    d->ok     = (uint8_t)(ok1 && ok2 && ok3);

    return d->ok;
}
