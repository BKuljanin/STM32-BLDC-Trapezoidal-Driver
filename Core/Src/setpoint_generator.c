#include "setpoint_generator.h"

float ramp_speed_setpoint(float target, float ramp_rate_deg_s2, float dt)
  {
      static float current = 0.0f;
      float step = ramp_rate_deg_s2 * dt;					// Calculating step (rate * dt)

      if (current < target) current = fminf(current + step, target);
      else if (current > target) current = fmaxf(current - step, target);

      return current;
  }
