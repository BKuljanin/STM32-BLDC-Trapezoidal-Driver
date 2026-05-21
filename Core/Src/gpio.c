// Setting up PC10,11,12 for EN for phases U,V,W. Nucleo IHM07 user manual p14
#include "gpio.h"

void en_uvw_init(void)
{
	// Enable clock access to GPIOC
	RCC->AHB1ENR |= GPIOCEN; // Datasheet p16 GPIOA is connected to AHB1

	// Set PC10,11,12 mode as GPIO as per datasheet p57 (AF1)
	// PC10
	GPIOC->MODER |= (1U<<20); 	// Reference manual p186 01 for GPIO mode
	GPIOC->MODER &=~ (1U<<21);  // For PC10 bits 21 and 20 are configured to 01 for GPIO mode
	// PC11
	GPIOC->MODER |= (1U<<22); 	// Reference manual p186 01 for GPIO mode
	GPIOC->MODER &=~ (1U<<23);  // For PC10 bits 21 and 20 are configured to 01 for GPIO mode
	// PC12
	GPIOC->MODER |= (1U<<24); 	// Reference manual p186 01 for GPIO mode
	GPIOC->MODER &=~ (1U<<25);  // For PC10 bits 21 and 20 are configured to 01 for GPIO mode

	// PC9 for bringing GPIO_BEMF low to measure back emf
	/* IHM07 reference manual p14 BEMF six step, p9 BEMF detection circuit
	 * Detection circuit consists of voltage divider, resistors 10kOhm and 2.2kOhm.
	 * Voltage of floating phase is scaled to 0-3.3V range. PC9 (GPIO_BEMF) is set LOW
	 * when the bemf should be sampled (to close the circuit) */

	GPIOC->MODER |= (1U<<18);
	GPIOC->MODER &=~ (1U<<19);

}
