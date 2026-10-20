#ifndef DELAY_H_
#define DELAY_H_

#include "ti_msp_dl_config.h"

/* SysConfig 会生成 CPUCLK_FREQ；万一没生成，按本工程 80MHz 兜底 */
#ifndef CPUCLK_FREQ
#define CPUCLK_FREQ 80000000UL
#endif

void Delay_us(unsigned long us);
void Delay_ms(uint32_t ms);

#endif /* DELAY_H_ */
