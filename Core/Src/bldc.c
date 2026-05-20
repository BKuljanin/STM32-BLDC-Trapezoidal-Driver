#include "bldc.h"
#include "pwm.h"
#include "adc.h"
#include "as5600.h"
#include "timer.h"

uint8_t volatile step = 0;  // current commutation step (exposed for debugging)
float electrical_angle;
uint32_t commutation_done;
BLDC_Direction_t bldc_direction = BLDC_REVERSE;

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
    electrical_angle = ea;

    uint8_t sector = (uint8_t)(ea / 60.0f) % 6;
    uint8_t new_step = (sector + 1) % 6;

    if (new_step != step) {
        float lower = (float)sector * 60.0f;
        float depth_lo = ea - lower;
        if (depth_lo >= STEP_HYSTERESIS_DEG)	// Prevents oscillations between 2 phases due to encoder noise
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
        static uint32_t blank_until = 0;
        static uint32_t crossing_time = 0;
        static uint32_t commutation_time = 0;
        static uint32_t half_time = 0;

        float bemf = floating_phase_back_emf;	// Take latest back EMF reading from floating phase

        if (read_time_us() < blank_until)
        {
            // blanking period after commutation - ignore BEMF
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
                blank_until = read_time_us() + BEMF_BLANK_US;
			}


        }
    }
}

// Initialize (park) BLDC by bringing PWM to one phase, one is used as sink and one floating
void bldc_init(CommutationMode_t mode) {
      bldc_commutate(step_pwm[0], step_sink[0], step_float[0], ALIGN_DUTY_PERCENT);
      HAL_Delay(ALIGN_SETTLE_MS);
      if (mode == ENCODER_MODE) {
          measurement_ready = 0;
          while (!measurement_ready);
          as5600_set_reference();
      }
      step = 1;  // ea=0 deg - sector 0 - advanced step = 1
  }

// Drives BLDC like a stepper, cycling through 6 commutation steps with fixed delay. Only for testing
void bldc_test_run(uint32_t delay_ms, uint32_t duty_cycle)
{
    while(1)
    {
        for (int s = 0; s < 6; s++) {
            step = s;
            bldc_commutate(step_pwm[s], step_sink[s], step_float[s], duty_cycle);
            back_emf_float_channel(step_float[s]);
            HAL_Delay(delay_ms);
        }
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
