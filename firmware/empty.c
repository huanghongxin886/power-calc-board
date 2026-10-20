/* ============================================================================
 *  type-C口功率计算器放大板 —— MSPM0G3507 + 1.9 寸 TFT(ST7789V 320x170)
 *                          + INA228 电源检测芯片
 *  ---------------------------------------------------------------------------
 *  上电后屏幕内容（白底）：
 *
 *      第 0 行(小字): INA228 @0x40 OK   ← 芯片应答情况 / 实际 I²C 地址 / ID 是否正确
 *      第 1 行(大字): U=  12.345 V      ← 总线电压（INA228 的 VBUS 寄存器）
 *      第 2 行(大字): I=   1.234 A      ← 电流（VSHUNT ÷ 采样电阻 R5）
 *      第 3 行(大字): P=  15.200 W      ← 功率（电压 × 电流）
 *      第 4 行(大字): T=      12 s      ← 开机以来的秒数（从 0 开始）
 *
 *  换算全部按 TI INA228 数据手册 ZHCSN47A：
 *      VBUS   : 195.3125 µV/LSB
 *      VSHUNT : 312.5 nV/LSB
 *      I      : VSHUNT ÷ R5,  R5 = 板上采样电阻 = 10 mΩ ⇒ 31.25 µA/LSB
 *      P      : U × I
 *  详见 ina228.h / ina228.c 顶部注释。
 *
 *  主函数流程：SysConfig → 屏初始化 → 毫秒时基 → INA228 初始化 → 循环刷新显示。
 *  蓝本工程里的电机 PWM / 编码器 / IMU / 超声波 / 灰度 / 蓝牙 / 云台(UART)
 *  等代码一律不参与编译（引脚与本板冲突，见 board_pins.h 与 README.md）。
 *
 *  硬件接线（1.9" SPI TFT 模块丝印 -> 本板网络）：
 *    VCC -> VCC(3.3V)   GND -> GND
 *    SCL -> C0-SCL (PA31/39脚)   SDA -> C0-SDA (PA22/18脚)
 *    RES -> OLEDRES(PA7/49脚)    DC  -> DC (PB7/59脚)    CS -> CS (PB6/58脚)
 *    BLK -> 3.3V 或 PB9(U4 第 61 脚)  ← 模块第 8 脚，插在 7 脚座上会悬空 ⇒ 全黑
 *  INA228：SDA = PA16(第 9 脚) / SCL = PA17(第 10 脚)，板上 R2/R3 已有 10 kΩ 上拉
 * ==========================================================================*/

#include "ti_msp_dl_config.h"
#include "hw_lcd.h"
#include "board_pins.h"
#include "ina228.h"

/* ---------------------------------------------------------------------------
 * 一、按毫秒延时（与 hw_lcd.c 同一套写法）
 * -------------------------------------------------------------------------*/
#define delay_ms(X)     delay_cycles( ( (CPUCLK_FREQ/1000u) * (X) ) )

/* 上电自检开关：1 = 上电先跑一遍 红/绿/蓝/白/黑 + “TFT OK”（约 1.5 秒）
 *              （屏完全黑、想分清是硬件还是程序问题时才需要打开；打开时秒表
 *                因为要等自检，会在显示出现时已经走到 1~2 秒）
 *               0 = 跳过自检，上电直接显示测量画面，秒表从 0 开始（默认） */
#define TFT_SELFTEST_ON_BOOT    0

/* ---------------------------------------------------------------------------
 * 二、显示排版
 * -------------------------------------------------------------------------*/
#define FONT_BIG        32      /* 大字：16 像素宽 × 32 高 */
#define FONT_SMALL      16      /* 小字：8 像素宽 × 16 高  */
#define LINE_LEN        12      /* 每行固定 12 个字符 = 192 像素 */
#define LINE_X          64      /* (320 - 192) ÷ 2 = 64，水平居中 */

#define Y_STATUS        4
#define Y_LINE1         26
#define Y_LINE2         62
#define Y_LINE3         98
#define Y_LINE4         134

#define COLOR_V         RED
#define COLOR_I         GREEN
#define COLOR_P         BLUE
#define COLOR_T         BLACK

static const char HEXCH[] = "0123456789ABCDEF";

