#ifndef INC_PWM_H_
#define INC_PWM_H_

#include "bldc.h"

int tim1_pwm_init(uint32_t f_pwm_hz, uint16_t resolution);
void tim1_pwm_set_duty_percent(uint32_t duty, BLDC_Phase_t phase);
void tim1_trig_adc(void);


#endif /* INC_PWM_H_ */
