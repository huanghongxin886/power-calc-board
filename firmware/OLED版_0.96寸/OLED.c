/***************************************************************************************
  * MSPM0G3507 —— 0.96" OLED (SSD1306, 7 脚 4 线 SPI 模块) 软件 SPI 驱动
  * ------------------------------------------------------------------------------------
  * 移植自蓝本工程 user/OLED.c (江协科技风格驱动)，仅把底层“软件 I2C”换成本板需要的
  * “软件 4 线 SPI”，上层显存管理 / 字符显示 API 全部保持一致。
  *
  * 为什么必须是软件 SPI？
  *   本板 OLED 的 SCK/MOSI 接在 PA31(39脚) 与 PA22(18脚) 上，查 TI MSPM0G3507
  *   IOMUX 表：这两个引脚【没有任何 SPI 外设功能】，做不了硬件 SPI SCK/MOSI。
  *   并联的另一组 PA4(44脚)/PA3(43脚) 虽然有 SPI0 功能，但分别是 SPI0_POCI(MISO)
  *   与 SPI0_CS1_POCI1，同样不是 MOSI。=> 只能用 GPIO 翻转模拟 SPI。
  *
  * 引脚（详见 board_pins.h）：
  *   SCLK = PA31(39)   MOSI = PA22(18)   RES = PA7(49)   DC = PB7(59)   CS = PB6(58)
  *   ⚠ PA4(44)/PA3(43) 与 SCLK/MOSI 硬件并联，本驱动把它们置为高阻输入，禁止翻转。
  *
  * WARNING: 本驱动使用软件 GPIO 模拟 SPI，不可重入；多任务/中断里调用需自行加锁。
  ***************************************************************************************/

#include "OLED.h"
#include "board_pins.h"
#include "Delay.h"
#include <stdio.h>
#include <stdarg.h>

/* 显存 8 页 x 128 列，1 个 bit 对应 1 个像素 */
uint8_t OLED_DisplayBuf[8][128];

/* SPI 位延时。手册 3.3.1：SCLK 最小时钟周期 100ns(≈10MHz)，数据建立 25ns/保持 30ns。
 * 这里 1us 非常保守（≈250kHz），改成 0 可提速到几百 kHz~1MHz 量级。 */
#define OLED_SPI_DELAY_US   1


/* =========================================================================
 * 1. 硬件引脚抽象层（软件 4 线 SPI）
 * ========================================================================= */

/**
  * @brief  OLED 写 SCLK 电平（SPI 时钟）
  */
static void OLED_W_SCLK(uint8_t BitValue)
{
    if (BitValue) {
        DL_GPIO_setPins(OLED_SCLK_PORT, OLED_SCLK_PIN);
    } else {
        DL_GPIO_clearPins(OLED_SCLK_PORT, OLED_SCLK_PIN);
    }
    Delay_us(OLED_SPI_DELAY_US);
}

/**
  * @brief  OLED 写 MOSI/SDA 电平（SPI 数据，单向输出，模块不回读）
  */
static void OLED_W_MOSI(uint8_t BitValue)
{
    if (BitValue) {
        DL_GPIO_setPins(OLED_MOSI_PORT, OLED_MOSI_PIN);
    } else {
        DL_GPIO_clearPins(OLED_MOSI_PORT, OLED_MOSI_PIN);
    }
    Delay_us(OLED_SPI_DELAY_US);
}

/**
  * @brief  OLED 写 RES 复位电平（低有效）
  */
static void OLED_W_RES(uint8_t BitValue)
{
    if (BitValue) {
        DL_GPIO_setPins(OLED_RES_PORT, OLED_RES_PIN);
    } else {
        DL_GPIO_clearPins(OLED_RES_PORT, OLED_RES_PIN);
    }
}

/**
  * @brief  OLED 写 DC 电平：0=命令 1=数据
  */
static void OLED_W_DC(uint8_t BitValue)
{
    if (BitValue) {
        DL_GPIO_setPins(OLED_DC_PORT, OLED_DC_PIN);
    } else {
        DL_GPIO_clearPins(OLED_DC_PORT, OLED_DC_PIN);
    }
}

/**
  * @brief  OLED 写 CS 片选电平（低有效）
  */
static void OLED_W_CS(uint8_t BitValue)
{
    if (BitValue) {
        DL_GPIO_setPins(OLED_CS_PORT, OLED_CS_PIN);
    } else {
        DL_GPIO_clearPins(OLED_CS_PORT, OLED_CS_PIN);
    }
}

/**
  * @brief  OLED 引脚初始化
  * @note   本板 OLED 的 5 根信号线全部由本函数直接配置（不依赖 SysConfig 里的
  *         Pin Group），因此只要 SysConfig 里没有别的外设抢占这些引脚即可直接跑。
  *         所有引脚默认复用功能为 GPIO，上电复位后即为输入/高阻，安全。
  */
