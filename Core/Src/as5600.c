#include "as5600.h"

as5600 encoder;

static float lp_filter(float signal, float LP_cutoff, float delta_t, float *state)
{
	float alpha = 2 * PI * delta_t * LP_cutoff / (1 + 2 * PI * delta_t * LP_cutoff);

	*state = alpha * signal + (1-alpha) * (*state);

	return *state;
}

void as5600_pwm_to_angle(void)
{
	encoder.angle_previous = encoder.angle;

	// Snapshot both together so an interrupt cant update one without the other
	__disable_irq();
	uint32_t width = pulse_width;
	uint32_t period = pulse_period;
	__enable_irq();

	if (period < 900) period = 900;

	// Remove the 128 tick bias so 0 degrees maps to 0 and 360 maps to 360
	// AS5600 PWM cycle: [128 always LOW][4095 angle][128 always HIGH] = 4351 total
	// Even at 0 degrees the duty cycle is ~3%, never fully LOW.
	// Even at 360 degrees the duty cycle is ~97%, never fully HIGH.
	// This way we can always tell the sensor is alive and outputting.
	float duty = (float)width / (float)period;
	float raw = duty * AS5600_DCL_TOTAL - AS5600_DCL_PADDING;

	if (raw < 0.0f) raw = 0.0f;
	if (raw > AS5600_DCL_ANGLE) raw = AS5600_DCL_ANGLE;

	float angle = (raw / AS5600_DCL_ANGLE) * 360.0f;

	static float prev_raw_angle;
	static float unwrapped;
	static float angle_filter_state;
	static uint8_t first = 1;

	if (first) {
		first = 0;
		prev_raw_angle = angle;
		unwrapped = angle;
		angle_filter_state = angle;
		encoder.angle_previous = angle;
		encoder.angle = angle;
		return;
	}

	// Unwrap angle to continuous values so the filter doesnt see 360 to 0 jumps
	float delta = angle - prev_raw_angle;
	if (delta > 180.0f) delta -= 360.0f;
	if (delta < -180.0f) delta += 360.0f;
	prev_raw_angle = angle;
	unwrapped += delta;

	// Filter the continuous angle
	float filtered = lp_filter(unwrapped, ANGLE_LP_CUTOFF, (float)period / 1e6f, &angle_filter_state);

	// Wrap back to 0-360
	filtered = filtered - 360.0f * (int)(filtered / 360.0f);
	if (filtered < 0.0f) filtered += 360.0f;

	encoder.angle = filtered;
}

void as5600_calculate_speed(void)
{
	// Let angle settle before computing speed
	static uint8_t skip = 50;
	if (skip > 0) {
		skip--;
		return;
	}

	uint32_t period = pulse_period;
	if (period < 900) period = 900;

	// Wrap aware delta, if prev ~360 and current ~0 (or vice versa) compute actual small movement
	float delta_angle = encoder.angle - encoder.angle_previous;
	if (delta_angle > 180.0f) delta_angle -= 360.0f;
	if (delta_angle < -180.0f) delta_angle += 360.0f;

	float angular_speed_raw = delta_angle / ((float)period / 1e6f);	// Converting pulse width from us to s to get [deg/s]

	// Second stage filter on speed
	static float speed_filter_state;
	encoder.angular_speed = lp_filter(angular_speed_raw, SPEED_LP_CUTOFF, (float)period / 1e6f, &speed_filter_state);
}
