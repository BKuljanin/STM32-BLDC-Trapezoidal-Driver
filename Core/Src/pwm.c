#include "stm32f4xx.h"
#include "pwm.h"
// TIM1 will be used, datasheet p16, connected to APB2 bus (100 MHz)

// Setting up PWM on timer 2 channel 1
#define TIM_CLK_HZ 100000000UL  // Timer clock fixed at 100 MHz, this can be changed for different clock frequencies, HSI is chosen as SYSCLK in this project
#define TIM1EN (1U<<0)
#define GPIOAEN (1U<<0)
#define AFR8_TIM (1U<<0)
#define AFR9_TIM (1U<<4)
#define AFR10_TIM (1U<<8)
#define CCER_CC1E (1U<<0)
#define CCER_CC2E (1U<<4)
#define CCER_CC3E (1U<<8)
#define OC_PWM1 ((1U<<6) | (1U<<5))
#define OC_PWM2 ((1U<<14) | (1U<<13))
#define OC_PWM3 ((1U<<6) | (1U<<5))
#define CR1_CEN (1U<<0)
#define CR1_CENTER_MODE1 (1U<<5)
#define CCMR1_OC1PE (1U<<3)
#define CCMR1_OC2PE (1U<<11)
#define CCMR1_OC3PE (1U<<3)
#define TIM_BDTR_MOE (1U<<15)

// Setting up TRGO for ADC when timer 2 reaches ARR
#define CCER_CC4E (1U<<12)
#define CCMR1_OC4M_TOGGLE ((1U<<12) | (1U<<13))

int tim1_pwm_init(uint32_t f_pwm_hz, uint16_t resolution)
{
	// Initializing timer. Configuring PA8,9,10 pins as output capture in PWM mode

	/* PA8,9,10 configuration */

	// Enable clock access to GPIOA
	RCC->AHB1ENR |= GPIOAEN; // Datasheet p16 GPIOA is connected to AHB1

	// Set PA8,9,10 mode as alternate function mode as per datasheet p57 (AF1)
	// PA8
	GPIOA->MODER &=~ (1U<<16); // Reference manual p186 alternate function 10 for alternate function mode
	GPIOA->MODER |= (1U<<17); // For PA8 bits 16 and 17 are configured to 10 for alternate fucntion mode
	// PA9
	GPIOA->MODER &=~ (1U<<18); // Reference manual p186 alternate function 10 for alternate function mode
	GPIOA->MODER |= (1U<<19); // For PA9 bits 18 and 19 are configured to 10 for alternate fucntion mode
	// PA10
	GPIOA->MODER &=~ (1U<<20); // Reference manual p186 alternate function 10 for alternate function mode
	GPIOA->MODER |= (1U<<21); // For PA10 bits 20 and 21 are configured to 10 for alternate fucntion mode

	// Set PA8,9,10 alternate function type to TIM1_CH1/2/3 (AF01 datasheet p57)
	// PA8
	GPIOA->AFR[1] |= AFR8_TIM; // Alternate function register reference manual p190 (high register since its pin 8 (AFRL8)), bits 0-3. AF1 0001

	// PA9
	GPIOA->AFR[1] |= AFR9_TIM;

	// PA10
	GPIOA->AFR[1] |= AFR10_TIM;

	/*TIM1 configuration*/

	/* Calculate ARR and PSC to obtain given resolution and PWM frequency */
	uint32_t ARR;
	int32_t  PSC;
	ARR = (1UL << resolution) - 1;   // Set auto-reload for N-bit resolution (ARR = 2^N - 1)
	PSC = (int32_t)(TIM_CLK_HZ / (f_pwm_hz * 2 * ARR)) - 1;

	if ((int32_t)PSC < 0) {
	    return -1;   // ERROR: configuration impossible
	}

	// Enable clock access to timer 1. Reference manual p149 ch 6.3.13 bit 0 is for TIM1 EN
	RCC->APB2ENR |= TIM1EN;

	// Set the prescaler value
	TIM1->PSC = PSC; // Setting the calculated PSC value in the register

	// Set the auto reload value
	TIM1->ARR = ARR; // Counting to ARR value

	// Clear timer counter
	TIM1->CNT = 0; // Reference manual p509

	// Initializing duty cycle. CCR1, value that PWM mode 1 compares with counted value. Initializing on every channel
	TIM1->CCR1 = 0;  // Reference manual p504. 0% initialization duty
	TIM1->CCR2 = 0;
	TIM1->CCR3 = 0;

	// Center aligned mode 1 selection
	TIM1->CR1 &= ~ (1U << 6) ; // Reference manual p485 bits 6 and 5: 01 configuration is center aligned mode 1
	TIM1->CR1 |= CR1_CENTER_MODE1 ;
	// Choosing mode 1 that triggers events and interrupts when counting down starts
	// This aligns with triggering ADC in the middle of the pulse (CNT = ARR marks the beginning of countdown)

	/* Preload (OC1PE) is used to prevent duty cycle glitches.
	   Without it, if CCR1 (compared value) is updated to a value the counter has already passed,
	   the hardware misses the match event, causing 100% or 0% pulse.
	   Preload buffers the update in a shadow register, loading it only at the end of the cycle
	   to ensure the counter always hits the new target correctly.*/
	TIM1->CCMR1 |= CCMR1_OC1PE;	// Channel 1
	TIM1->CCMR1 |= CCMR1_OC2PE; // Channel 2
	TIM1->CCMR2 |= CCMR1_OC3PE; // Channel 3

	// Set output compare PWM1 mode
	TIM1->CCMR1 |= OC_PWM1; // Reference manual p495. Setting5 bits 6 and 5 to 1. 110 is PWM mode 1, pin is high only while CNT<CCR1
	/* Using PWM1 mode for the PWM that will go to high side MOSFET. Low side MOSFET will be inverted.
	  The signal will be high until CCR1 is reached and it says low at ARR. Then counting backwards
	  starts and signal is low until CCR1 is reached again. ARR is in the middle of the pulse (signal is low then on high side) */

	TIM1->CCMR1 |= OC_PWM2;	// Channel 2 PWM mode
	TIM1->CCMR2 |= OC_PWM3; // Channel 3 PWM mode

	// Enable tim1 ch1,ch,ch3 in compare mode
	// CH1
	TIM1->CCER |= CCER_CC1E; // Reference manual p499-501 ch 16.4.9. capture/compare 1 output enable
	// CH2
	TIM1->CCER |= CCER_CC2E;
	// CH3
	TIM1->CCER |= CCER_CC3E;

	// Enable timer
	TIM1->CR1 |= CR1_CEN; // Reference manual p485. Timer 2 CR1

	// Enable output
	TIM1->BDTR |= TIM_BDTR_MOE;

	return 0; // Initialization successful
}