/* ---------------------------------------------------------------------------
 * 三、1 毫秒时基（SysTick 中断）
 * ---------------------------------------------------------------------------
 * ⚠ 上一版是“轮询 SysTick->VAL 算差值”，这里必须换掉，原因是硬的：
 *   SysTick 是 24 位【递减】计数器，80 MHz 下最多只能数 0x1000000 个周期
 *   （= 209.7 ms）就回绕一次；而本工程主循环一轮 = 200 ms 延时 + 画屏，
 *   实际 ≈ 250~350 ms，【比 SysTick 的回绕周期还长】。
 *   轮询时两次读之间最多只能识别一次回绕，所以每次算出来的其实是
 *   “真实间隔 mod 209.7 ms”—— 秒数会严重偏慢，甚至看着完全不动。
 *
 * 中断方案没有这个问题：计数器一到 0 就进一次中断，中断里 +1 ms，
 * 与主循环快慢完全无关，秒数 = 中断次数 ÷ 1000。
 * 用法与 TI SysConfig 生成的 SYSCFG_DL_SYSTICK_init() 完全一致。
 * -------------------------------------------------------------------------*/
#define TICK_PERIOD     (CPUCLK_FREQ / 1000u)   /* 1 ms = 80000 个周期 */

static volatile uint32_t s_tick_ms = 0;   /* 只在中断里累加 */
static uint8_t           s_tick_ok = 0;   /* 自检：中断有没有真的在跑 */

/* SysTick 中断服务函数（startup_mspm0g350x_uvision.s 里是 [WEAK]，这里覆盖它） */
void SysTick_Handler(void)
{
    s_tick_ms++;
}

static void Tick_Init(void)
{
    uint32_t t0;

    s_tick_ms = 0;

    DL_SYSTICK_init(TICK_PERIOD);       /* 设定周期并清零计数器（此时还没开始数） */
    DL_SYSTICK_enableInterrupt();       /* CTRL.TICKINT = 1 */
    NVIC_SetPriority(SysTick_IRQn, 3);
    DL_SYSTICK_enable();                /* CTRL.ENABLE = 1，开始计数 */

    /* 自检：等 20 ms，看计数有没有往前跑；结果会显示在屏幕右上角 */
    t0 = s_tick_ms;
    delay_cycles((CPUCLK_FREQ / 1000u) * 20u);
    s_tick_ok = (uint8_t)(s_tick_ms != t0);
}

/* ---------------------------------------------------------------------------
 * 四、字符串小工具（不引 string.h，代码量更小）
 * -------------------------------------------------------------------------*/
