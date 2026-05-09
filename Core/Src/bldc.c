#include "bldc.h"

void phase_enable(BLDC_Phase_t phase)
{

	switch(phase)
	    {
	    case PHASE_U:
	    	GPIOC->BSRR = (1U<<10);	// Write 1 to bit 10 and 0 to everything else. 0s are ignored only PC10 changes. This is atomic, no reading
	    	break;

	    case PHASE_V:
	    	GPIOC->BSRR = (1U<<11);
	    	break;

	    case PHASE_W:
	    	GPIOC->BSRR = (1U<<12);
	       break;

	    default:
	    	break;
	    }
}


void phase_disable(BLDC_Phase_t phase)
{

	switch(phase)
		    {
		    case PHASE_U:
		    	GPIOC->BSRR = (1U<<10+16);	// Reference manual p188,189. First 16 register are for setting, other 16 for resetting
		    	break;

		    case PHASE_V:
		    	GPIOC->BSRR = (1U<<11+16);
		    	break;

		    case PHASE_W:
		    	GPIOC->BSRR = (1U<<12+16);
		       break;

		    default:
		    	break;
		    }
}


//  Function for testing BLDC. Drives BLDC similar to step motor, energizing phase by phase, moving motor in 6 steps
void bldc_test_run(uint32_t delay_ms, uint32_t duty_cycle)
{

	while(1)
	{
		// Step 1
		phase_enable(PHASE_U);
		phase_enable(PHASE_V);
		phase_disable(PHASE_W);
		tim1_pwm_set_duty_percent(duty_cycle, PHASE_U);
		tim1_pwm_set_duty_percent(0, PHASE_V);
		HAL_DELAY(delay_ms);

		// Step 2
		phase_enable(PHASE_U);
		phase_enable(PHASE_V);
		phase_disable(PHASE_W);
		tim1_pwm_set_duty_percent(0, PHASE_U);
		tim1_pwm_set_duty_percent(duty_cycle, PHASE_V);
		HAL_DELAY(delay_ms);
	}

}
