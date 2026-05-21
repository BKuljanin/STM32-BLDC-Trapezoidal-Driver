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

// All three phases sampled every ADC trigger (3-conversion injected sequence).
// Per-IRQ values (full rate ~21.7kHz) — too fast for SWV trace, useful for in-firmware logic.
extern volatile uint16_t bemf_raw_u, bemf_raw_v, bemf_raw_w;
extern volatile float    bemf_u,     bemf_v,     bemf_w;

// SWV plot mirrors — updated every 3rd ADC IRQ (same throttle as back_emf_plot).
// Plot these three together to see all phases on one timeline.
extern volatile float    bemf_u_plot, bemf_v_plot, bemf_w_plot;

void back_emf_adc_init(void);
float adc_to_volts(uint32_t adc_value);
void back_emf_float_channel(BLDC_Phase_t floating_phase);


#endif /* INC_ADC_H_ */