void OLED_GPIO_Init(void)
{
    /* 1) 5 根信号线：推挽输出 */
    DL_GPIO_initDigitalOutput(OLED_SCLK_IOMUX);
    DL_GPIO_initDigitalOutput(OLED_MOSI_IOMUX);
    DL_GPIO_initDigitalOutput(OLED_RES_IOMUX);
    DL_GPIO_initDigitalOutput(OLED_DC_IOMUX);
    DL_GPIO_initDigitalOutput(OLED_CS_IOMUX);

    DL_GPIO_enableOutput(OLED_SCLK_PORT, OLED_SCLK_PIN);
    DL_GPIO_enableOutput(OLED_MOSI_PORT, OLED_MOSI_PIN);
    DL_GPIO_enableOutput(OLED_RES_PORT,  OLED_RES_PIN);
    DL_GPIO_enableOutput(OLED_DC_PORT,   OLED_DC_PIN);
    DL_GPIO_enableOutput(OLED_CS_PORT,   OLED_CS_PIN);

    /* 2) 空闲电平：CS 拉高(未选中)、SCLK 拉低、DC 拉低 */
    OLED_W_CS(1);
    OLED_W_SCLK(0);
    OLED_W_DC(0);

    /* 3) 【重要】引脚重合保护：
     *    PA4(44脚) 与 PA31 并联在 C0-SCL；PA3(43脚) 与 PA22 并联在 C0-SDA。
     *    这里把它们显式配置成“无上下拉的高阻输入”，不输出任何电平，
     *    避免两个推挽输出对顶短路。对应的驱动代码见下方注释块。 */
    DL_GPIO_initDigitalInputFeatures(OLED_ALT_SCLK_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(OLED_ALT_MOSI_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    /* --- 如确需改用 PA4/PA3 那一组驱动（此时必须把上面的 PA31/PA22 同样改成高阻
     *     输入，否则依旧对顶），再把下面 6 行取消注释、并把 board_pins.h 中
     *     OLED_SCLK_x 与 OLED_MOSI_x 改成 OLED_ALT_x 即可 —— 默认注释掉：
     *
     * DL_GPIO_initDigitalOutput(OLED_ALT_SCLK_IOMUX);
     * DL_GPIO_initDigitalOutput(OLED_ALT_MOSI_IOMUX);
     * DL_GPIO_enableOutput(OLED_ALT_SCLK_PORT, OLED_ALT_SCLK_PIN);
     * DL_GPIO_enableOutput(OLED_ALT_MOSI_PORT, OLED_ALT_MOSI_PIN);
     * DL_GPIO_setPins(OLED_ALT_SCLK_PORT, OLED_ALT_SCLK_PIN);
     * DL_GPIO_setPins(OLED_ALT_MOSI_PORT, OLED_ALT_MOSI_PIN);
     */
}


/* =========================================================================
 * 2. 软件 4 线 SPI 通信层
 *    SSD1306 4 线 SPI：MSB 先出，SCLK 上升沿采样，DC 决定命令/数据，
 *                     CS 全程拉低，模块只收不发（无 MISO）。
 * ========================================================================= */

/**
  * @brief  按位把一个字节送上 MOSI（MSB First）
  */
static void OLED_SPI_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_MOSI(!!(Byte & (0x80 >> i)));
        OLED_W_SCLK(1);     /* 上升沿：OLED 采样 SDA */
        OLED_W_SCLK(0);
    }
}

/**
  * @brief  向 OLED 屏幕发送“控制指令”
  * @param  Command: SSD1306 寄存器指令（如 0xAE 关显示）
  */
void OLED_WriteCommand(uint8_t Command)
{
    OLED_W_CS(0);
    OLED_W_DC(0);           /* DC=0 -> 命令 */
    OLED_SPI_SendByte(Command);
    OLED_W_CS(1);
}

/**
  * @brief  向 OLED 屏幕的显存中连续发送“图像数据”
  * @param  Data: 指向要发送的数据数组的指针
  * @param  Count: 本次连续发送的数据字节数量（一次 CS 内连发，效率高）
  */
void OLED_WriteData(uint8_t *Data, uint16_t Count)
{
    uint16_t i;
    OLED_W_CS(0);
    OLED_W_DC(1);           /* DC=1 -> 数据 */
    for (i = 0; i < Count; i ++)
    {
        OLED_SPI_SendByte(Data[i]);
    }
    OLED_W_CS(1);
}


/* =========================================================================
 * 3. OLED 硬件配置层
 * ========================================================================= */

