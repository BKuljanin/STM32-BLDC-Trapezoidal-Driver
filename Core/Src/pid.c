#include "timer.h"

float pi_controller(float setpoint, float measurement,
		float Kp, float Ki,
		float integral_saturation,
		float output_upper_saturation, float output_lower_saturation, float dt)
{
	static float integral = 0.0f;

	float error = setpoint - measurement;

	integral += error * dt;

	float i_term = integral * Ki;

	if (i_term > integral_saturation)
	{
		i_term = integral_saturation;
		integral = integral_saturation/Ki;
	}
	if (i_term < - integral_saturation)
	{
		i_term = - integral_saturation;
		integral = - integral_saturation/Ki;
	}

	float out = Kp * error + i_term;

	if (out > output_upper_saturation)
	{
		out = output_upper_saturation;
	}
	if (out < output_lower_saturation)
	{
		out = output_lower_saturation;
	}

	return out;

}
