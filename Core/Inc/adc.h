#ifndef INC_ADC_H_
#define INC_ADC_H_

#define VREF 3.3f
#define ADC_RES 4095.0f
#define DIVIDER_RATIO 7.66f

void back_emf_adc_dma_init(void);
void tim2_trgo_adc(void);
float adc_to_volts(uint32_t adc_value);
float adc_to_amps(float voltage);


#endif /* INC_ADC_H_ */
