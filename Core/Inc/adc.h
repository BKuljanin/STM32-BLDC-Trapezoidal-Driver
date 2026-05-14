#ifndef INC_ADC_H_
#define INC_ADC_H_

#define VREF 3.3f
#define ADC_RES 4095.0f
#define DIVIDER_RATIO ((10.0f + 2.2f) / 2.2f) // IHM07 reference manual p17

#define VBUS        12.0f
#define VBUS_HALF   (VBUS / 2.0f)

#include "bldc.h"

extern volatile uint16_t back_emf_raw;
extern volatile float floating_phase_back_emf;
extern volatile float back_emf_plot;
extern volatile uint16_t raw_plot;

void back_emf_adc_init(void);
float adc_to_volts(uint32_t adc_value);
void back_emf_float_channel(BLDC_Phase_t floating_phase);


#endif /* INC_ADC_H_ */
