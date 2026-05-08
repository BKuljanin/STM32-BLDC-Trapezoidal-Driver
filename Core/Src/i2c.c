#include "stm32f4xx_hal.h"
#include "i2c.h"

I2C_HandleTypeDef hi2c1;

void as5600_init(void)
{
	uint8_t conf = AS5600_PWM_MODE | AS5600_PWM_920_HZ;

	HAL_I2C_Mem_Write(&hi2c1, AS5600_ADDRESS << 1, AS5600_CONF_1_REG, I2C_MEMADD_SIZE_8BIT, &conf, 1, 100);
}

void MX_I2C1_Init(void)
{

  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }


}
