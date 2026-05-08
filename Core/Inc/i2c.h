#ifndef INC_I2C_H_
#define INC_I2C_H_
#include "stm32f4xx_hal.h"

#define AS5600_ADDRESS (0x36)		// AS5600 I2C address

#define AS5600_CONF_1_REG (0x08) 	// Configuration register 2

#define AS5600_PWM_MODE (0x20)	 	// PWM mode of the encoder
#define AS5600_PWM_920_HZ (0xC0)	// PWM frequency 920 Hz

extern I2C_HandleTypeDef hi2c1;

void as5600_init(void);
void MX_I2C1_Init(void);

#endif /* INC_I2C_H_ */
