#ifndef INC_AS5600_H_
#define INC_AS5600_H_

#include <stdint.h>
#include "stm32f4xx.h"

#define PI 3.14159265f

#define ANGLE_LP_CUTOFF 30  // [Hz]
#define SPEED_LP_CUTOFF 20  // [Hz]

// AS5600 PWM cycle: [128 always LOW][4095 angle][128 always HIGH] = 4351 total
// Even at 0 degrees the duty cycle is ~3%, never fully LOW.
// Even at 360 degrees the duty cycle is ~97%, never fully HIGH.
// This way we can always tell the sensor is alive and outputting.
#define AS5600_DCL_TOTAL   4351.0f
#define AS5600_DCL_PADDING 128.0f
#define AS5600_DCL_ANGLE   4095.0f

extern volatile uint32_t pulse_width;
extern volatile uint32_t pulse_period;

void as5600_pwm_to_angle(void);
void as5600_calculate_speed(void);

typedef struct {
    float angle;
    float angle_previous;
    float angular_speed;
} as5600;

extern as5600 encoder;

#endif /* INC_AS5600_H_ */
