/*
 * pwm.h
 *
 *  Created on: Jan 16, 2026
 *      Author: Bogdan Kuljanin
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

int tim2_pa5_pwm_init(uint32_t f_pwm_hz, uint16_t resolution);
void tim2_pwm_set_duty_percent(uint32_t duty);
void tim2_trig_adc(void);

#endif /* INC_PWM_H_ */