void tim1_pwm_set_duty_percent(uint32_t duty, BLDC_Phase_t phase)
{
	//Input is duty cycle in %

    if (duty > 100) duty = 100;  // limit to 100 % in case input is >100%

    if (duty > MAX_DUTY_CYCLE) // Limit duty cycle to maximum allowed
    {
    	duty = MAX_DUTY_CYCLE;
    }

    // Setting up count value as a percentage of ARR
    uint32_t count_value = ((uint32_t)duty * TIM1->ARR) / 100; // First multiplication then division because of integer division

    switch(phase)
    {
    case PHASE_U:
    	TIM1->CCR1 = count_value; // Capture/compare register for channel 1 (CCR1)
    	break;

    case PHASE_V:
    	TIM1->CCR2 = count_value;
    	break;

    case PHASE_W:
       TIM1->CCR3 = count_value;
       break;

    default:
    	break;
    }

}

void tim1_trig_adc(void)
{
	// Trigger for ADC conversion set up at ARR (when the current stabilizes and switching noise settles) at channel 4

	// Loading ARR value of tim 1 (PWM) to capture/compare register for channel 4 (CCR2)
	TIM1->CCR4 = TIM1->ARR;

	// Setting up TIM1 CH4 as output compare in toggle mode
 	TIM1->CCMR2 |= CCMR1_OC4M_TOGGLE; // Reference manual p495 bits 12-14 OC2M (output compare for channel 2), TOGGLE mode selected (011). Once ARR is reached output is toggled
 	// Since CH2 output compare is in toggle mode (toggles on ARR), ADC is triggered on both rising and falling edge (see ADC config)

	// Enabling TIM1 channel 4 in output mode
	TIM1->CCER |= CCER_CC4E; // Reference manual p499 bit 4 CC2E
}
