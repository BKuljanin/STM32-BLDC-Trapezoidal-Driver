#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#include <stdint.h>

void tim3_1khz_it_init(void);
void tim2_1mhz_init(void);
uint32_t tim2_get_delta_us(void);
uint32_t read_time_us(void);

#endif /* INC_TIMER_H_ */
