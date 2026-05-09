#ifndef INC_ADC_H_
#define INC_ADC_H_

void pa1_adc_dma_init(uint32_t src, uint32_t dst);
void tim2_trgo_adc(void);
float adc_to_volts(uint32_t adc_value);
float adc_to_amps(float voltage);

#endif /* INC_ADC_H_ */