/**
  * @brief  OLED 屏幕的上电初始化总函数（上电时序 + 寄存器初始化 + 清屏）
  * @note   初始化序列与手册 4.2 节《VCC Generated by Internal DC/DC Circuit》
  *         的 Power up Sequence 完全一致（同样与蓝本 I2C 版驱动一致）。
  */
void OLED_Init(void)
{
    OLED_GPIO_Init();

    /* 硬件复位：手册要求 RES# 先低再拉高，上电后建议 100ms 稳定 */
    OLED_W_RES(0);
    Delay_ms(20);
    OLED_W_RES(1);
    Delay_ms(100);

    OLED_WriteCommand(0xAE);	//关闭显示
    OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);	//设置多路复用率
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);	//设置显示偏移
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);	//设置显示开始行
    OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常，0xA0左右反置
    OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常，0xC0上下反置
    OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);	//设置对比度
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);	//设置预充电周期
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭
    OLED_WriteCommand(0xA6);	//设置正常/反色显示，0xA6正常，0xA7反色
    OLED_WriteCommand(0x8D);	//设置充电泵
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);	//开启显示

    OLED_Clear();
    OLED_Update();
}

/**
  * @brief  设置 OLED 硬件写入指针的物理坐标位置
  * @param  Page: 页地址 (Y轴)，范围 0~7
  * @param  X: 列地址 (X轴)，范围 0~127
  */
void OLED_SetCursor(uint8_t Page, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Page);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}


/* =========================================================================
 * 4. 显存管理与刷新层
 * ========================================================================= */

/**
  * @brief  将单片机内存中的缓冲区推送到 OLED 硬件屏幕
  * @note   每次调用下面的显示函数后，必须调用此函数，屏幕才会真正刷新！
  */
void OLED_Update(void)
{
    uint8_t j;
    for (j = 0; j < 8; j ++)
    {
        OLED_SetCursor(j, 0);
        OLED_WriteData(OLED_DisplayBuf[j], 128);
    }
}

/**
  * @brief  将内存缓冲区全部填 0（全局清屏）
  */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j ++)
    {
        for (i = 0; i < 128; i ++)
        {
            OLED_DisplayBuf[j][i] = 0x00;
        }
    }
}

/**
  * @brief  将内存缓冲区中指定的矩形区域清零（局部擦除）
  */
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t i, j;
    for (j = Y; j < Y + Height; j ++)
    {
        for (i = X; i < X + Width; i ++)
        {
            if (i >= 0 && i <= 127 && j >= 0 && j <= 63)
            {
                OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
            }
        }
    }
}


/* =========================================================================
 * 5. 用户 API 接口层
 * ========================================================================= */

/**
  * @brief  在缓冲区的指定位置绘制一幅二维点阵图像
  */
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
    uint8_t i = 0, j = 0;
    int16_t Page, Shift;

    OLED_ClearArea(X, Y, Width, Height);

    for (j = 0; j < (Height - 1) / 8 + 1; j ++)
    {
        for (i = 0; i < Width; i ++)
        {
            if (X + i >= 0 && X + i <= 127)
            {
                Page = Y / 8;
                Shift = Y % 8;
                if (Y < 0) { Page -= 1; Shift += 8; }

                if (Page + j >= 0 && Page + j <= 7)
                {
                    OLED_DisplayBuf[Page + j][X + i] |= (uint8_t)(Image[j * Width + i] << Shift);
                }
                if (Page + j + 1 >= 0 && Page + j + 1 <= 7)
                {
                    OLED_DisplayBuf[Page + j + 1][X + i] |= (uint8_t)(Image[j * Width + i] >> (8 - Shift));
                }
            }
        }
    }
}

/**
  * @brief  在指定位置显示单个 ASCII 字符
  */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
    if (Char < ' ' || Char > '~') { Char = ' '; }   /* 超范围字符用空格兜底，防数组越界 */

    if (FontSize == OLED_8X16)
    {
        OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[Char - ' ']);
    }
    else if (FontSize == OLED_6X8)
    {
        OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[Char - ' ']);
    }
}

/**
  * @brief  在指定位置显示一串纯英文/符号字符串
  */
void OLED_ShowString(int16_t X, int16_t Y, const char *String, uint8_t FontSize)
{
    uint16_t i = 0;
    while (String[i] != '\0')
    {
        OLED_ShowChar(X + i * FontSize, Y, String[i], FontSize);
        i ++;
    }
}

/**
  * @brief  类 printf 的格式化输出
  */
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, const char *format, ...)
{
    char String[64];
    va_list arg;
    va_start(arg, format);
    vsnprintf(String, sizeof(String), format, arg);
    va_end(arg);
    OLED_ShowString(X, Y, String, FontSize);
}
