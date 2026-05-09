#include "bldc.h"

void phase_enable(BLDC_Phase_t phase)
{

	switch(phase)
	    {
	    case PHASE_U:
	    	GPIOC->ODR |= (1U<<10);
	    	break;

	    case PHASE_V:
	    	GPIOC->ODR |= (1U<<11);
	    	break;

	    case PHASE_W:
	    	GPIOC->ODR |= (1U<<12);
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
		    	GPIOC->ODR &= ~(1U<<10);
		    	break;

		    case PHASE_V:
		    	GPIOC->ODR &= ~(1U<<11);
		    	break;

		    case PHASE_W:
		    	GPIOC->ODR &= ~(1U<<12);
		       break;

		    default:
		    	break;
		    }
}
