#include "stm32f4xx.h"
#include "bldc.h"
#include "adc.h"

// ADC configuration
#define GPIOBEN (1U<<1)
#define ADC1EN (1U<<8)
#define ADC_CH13 (13U<<15)
#define ADC_CH7 (7U<<15)
#define ADC_CH8 (8U<<15)
#define ADC_SEQ_LEN_1 0x00
#define CR2_ADON (1U<<0)
#define CR2_DMA (1U<<8)

// Triggering the ADC
#define CR2_JEXTEN (3U<<20)
#define CR2_CONT (1U<<1)
#define CR2_EXTSEL (0x3U << 24)
#define CR2_JEXTSEL (0xFU << 16)

#define CR2_SWSTART (1U<<30)
//#define CR2_CONT (1U<<1)
#define CR2_JEOCIE (1U<<7)
#define CR2_ADCPRE (1U<<16)

volatile uint16_t back_emf_raw;
volatile float floating_phase_back_emf;
volatile uint16_t counter;
volatile float back_emf_plot;
volatile uint16_t raw_plot;

void back_emf_adc_init(void)
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
	// Datasheet p46 PC3-ADC123_IN13, p48 PB0-ADC12_IN8, p47 PA7-ADC12_IN7
	// Hence we are choosing ADC1 since all of these are connected to it. Channels 13,7,8

	// Enable clock access to ADC
	RCC->APB2ENR |= ADC1EN; // Datasheet p16 ADC1 is connected to APB2. Reference manual p149

	// Conversion sequence length
	ADC1->JSQR = ADC_SEQ_LEN_1; // Single channel. Reference manual p386. 0000 - 1 conversion

	// Conversion sequence start (where conversion starts), for injected ADC
	ADC1->JSQR |= ADC_CH13; // Only one channel, ch 1. Reference manual p387,388. Sets up channel one as first (and only) sequence
	// Commenting sequence line, delete if not needed. The idea is to trigger sampling only on 1 floating phase
	// THIS WILL HAVE TO BE MOVED TO SELECTION OF ADC CHANNEL LATER BECAUSE THERE IS ONLY 1 MEAS IN SEQUENCE
	// For now this is just initialization, will be changed as floating phases change.
	// IN INJECTING ADC STARTS FROM JSQ4

	// Sampling time = 480 cycles for CH7, CH8, CH13 (SMPR2 p385, SMPR1 p384)
	// 111b = 480 cycles; allows ringing on floating phase to settle before sample is taken
	ADC1->SMPR2 |= (7U << 21);  // CH7  bits [23:21]
	ADC1->SMPR2 |= (7U << 24);  // CH8  bits [26:24]
	ADC1->SMPR1 |= (7U << 9);   // CH13 bits [11:9]

	/* Setting up ADC triggering from TIM1 CH4 (in the middle of the pulse - CNT=ARR) */

	// Enabling external trigger for channels
	ADC1->CR2 |= CR2_JEXTEN; // Reference manual p382, bits 29,28: 11 is trigger on both rising and falling edge of the trigger signal, this is chosen because output compare CH4 is in TOGGLE mode

	// Disabling continuous mode since triggering comes through TIM1 CH4
	ADC1->CR2 &= ~ CR2_CONT;

	// Selecting external event for trigger, TIM1 CH4. This is commented because there is no tim1 ch 4 trigger, injected ADC is used instaed
	// ADC1->CR2 |= CR2_EXTSEL; // Reference manual p382
	ADC1->CR2 &= ~CR2_JEXTSEL; // 0000 TIM1 CH4 trigger event

	// ADC JSQR sequence length of injected ADC, reference manual p388
	ADC1->JSQR &= ~ (1U<<20);
	ADC1->JSQR &= ~ (1U<<21);

	// Setting prescaler to 4 to not be above maximum clock speed for ADC (36MHz max)
	ADC->CCR |= CR2_ADCPRE;

	// Setting up interrupt for injected channels
	ADC1->CR1 |= CR2_JEOCIE; // Reference manual p381

	//ADC set up, enable the ADC module
	ADC1->CR2 |= CR2_ADON; // Reference manual p382-384 bit 0

	// Enable interrupt in NVIC
	NVIC_EnableIRQ(ADC_IRQn);

	// Configure PA0 as GPIO output for ISR timing debug
	//GPIOA->MODER |=  (1U << 0);
	//GPIOA->MODER &= ~(1U << 1);

}

float adc_to_volts(uint32_t adc_raw_value)
{
	float factor = (VREF / ADC_RES) * DIVIDER_RATIO; // Pre-calculate this constant
	float phase_voltage = (float)adc_raw_value * factor;
	return phase_voltage;
}

void back_emf_float_channel(BLDC_Phase_t floating_phase)
{

    switch(floating_phase)
    {
    case PHASE_U:
    	ADC1->JSQR = ADC_CH13;	// Clearing content of register and writing channel of floating phase
        break;
    case PHASE_V:
    	ADC1->JSQR = ADC_CH8;
        break;
    case PHASE_W:
    	ADC1->JSQR = ADC_CH7;
        break;
    default:
        break;
    }
}

void ADC_IRQHandler(void)
{
    GPIOA->ODR ^= (1U << 0);          // toggle PA0 for debugging
    if (ADC1->SR & (1U << 2))         // JEOC flag
    {
        back_emf_raw = ADC1->JDR1;    // Read result from injected data register
        ADC1->SR &= ~(1U << 2);       // Clear JEOC flag
        floating_phase_back_emf = adc_to_volts(back_emf_raw) - VBUS_HALF;
        counter++;
        if (counter >= 20)
        {
        	back_emf_plot = floating_phase_back_emf;
        	raw_plot = back_emf_raw;
        	counter = 0;
        }
    }
}
