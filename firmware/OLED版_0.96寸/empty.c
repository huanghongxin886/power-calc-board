/* ============================================================================
 *  type-C口功率计算器放大板 —— MSPM0G3507 + 0.96 寸 OLED(SSD1306, 128x64) 测试程序
 *  ---------------------------------------------------------------------------
 *  功能：OLED 上
 *        第一行显示 1
 *        第二行显示 2
 *        第三行显示 3
 *        第四行显示 4
 *
 *  主函数【只保留 OLED 显示】，不初始化任何其它外设：
 *    - 蓝本工程里的电机 PWM / 编码器 / IMU / 超声波 / 灰度 / 蓝牙 / 云台(UART)
 *      等代码全部不参与编译（引脚与本板冲突，详见 board_pins.h 与 README.md）
 *
 *  硬件：
 *    MCU   U4 = MSPM0G3507SPMR (LQFP-64)
 *    OLED  SCL=PA31(39)  SDA=PA22(18)  RES=PA7(49)  DC=PB7(59)  CS=PB6(58)
 * ==========================================================================*/

#include "ti_msp_dl_config.h"
#include "OLED.h"
#include "Delay.h"
#include "board_pins.h"

/* 四行文字的起始 X 坐标：8x16 字体宽 8 像素，居中显示 (128-8)/2 = 60 */
#define OLED_LINE_X     60

int main(void)
{
    /* 1. 系统时钟等基础配置（SysConfig 生成，80MHz） */
    SYSCFG_DL_init();

    /* 2. OLED 初始化（含软件 SPI 引脚配置、硬件复位、寄存器初始化、清屏） */
    OLED_Init();

    /* 3. 四行内容：1 / 2 / 3 / 4 */
    OLED_ShowString(OLED_LINE_X, 0,  "1", OLED_8X16);   /* 第一行 */
    OLED_ShowString(OLED_LINE_X, 16, "2", OLED_8X16);   /* 第二行 */
    OLED_ShowString(OLED_LINE_X, 32, "3", OLED_8X16);   /* 第三行 */
    OLED_ShowString(OLED_LINE_X, 48, "4", OLED_8X16);   /* 第四行 */

    /* 4. 把显存刷到屏幕（上面的显示函数只改内存，必须调用本函数才可见） */
    OLED_Update();

    /* 5. 内容固定，无需反复刷新。
     *    若以后要显示变化的数据，把 OLED_Update() 放进循环即可，例如：
     *    while (1) { OLED_ShowString(...); OLED_Update(); Delay_ms(200); }
     */
    while (1)
    {
    }
}
