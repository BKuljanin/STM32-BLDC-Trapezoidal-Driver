#ifndef INC_PID_H_
#define INC_PID_H_

#define PI_KP 0.1f
#define PI_KI 0.01f
#define PI_INTEGRAL_SAT 50.0f
#define PI_OUTPUT_SAT_LOWER 0.0f
#define PI_OUTPUT_SAT_UPPER 50.0f

float pi_controller(float setpoint, float measurement,
		float Kp, float Ki,
		float integral_saturation,
		float output_upper_saturation, float output_lower_saturation);

#endif /* INC_PID_H_ */
