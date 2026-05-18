#ifndef INC_I2C_H_
#define INC_I2C_H_
#include "stm32f4xx_hal.h"
#include "main.h"

#define AS5600_ADDRESS       (0x36)
#define AS5600_RAW_ANGLE_REG (0x0C)

extern I2C_HandleTypeDef hi2c1;

void as5600_init(void);
void MX_I2C1_Init(void);

#endif /* INC_I2C_H_ */
