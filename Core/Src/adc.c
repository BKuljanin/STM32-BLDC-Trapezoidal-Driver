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

// Per-phase raw + converted values, populated every ADC trigger from the 3-conversion sequence
volatile uint16_t bemf_raw_u, bemf_raw_v, bemf_raw_w;
volatile float    bemf_u,     bemf_v,     bemf_w;

// Downsampled SWV plot mirrors — written only when the every-3rd-IRQ counter rolls.
volatile float    bemf_u_plot, bemf_v_plot, bemf_w_plot;

// Which phase is currently floating — set by back_emf_float_channel(), read in the ISR
// to route the corresponding per-phase value into floating_phase_back_emf.
static volatile BLDC_Phase_t current_floating_phase = PHASE_U;

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

	// 3-conversion injected sequence: every trigger reads U, V, W back-to-back.
	// JL = 10 (3 conversions) → sequence uses JSQ2, JSQ3, JSQ4 in that order.
	// First conversion result → JDR1, second → JDR2, third → JDR3.
	//   JSQ2 = CH13 (U) → JDR1
	//   JSQ3 = CH8  (V) → JDR2
	//   JSQ4 = CH7  (W) → JDR3
	ADC1->JSQR  = 0;
	ADC1->JSQR |= (2U  << 20);  // JL[1:0] = 10 → 3 conversions
	ADC1->JSQR |= (13U << 5);   // JSQ2 = CH13 (U)
	ADC1->JSQR |= (8U  << 10);  // JSQ3 = CH8  (V)
	ADC1->JSQR |= (7U  << 15);  // JSQ4 = CH7  (W)

	// Sample time = 15 cycles (001b) so the whole 3-conversion sequence (3 × 27 ADC cycles =
	// 81 cycles ≈ 6.5µs @ 12.5MHz) finishes inside the PWM ON window (~7µs each side of trigger
	// at 30% duty / 21.7kHz). 15 cycles is still ample for the 1.8kΩ BEMF divider source impedance.
	ADC1->SMPR2 |= (1U << 21);  // CH7  bits [23:21]
	ADC1->SMPR2 |= (1U << 24);  // CH8  bits [26:24]
	ADC1->SMPR1 |= (1U << 9);   // CH13 bits [11:9]

	/* Setting up ADC triggering from TIM1 CH4 (in the middle of the pulse - CNT=ARR) */

	// Enabling external trigger for channels
	ADC1->CR2 |= CR2_JEXTEN; // Reference manual p382, bits 29,28: 11 is trigger on both rising and falling edge of the trigger signal, this is chosen because output compare CH4 is in TOGGLE mode

	// Disabling continuous mode since triggering comes through TIM1 CH4
	ADC1->CR2 &= ~ CR2_CONT;

	// Selecting external event for trigger, TIM1 CH4. This is commented because there is no tim1 ch 4 trigger, injected ADC is used instaed
	// ADC1->CR2 |= CR2_EXTSEL; // Reference manual p382
	ADC1->CR2 &= ~CR2_JEXTSEL; // 0000 TIM1 CH4 trigger event

	// Setting prescaler to 4 to not be above maximum clock speed for ADC (36MHz max)
	ADC->CCR |= CR2_ADCPRE;

	// Setting up interrupt for injected channels
	ADC1->CR1 |= CR2_JEOCIE; // Reference manual p381

	//ADC set up, enable the ADC module
	ADC1->CR2 |= CR2_ADON; // Reference manual p382-384 bit 0

	// Enable interrupt in NVIC
	NVIC_EnableIRQ(ADC_IRQn);

	// Configure PB15 as GPIO output for ADC sample timing debug
	GPIOB->MODER |=  (1U << 30);  // bits [31:30] = 01 → output
	GPIOB->MODER &= ~(1U << 31);

	// See gpio.c for description of PC9 GPIO
	GPIOC->BSRR = (1U << (9 + 16));
}

float adc_to_volts(uint32_t adc_raw_value)
{
	float factor = (VREF / ADC_RES) * DIVIDER_RATIO; // Pre-calculate this constant
	float phase_voltage = (float)adc_raw_value * factor;
	return phase_voltage;
}

// JSQR is now a fixed 3-conversion sequence (U, V, W) configured once in back_emf_adc_init().
// This function just records which phase is currently floating so the ADC ISR can route the
// corresponding per-phase value into floating_phase_back_emf for the BEMF mode logic.
void back_emf_float_channel(BLDC_Phase_t floating_phase)
{
    current_floating_phase = floating_phase;
}

void ADC_IRQHandler(void)
{
    if (ADC1->SR & (1U << 2))         // JEOC = end of injected sequence (all 3 conversions done)
    {
        GPIOB->ODR ^= (1U << 15);     // toggle PB15 — probe here to verify sample timing

        // Pull all three results out of the injected data registers.
        // Mapping (set in back_emf_adc_init): JSQ2(U)→JDR1, JSQ3(V)→JDR2, JSQ4(W)→JDR3
        bemf_raw_u = ADC1->JDR1;
        bemf_raw_v = ADC1->JDR2;
        bemf_raw_w = ADC1->JDR3;

        ADC1->SR &= ~(1U << 2);       // Clear JEOC flag

        bemf_u = adc_to_volts(bemf_raw_u) - VBUS_HALF;
        bemf_v = adc_to_volts(bemf_raw_v) - VBUS_HALF;
        bemf_w = adc_to_volts(bemf_raw_w) - VBUS_HALF;

        // Route the floating phase's reading into the variable BEMF mode logic consumes.
        switch (current_floating_phase)
        {
        case PHASE_U:
            floating_phase_back_emf = bemf_u;
            back_emf_raw            = bemf_raw_u;
            break;
        case PHASE_V:
            floating_phase_back_emf = bemf_v;
            back_emf_raw            = bemf_raw_v;
            break;
        case PHASE_W:
            floating_phase_back_emf = bemf_w;
            back_emf_raw            = bemf_raw_w;
            break;
        default:
            break;
        }

        counter++;
        if (counter >= 3)
        {
        	back_emf_plot = floating_phase_back_emf;
        	raw_plot      = back_emf_raw;
        	counter = 0;
        }

        // Separate, slower counter for the per-phase SWV mirrors — 3 floats per write
        // would saturate the SWV trace at the back_emf_plot rate. ~720Hz update here.
        static uint16_t phase_plot_counter = 0;
        phase_plot_counter++;
        if (phase_plot_counter >= 30)
        {
        	bemf_u_plot = bemf_u;
        	bemf_v_plot = bemf_v;
        	bemf_w_plot = bemf_w;
        	phase_plot_counter = 0;
        }
    }
}
