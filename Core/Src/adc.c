#include "stm32f4xx.h"

// ADC configuration
#define GPIOBEN (1U<<1)
#define ADC1EN (1U<<8)
#define ADC_CH13 13
#define ADC_SEQ_LEN_1 0x00
#define CR2_ADON (1U<<0)
#define CR2_DMA (1U<<8)

// Triggering the ADC
#define CR2_EXTEN ((1U<<28)|(1U<<29))
#define CR2_CONT (1U<<1)
#define CR2_EXTSEL (0x3U << 24)
#define CR2_JEXTSEL (0x15U << 16)

#define CR2_SWSTART (1U<<30)
//#define CR2_CONT (1U<<1)

// DMA configuration
#define DMA2EN (1U<<22)
#define DMA_CR_EN (1U<<0)
#define DMA_MEM_INC (1U<<10)
#define DMA_DIR_PERIPH_TO_MEM_6 (1U<<6)
#define DMA_DIR_PERIPH_TO_MEM_7 (1U<<7)
#define DMA_CR_CIRC (1U<<8)


void back_emf_adc_dma_init(uint32_t src, uint32_t dst)
{
	/* Configure the ADC GPIO pins: PC3, PB0, PA7 */

	// Enable clock access to ADC's pin port, datasheet p46, reference manual p146
	//RCC->AHB1ENR |= GPIOAEN; // Enabled in pwm.c
	RCC->AHB1ENR |= GPIOBEN;
	//RCC->AHB1ENR |= GPIOCEN; // Enabled in gpio.c

	// Set mode of PC3 to analog mode, reference manual p186 11-analog mode
	GPIOC->MODER |= (1U<<6);
	GPIOC->MODER |= (1U<<7);

	// Set mode of PB0 to analog mode, reference manual p186 11-analog mode
	GPIOB->MODER |= (1U<<0);
	GPIOB->MODER |= (1U<<1);

	// Set mode of PA7 to analog mode, reference manual p186 11-analog mode
	GPIOA->MODER |= (1U<<14);
	GPIOA->MODER |= (1U<<15);

	/* Configure ADC modules */
	// Datasheet p46 PC3-ADC123_IN13, p47 PA7-ADC12_IN7, p48 PB0-ADC12_IN8
	// Hence we are choosing ADC1 since all of these are connected to it. Channels 13,7,8

	// Enable clock access to ADC
	RCC->APB2ENR |= ADC1EN; // Datasheet p16 ADC1 is connected to APB2. Reference manual p149

	// Conversion sequence start (where conversion starts), PA1 corresponds to CH1
	ADC1->SQR3 = ADC_CH13; // Only one channel, ch 1. Reference manual p387,388. Sets up channel one as first (and only) sequence
	// Commenting sequence line, delete if not needed. The idea is to trigger sampling only on 1 floating phase
	// THIS WILL HAVE TO BE MOVED TO SELECTION OF ADC CHANNEL LATER BECAUSE THERE IS ONLY 1 MEAS IN SEQUENCE
	// For now this is just initialization, will be changed as floating phases change.

	// Conversion sequence length
	ADC1->SQR1 = ADC_SEQ_LEN_1; // Single channel. Reference manual p386. 0000 - 1 conversion

	// Configure Sampling Time for Channel 1 (Bits 3,4,5 in SMPR2). Required especially for high impedance input
	ADC1->SMPR2 &= ~ (1U << 5);
	ADC1->SMPR2 &= ~ (1U << 4);
	ADC1->SMPR2 &= ~ (1U << 3);
	// As per reference manual p385, 000 on bits 3,4,5 of SMPR2 define sampling time of 3 cycles (time it takes for internal capacitor to charge)

	/* Setting up ADC triggering from TIM1 CH4 (in the middle of the pulse - CNT=ARR) */

	// Enabling external trigger for channels
	ADC1->CR2 |=  CR2_EXTEN; // Reference manual p382, bits 29,28: 11 is trigger on both rising and falling edge of the trigger signal, this is chosen because output compare CH4 is in TOGGLE mode

	// Disabling continuous mode since triggering comes through TIM1 CH4
	ADC1->CR2 &= ~ CR2_CONT;

	// Selecting external event for trigger, TIM1 CH4. This is commented because there is no tim1 ch 4 trigger, injected ADC is used instaed
	// ADC1->CR2 |= CR2_EXTSEL; // Reference manual p382
	ADC1->CR2 &= ~CR2_JEXTSEL; // 0000 TIM1 CH4 trigger event

	//ADC set up, enable the ADC module
	ADC1->CR2 |= CR2_ADON; // Reference manual p382-384 bit 0

	/* Setting up DMA */
	// Reference manual p205 shows that DMA2 stream 0 channel 0 is used for ADC1

	// DMA mode enable for ADC
	ADC1->CR2 |= CR2_DMA; // Reference manual p384 bit 8

	// Enable clock access to DMA
	RCC->AHB1ENR |= DMA2EN; // Reference manual p144. Enable the clock for the DMA2 controller, which is mapped to the AHB1 peripheral bus

	ADC1->CR2 |= (1U << 9); // Reference manual p383 bit 9, DMA requests are issued as long as there is converted data

	// Disable DMA2 stream 0
	DMA2_Stream0->CR &=~ DMA_CR_EN; // Reference manual p226 DMA stream configuration register, bit 0 needs to be 0 to disable the stream
	// DMA1_Streamx and DMA2_Streamx use the same offsets but different base addresses. Each DMA controller has 8 independent streams (0–7)

	// Wait until DMA2 stream 0 is disabled
	while(DMA2_Stream0->CR & DMA_CR_EN){} // Stuck here until its disabled, happens only in initialization

	// Clear interrupt flags for stream 0
	DMA2->LIFCR |= (1U<<0); // Reference manual p225, low interrupt flag clear register
	DMA2->LIFCR |= (1U<<2); // this is all flag clear register, these bits are for stream 0 (clears bits by putting 1 in them)
	DMA2->LIFCR |= (1U<<3);
	DMA2->LIFCR |= (1U<<4);
	DMA2->LIFCR |= (1U<<5);

	// Set the source buffer
	DMA2_Stream0->PAR = src; // Source that will be passed (peripheral)

	// Set the destination buffer
	DMA2_Stream0->M0AR = dst; // Memory destination. Memory destination address register. Reference manual p229. Destination is memory. Memory 0 AR

	// Set length
	DMA2_Stream0-> NDTR = 1; // Number of transfers (single ADC sample per request)

	// Set MSIZE
	DMA2_Stream0->CR |= (1 << 11);  // MSIZE = 16-bit, reference manual p227

	// Set PSIZE
	DMA2_Stream0->CR |= (1 << 13);  // PSIZE = 16-bit, reference manual p227

	// Set circular mode
	DMA2_Stream0->CR |= DMA_CR_CIRC; // Reference manual p226 CR bit 8. DMA stays enabled and waits for the next ADC request (instead of just saving ADC reading once and then idle)

	// Select stream 0 channel 0 (ADC1) Reference manual p205
	DMA2_Stream0->CR &=~ (1U<<25); // Reference manual p226 stream configuration register. CHSEL bit is for channel select. Bits 25,26,27. 000 is channel 0
	DMA2_Stream0->CR &=~ (1U<<26);
	DMA2_Stream0->CR &=~ (1U<<27);

	// Configure transfer direction peripheral->memory (ADC data register to RAM)
	DMA2_Stream0->CR &=~ DMA_DIR_PERIPH_TO_MEM_6; // Same register as above, bits 6,7 DIR. 00 peripheral to memory,
	DMA2_Stream0->CR &=~ DMA_DIR_PERIPH_TO_MEM_7;

	// Enable direct mode and disable FIFO
	DMA2_Stream0->FCR = 0; // Fifo control register, reference manual p231. 0 in the register is interrupt disabled, also 0 is direct mode enable

	// Enable DMA2 stream 0 since everything is now set up
	DMA2_Stream0->CR |= DMA_CR_EN; // DMA stream configuration register p226 reference manual

}

float adc_to_volts(uint32_t adc_value)
{
    // Converts ADC reading to volts. Output of the current sensor is in range 0 - 5 V. STM32 pins are rated for 3.3 V input.
	// That is why voltage divider is implemented, as a series of three 10 kOhm resistors, reducing the voltage.
	// The equation below accounts for the voltage divider and returns the original voltage, before dividing (4095 is max output of the 12-bit ADC).
    float voltage = 9.9f * (float)adc_value / 8190.0f * 2.47f / 2.5f; // 2.47f / 2.5f This calibration is experimental, on 0 A the measured voltage is 2.47 V (not 2.5V)
    return voltage;
}

float adc_to_amps(float voltage)
{
	// Voltage reading represents current signal, -5 - 5 A translates to 0 - 3.3 V on Hall sensor OUT pin
	// Linear conversion derived from sensor datasheet and calibrated experimentally
	float current = 5.405f * voltage - 13.5135f + 0.16f; // Last element in the sum is the offset which is experimentally obtained
	return current;
}

