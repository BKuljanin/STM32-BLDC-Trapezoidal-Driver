#ifndef INC_ADC_H_
#define INC_ADC_H_

#define VREF 3.3f
#define ADC_RES 4095.0f
#define DIVIDER_RATIO 7.66f

#include "bldc.h"

extern volatile uint16_t back_emf_raw;

void back_emf_adc_init(void);
float adc_to_volts(uint32_t adc_value);
void back_emf_float_channel(BLDC_Phase_t floating_phase);


#endif /* INC_ADC_H_ */
