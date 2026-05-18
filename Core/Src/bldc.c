#include "bldc.h"
#include "pwm.h"
#include "adc.h"
#include "as5600.h"
#include "timer.h"

uint8_t step = 0;  // current commutation step (exposed for debugging)
float electrical_angle;
static float   electrical_offset = 0.0f;
uint32_t commutation_done;

static void phase_enable(BLDC_Phase_t phase)
{
    switch(phase)
    {
    case PHASE_U:
        GPIOC->BSRR = (1U << 10);
        break;
    case PHASE_V:
        GPIOC->BSRR = (1U << 11);
        break;
    case PHASE_W:
        GPIOC->BSRR = (1U << 12);
        break;
    default:
        break;
    }
}

static void phase_disable(BLDC_Phase_t phase)
{
    switch(phase)
    {
    case PHASE_U:
        GPIOC->BSRR = (1U << (10 + 16));	// Reference manual p188. First 16 bits for setting next 16 for resetting. Atomic (no readaing)
        break;
    case PHASE_V:
        GPIOC->BSRR = (1U << (11 + 16));
        break;
    case PHASE_W:
        GPIOC->BSRR = (1U << (12 + 16));
        break;
    default:
        break;
    }
}

// pwm_phase = high side PWM, sink_phase = low side ON, float_phase = both OFF (floating)
static void bldc_commutate(BLDC_Phase_t pwm_phase, BLDC_Phase_t sink_phase, BLDC_Phase_t float_phase, uint32_t duty)
{
    phase_enable(pwm_phase);
    phase_enable(sink_phase);
    phase_disable(float_phase);

    tim1_pwm_set_duty_percent(duty, pwm_phase);
    tim1_pwm_set_duty_percent(0,    sink_phase);
    tim1_pwm_set_duty_percent(0,    float_phase);
}

static const BLDC_Phase_t step_pwm[6]   = {PHASE_U, PHASE_U, PHASE_V, PHASE_V, PHASE_W, PHASE_W};
static const BLDC_Phase_t step_sink[6]  = {PHASE_V, PHASE_W, PHASE_W, PHASE_U, PHASE_U, PHASE_V};
static const BLDC_Phase_t step_float[6] = {PHASE_W, PHASE_V, PHASE_U, PHASE_W, PHASE_V, PHASE_U};

void bldc_update_step(void)
{
    float ea = encoder.angle * BLDC_POLE_PAIRS;
    while (ea >= 360.0f) ea -= 360.0f;
    ea -= electrical_offset;
    while (ea < 0.0f) ea += 360.0f;
    electrical_angle = ea;

    uint8_t new_step = (uint8_t)(ea / 60.0f) % 6;
    if (new_step != step) {
        float lower = (float)new_step * 60.0f;
        float depth_lo = ea - lower;
        float depth_hi = lower + 60.0f - ea;
        float depth = depth_lo < depth_hi ? depth_lo : depth_hi;
        if (depth >= STEP_HYSTERESIS_DEG)
            step = new_step;
    }
}

void bldc_run(uint32_t duty, CommutationMode_t mode)
{
    static float   bemf_previous    = 0.0f;
    static uint8_t crossed          = 0;

    // Always called when bldc_run is called, will not do anything if step has not changed from last iteration
    bldc_commutate(step_pwm[step], step_sink[step], step_float[step], duty);
    back_emf_float_channel(step_float[step]);

    if (mode == BEMF_MODE)
    {
        static uint8_t blank = BEMF_BLANK_SAMPLES;	// See below for description of blank
        static uint32_t crossing_time = 0;
        static uint32_t commutation_time = 0;
        static uint32_t half_time = 0;

        float bemf = floating_phase_back_emf;	// Take latest back EMF reading from floating phase

        if (blank > 0)
        {
            blank--;	// Due to noise we can falsely detect several crossings around 0V
        }
        else if (!crossed)	// If no crossing has happened check if there is a crossing now
        {
            if ((bemf_previous < 0.0f && bemf >= 0.0f) ||	// Check if there is a crossing
                (bemf_previous >= 0.0f && bemf < 0.0f))
            {
                crossed = 1;

                // Reading crossing time in us
                crossing_time = read_time_us();

                // Scheduling next commutation
                if (commutation_time == 0) {	// This is just for startup since its first commutation. (commutation_time is 0 here)
                      half_time = 3000;  // 3ms last open loop step delay.
                  } else {
                      half_time = crossing_time - commutation_time;	// After startup only this equation decides
                  }

            }


        }

        bemf_previous = bemf;	// Saving back EMF value as previous

        if (crossed)
        {

            // Now we check if time for commutation has passed
        	uint32_t current_time = read_time_us();

        	if(crossing_time + half_time <= current_time)
			{

        		// Reading commutation time in us
        		commutation_time = read_time_us();

        		// Calculating next step, this will cause commutation (called in the beginning of this function)
                step = (step + 1) % 6;

                // Resetting crossed flag
                crossed = 0;

                // This prevents frequent reading of crossing because of noise around 0 V
                blank   = BEMF_BLANK_SAMPLES;	// We cant cross again if we crossed once, resetting blank
			}


        }
    }
}

