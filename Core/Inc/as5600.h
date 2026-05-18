#ifndef INC_AS5600_H_
#define INC_AS5600_H_

#include <stdint.h>
#include "stm32f4xx.h"

#define PI 3.14159265f

#define ANGLE_LP_CUTOFF 30  // [Hz]
#define SPEED_LP_CUTOFF 20  // [Hz]

#define AS5600_SAMPLE_PERIOD_S 0.001f  // 1 kHz TIM3 trigger rate

typedef struct {
    float angle;
    float angle_previous;
    float angular_speed;
} as5600;

extern as5600 encoder;
extern volatile uint8_t measurement_ready;

void as5600_trigger_read(void);
void as5600_calculate_speed(void);
void as5600_set_reference(void);

#endif /* INC_AS5600_H_ */
