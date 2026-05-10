#include "timer.h"

float pi_controller(float setpoint, float measurement,
		float Kp, float Ki,
		float integral_saturation,
		float output_upper_saturation, float output_lower_saturation)
{
	static float integral = 0.0f;

	float dt = tim2_get_delta_us() / 1000000.0f;  //  Convert microseconds to seconds;

	float error = setpoint - measurement;

	integral += error * dt;

	if (integral > integral_saturation)
	{
		integral = integral_saturation;
	}
	if (integral < - integral_saturation)
	{
		integral = - integral_saturation;
	}

	float out = Kp * error + Ki * integral;

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
