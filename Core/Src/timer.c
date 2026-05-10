#include "stm32f4xx.h"
#include "timer.h"

#define GPIOAEN (1U<<0)
#define AFR6_TIM (1U<<25)
#define TIM3EN (1U<<1)
#define TIM2EN (1U<<0)
#define CCMR1_IN_CC1S (1U<<0)
#define CCMR1_IN_CC2S (1U<<9)
#define CCER_CC1E (1U<<0)
#define CCER_CC2E (1U<<4)
#define CCER_CC2P (1U<<5)
#define CR1_CEN	(1U<<0)
#define DIER_CC1IE (1U<<1)

volatile uint8_t measurement_ready;
volatile uint32_t pulse_width;
volatile uint32_t pulse_period;
volatile uint32_t rising_previous;

// Initializing timer for capturing encoder measurements
void tim3_pa6_1mhz_init(void)
{

	// Enable clock access to GPIOA
	RCC->AHB1ENR |= GPIOAEN; // Datasheet p16 GPIOA is connected to AHB1

	GPIOA->MODER &=~ (1U<<12); // Reference manual p186 10-alternate function
	GPIOA->MODER |= (1U<<13);

	// Set PA6 alternate function as timer 3 channel 1
	GPIOA->AFR[0] |= AFR6_TIM; // Reference manual p190 low AFRL (low register since its PA6), AF02. Set bit 25 to 1

	// Enable clock access to tim3
	RCC->APB1ENR |= TIM3EN; // Reference manual p146 tim3 bit 1

	// Set prescaler
	TIM3->PSC = 50 - 1; // -1 because of counting from zero, 50 000 000 / 50 = 1 000 000 (1MHz) (uses APB1, system clock is 100 MHz)

	// Set CH1 to input mode
	TIM3->CCMR1 = CCMR1_IN_CC1S; // Reference manual p497, 498

	// Set CH2 to listen on same input as CH1
	TIM3->CCMR1 |= CCMR1_IN_CC2S;	// Reference manual p495

	TIM3->CCMR1 |= (0x3 << 4) | (0x3 << 12); // Configuring filter

	// Initialize CH1 to capture at rising edge, default is rising edge
	TIM3->CCER |= CCER_CC1E; // Reference manual p499 capture/compare enable register, p501 CC1E see configuration for input, bit 0 capture enable

	// Initialize CH2 to capture at falling edge (to capture pulse width)
	TIM3->CCER |= CCER_CC2P | CCER_CC2E; // Reference manual p499, setting the polarity to falling edge

	// Enable TIM3
	TIM3->CR1 = CR1_CEN;

	// Enable TIM interrupt
	TIM3->DIER |= DIER_CC1IE; // Reference manual p491

	// Enable TIN interrupt in NVIC
	NVIC_EnableIRQ(TIM3_IRQn);
}

// Timer 3 interrupt to capture encoder pulse width
void TIM3_IRQHandler(void)
{

	if (TIM3->SR & SR_CC1IF) {
		TIM3->SR &=~ SR_CC1IF;	// When interrupt occurs clear the interrupt flag

		uint32_t rising = TIM3->CCR1;	// Capturing ticks for rising edge
		uint32_t falling = TIM3->CCR2;	// Capturing ticks for falling edge

		// Skip first interrupt, rising_previous is not valid yet
		static uint8_t first = 1;
		if (first) {
			first = 0;
			rising_previous = rising;
			return;
		}

		pulse_width = (falling - rising_previous) & 0xFFFF;
		pulse_period = (rising - rising_previous) & 0xFFFF;

		rising_previous = rising;

		measurement_ready = 1;

	    return;
	}
}


// Initializing timer for measuring time and commutation TIMER2
void tim2_1mhz_init(void)
{
	// Enable clock access to tim2
	RCC->APB1ENR |= TIM2EN; // Reference manual p146 tim3 bit 1

	// Set prescaler
	TIM2->PSC = 50 - 1; // -1 because of counting from zero, 50 000 000 / 50 = 1 000 000 (1MHz) (uses APB1, system clock is 100 MHz)

	// Set high counting value to reduce wrapping
	TIM2->ARR = 0xFFFFFFFF;

	// Enable TIM2
	TIM2->CR1 = CR1_CEN;

}

// Function for reading time in us
uint32_t read_time_us(void)
{
	uint32_t time_passed = TIM2->CNT;
	return time_passed;
}

uint32_t tim2_get_delta_us(void)
{
      static uint32_t last_time = 0;
      uint32_t now = TIM2->CNT;
      uint32_t delta = now - last_time;
      last_time = now;
      return delta;
}