// Initialize (park) BLDC by bringing PWM to one phase, one is used as sink and one floating
void bldc_init(void) {
      bldc_commutate(step_pwm[0], step_sink[0], step_float[0], ALIGN_DUTY_PERCENT);
      HAL_Delay(ALIGN_SETTLE_MS);
      measurement_ready = 0;
      while (!measurement_ready);  // Wait for fresh I2C angle reading
      electrical_offset = encoder.angle * BLDC_POLE_PAIRS;

      /* If electrical angle is above 360 degrees (which it most likely is since elec_angle = mech_angle * BLDC_POLE_PAIRS
      * We find electrical offset by reducing by full circle in order to get electrical offset between 0 and 360 deg */
      while (electrical_offset >= 360.0f) electrical_offset -= 360.0f;
  }

// Drives BLDC like a stepper, cycling through 6 commutation steps with fixed delay. Only for testing
void bldc_test_run(uint32_t delay_ms, uint32_t duty_cycle)
{
    while(1)
    {
        bldc_commutate(PHASE_U, PHASE_V, PHASE_W, duty_cycle);  // Step 1
        HAL_Delay(delay_ms);

        bldc_commutate(PHASE_U, PHASE_W, PHASE_V, duty_cycle);  // Step 2
        HAL_Delay(delay_ms);

        bldc_commutate(PHASE_V, PHASE_W, PHASE_U, duty_cycle);  // Step 3
        HAL_Delay(delay_ms);

        bldc_commutate(PHASE_V, PHASE_U, PHASE_W, duty_cycle);  // Step 4
        HAL_Delay(delay_ms);

        bldc_commutate(PHASE_W, PHASE_U, PHASE_V, duty_cycle);  // Step 5
        HAL_Delay(delay_ms);

        bldc_commutate(PHASE_W, PHASE_V, PHASE_U, duty_cycle);  // Step 6
        HAL_Delay(delay_ms);
    }
}

// Drives BLDC in open loop to have back emf
void bldc_open_loop_run(uint32_t duty_cycle)
  {
      uint32_t delay_ms = 20;  // Start slow

      for (uint32_t i = 0; i < OPEN_LOOP_COM_NR; i++)
      {
          for (int s = 0; s < 6; s++) {
              step = s;  // keep global step in sync so BEMF mode picks up from here
              bldc_commutate(step_pwm[step], step_sink[step], step_float[step], duty_cycle);
              back_emf_float_channel(step_float[step]);
              commutation_done = 1;
              HAL_Delay(delay_ms);
              commutation_done = 0;
          }
          if (delay_ms > 2) delay_ms--;  // ramp up speed
      }
  }

void test_adc_ch_switch(void)
{
	uint32_t delay_switch = 5000;
	uint8_t step = 0;
	while(1) {

	            // keep global step in sync so BEMF mode picks up from here
	          bldc_commutate(step_pwm[step], step_sink[step], step_float[step], 20);
	          back_emf_float_channel(step_float[step]);
	          commutation_done = 1;
	          HAL_Delay(delay_switch);
	          commutation_done = 0;
	          step = (step + 1) % 6;
	          }
}
