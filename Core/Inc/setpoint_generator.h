#ifndef INC_SETPOINT_GENERATOR_H_
#define INC_SETPOINT_GENERATOR_H_

#include "timer.h"
#include "math.h"

float ramp_speed_setpoint(float target, float ramp_rate_deg_s2, float dt);

#endif /* INC_SETPOINT_GENERATOR_H_ */
