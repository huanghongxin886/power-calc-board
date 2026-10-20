/* ============================================================================
 * 延时函数（裸机版）
 * 蓝本工程里 Delay_ms() 走的是 FreeRTOS 的 App_DelayMs()，本工程主函数只做
 * OLED 显示、不跑 RTOS，所以这里改成纯 delay_cycles() 忙等，零依赖。
 * ==========================================================================*/

#include "Delay.h"

/**
  * @brief  微秒级忙等延时
  * @note   80MHz 下 delay_cycles(80) ≈ 1us（含循环开销，实际略长，用于软件
  *         SPI 的位延时绰绰有余；OLED 手册 SCLK 周期最小只要 100ns）。
  */
void Delay_us(unsigned long us)
{
    while (us--)
    {
        delay_cycles(CPUCLK_FREQ / 1000000UL);
    }
}

/**
  * @brief  毫秒级忙等延时
  */
void Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        Delay_us(1000);
    }
}