static void str_copy(char *dst, const char *src, uint8_t max)
{
    uint8_t i = 0;
    while (src[i] != '\0' && i < (uint8_t)(max - 1u)) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static uint8_t str_same(const char *a, const char *b)
{
    uint8_t i = 0;
    while (a[i] != '\0' && a[i] == b[i]) { i++; }
    return (uint8_t)(a[i] == b[i]);
}

/* 把 v 的低 ndigits 位按十六进制写进 dst（高位在前，不写结束符） */
static void hex_put(char *dst, uint32_t v, uint8_t ndigits)
{
    uint8_t k;
    for (k = 0u; k < ndigits; k++) {
        dst[ndigits - 1u - k] = HEXCH[(v >> (4u * k)) & 0x0Fu];
    }
}

/* ---------------------------------------------------------------------------
 * 四之二、顶部诊断行（16 号小字，一行 4 个小字段）
 * ---------------------------------------------------------------------------
 *   D:2281@40   器件 ID + 实际 I²C 地址   （绿 = 制造商 ID 与器件 ID 都对）
 *   V:040CD     VBUS 寄存器的原始 20 位码（十六进制）
 *   S:00000     VSHUNT 寄存器的原始 20 位码
 *   TK:OK       SysTick 自检
 *
 * ★ 这几个原始码就是判断「代码换算问题」还是「芯片/接线问题」的直接证据：
 *     V 的原始码 × 195.3125 µV 必须正好等于 U 行显示的那个电压。
 *     如果原始码本身对应的电压就跟万用表不符，那就是芯片/接线的问题
 *     —— 换算只有一步乘法，不可能算错 23%。
 *   每个字段各自缓存，只在内容变化时才重画，所以不会闪。
 * -------------------------------------------------------------------------*/
static char s_stat[7][10];

static void stat_field(uint8_t slot, uint16_t x, const char *text, uint16_t fc)
{
    if (str_same(s_stat[slot], text)) { return; }
    str_copy(s_stat[slot], text, 10u);
    LCD_ShowString(x, Y_STATUS, (const unsigned char *)text, fc, WHITE, FONT_SMALL, 0);
}

/* 清掉第 1~6 段（"没找到芯片"时用） */
static void stat_clear_1_to_6(void)
{
    uint8_t k;
    for (k = 1u; k < 7u; k++) { stat_field(k, 0u, "", WHITE); }
}

static void status_show(const INA228_Data *d, uint8_t tick_ok)
{
    char s[10];

    if (d->addr == 0u) {
        stat_clear_1_to_6();
        stat_field(0u, 0u, "NO I2C DEV", RED);
        return;                     /* 总线上没人应答，其余字段连同 TK 一起清掉 */
    }

    /* M: 制造商 ID（正常 5449 = "TI"） */
    s[0] = 'M'; s[1] = ':';
    hex_put(&s[2], d->man_id, 4u);
    s[6] = '\0';
    stat_field(0u, 0u, s, (d->man_id == 0x5449u) ? GREEN : RED);

    /* D: 器件 ID → INA228 应为 228x，INA238 应为 238x */
    s[0] = 'D'; s[1] = ':';
    hex_put(&s[2], d->dev_id, 4u);
    s[6] = '\0';
    stat_field(1u, 48u, s, d->id_ok ? GREEN : RED);

    /* I²C 地址 */
    hex_put(s, d->addr, 2u);
    s[2] = '\0';
    stat_field(2u, 96u, s, BLACK);

    /* VBUS 原始码 */
    s[0] = 'V'; s[1] = ':';
    hex_put(&s[2], d->raw_vbus, 5u);
    s[7] = '\0';
    stat_field(3u, 112u, s, BLACK);

    /* VSHUNT 原始码 */
    s[0] = 'S'; s[1] = ':';
    hex_put(&s[2], d->raw_vshunt, 5u);
    s[7] = '\0';
    stat_field(4u, 172u, s, BLACK);

    /* SysTick 自检 */
    s[0] = 'T'; s[1] = 'K'; s[2] = ':';
    s[3] = tick_ok ? 'O' : 'N';
    s[4] = tick_ok ? 'K' : 'G';
    s[5] = '\0';
    stat_field(5u, 232u, s, tick_ok ? GREEN : RED);

    /* 识别出来的型号名（认不出就显示 UNKNOWN），放最右边 */
    stat_field(6u, 272u, INA228_ChipName(), BLACK);
}

/* ---------------------------------------------------------------------------
 * 五、把浮点数排成“固定宽度、右对齐”的字符串
 * ---------------------------------------------------------------------------
 * 例：fmt_fixed(out, 8, 12.345f, 3) -> "  12.345"
 * 小数位数不足补 0；结果永远是 width 个字符，绝不上溢。
 * -------------------------------------------------------------------------*/
static void fmt_fixed(char *out, uint8_t width, float v, uint8_t dec)
{
    char     tmp[24];
    uint8_t  n = 0;
    uint8_t  i, k;
    uint8_t  neg = 0;
    uint32_t scale = 1u;
    uint32_t ip, fp, t;
    float    av;

    for (i = 0; i < dec; i++) { scale *= 10u; }

    if (v < 0.0f) { neg = 1u; av = -v; } else { av = v; }
    t = (uint32_t)((av * (float)scale) + 0.5f);

    ip = t / scale;
    fp = t % scale;

    /* tmp 里按“低位在前”存放，最后整体反转 */
    if (dec > 0u) {
        for (k = dec; k > 0u; k--) {
            uint32_t d = 1u;
            for (i = 1u; i < k; i++) { d *= 10u; }
            tmp[n++] = (char)('0' + (char)((fp / d) % 10u));
        }
        tmp[n++] = '.';
    }
    if (ip == 0u) { tmp[n++] = '0'; }
    while (ip > 0u) {
        tmp[n++] = (char)('0' + (char)(ip % 10u));
        ip /= 10u;
    }
    if (neg) { tmp[n++] = '-'; }

    if (n > width) { n = width; }        /* 极端情况：只保留低位数字 */

    for (i = 0u; i < (uint8_t)(width - n); i++) { out[i] = ' '; }
    for (i = 0u; i < n; i++) { out[(uint8_t)(width - 1u - i)] = tmp[i]; }
    out[width] = '\0';
}

/* ---------------------------------------------------------------------------
 * 六、整行显示（只在内容变化时才重画，避免闪烁、也省 SPI 时间）
 * -------------------------------------------------------------------------*/
static char s_cache[5][LINE_LEN + 1];

static void show_line(uint8_t slot, uint16_t y, const char *text, uint16_t fc)
{
    if (str_same(s_cache[slot], text)) { return; }
    str_copy(s_cache[slot], text, LINE_LEN + 1u);
    LCD_ShowString(LINE_X, y, (const unsigned char *)text, fc, WHITE, FONT_BIG, 0);
}

/* 生成 “U=  12.345 V” 形式的整行（固定 LINE_LEN 个字符） */
static void make_line(char *line, char tag, float v, uint8_t dec, char unit)
{
    line[0]  = tag;
    line[1]  = '=';
    fmt_fixed(&line[2], 8u, v, dec);
    line[10] = ' ';
    line[11] = unit;
    line[12] = '\0';
}

/* ---------------------------------------------------------------------------
 * 七、主函数
 * -------------------------------------------------------------------------*/
int main(void)
{
    INA228_Data d;
    char        line[LINE_LEN + 1];
    uint8_t     i;
    uint32_t    sec;

    /* 1) 系统时钟等基础配置（SysConfig 生成，80 MHz） */
    SYSCFG_DL_init();

    /* 2) 1 ms 时基（SysTick 中断）：计时从这一刻的 0 秒开始。
     *    放在最前面，这样“秒数”是从 MCU 启动算起，而不受后面屏初始化耗时影响。 */
    Tick_Init();

    /* 3) TFT 初始化：先配 GPIO(软件 SPI) 并点亮背光，再做复位与寄存器初始化 */
    lcd_init();

#if TFT_SELFTEST_ON_BOOT
    /* 上电自检：红/绿/蓝/白/黑 → “TFT OK”（判别方法见 hw_lcd.c 的注释） */
    LCD_TestPattern();
    delay_ms(1000);
#endif

    /* 4) 清屏：整屏刷白底 */
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);

    /* 5) INA228 初始化（配引脚 → 自动在 0x40~0x4F 找地址 → 校验器件 ID
     *    → 写 CONFIG/ADC_CONFIG/SHUNT_CAL → 等第一次转换完成） */
    (void)INA228_Init();

    /* 6) 顶部诊断行（器件 ID / 地址 / 原始码 / SysTick 自检） */
    (void)INA228_Read(&d);
    for (i = 0u; i < 7u; i++) { s_stat[i][0] = '\0'; }   /* 清缓存，保证第一帧重画 */
    status_show(&d, s_tick_ok);

    /* 7) 行缓存清空，保证第一帧一定重画 */
    for (i = 0u; i < 5u; i++) { s_cache[i][0] = '\0'; }

    /* 8) 主循环：读 INA228 → 刷新顶部诊断行 + 四行  U / I / P / T */
    while (1)
    {
        (void)INA228_Read(&d);
        sec = s_tick_ms / 1000u;      /* 秒数直接来自 1 ms 中断，与画屏耗时无关 */

        status_show(&d, s_tick_ok);   /* 原始码有变化时才重画，方便和万用表对照 */

        make_line(line, 'U', d.vbus,     3u, 'V');
        show_line(0u, Y_LINE1, line, COLOR_V);

        make_line(line, 'I', d.current,  3u, 'A');
        show_line(1u, Y_LINE2, line, COLOR_I);

        make_line(line, 'P', d.power,    3u, 'W');
        show_line(2u, Y_LINE3, line, COLOR_P);

        make_line(line, 'T', (float)sec, 0u, 's');
        show_line(3u, Y_LINE4, line, COLOR_T);

        delay_ms(200);
    }
}
