#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#include <stdint.h>

void tim3_pa6_1mhz_init(void);
#define SR_CC1IF (1U<<1)

extern volatile uint8_t measurement_ready;
extern volatile uint32_t pulse_width;
extern volatile uint32_t pulse_period;
extern volatile uint32_t rising_previous;

uint32_t tim2_get_delta_us(void);
void tim2_1mhz_init(void);

#endif /* INC_TIMER_H_ */
