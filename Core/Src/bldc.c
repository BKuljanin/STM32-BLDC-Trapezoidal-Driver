#include "bldc.h"
#include "pwm.h"
#include "adc.h"
#include "as5600.h"

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

void bldc_run(uint32_t duty, CommutationMode_t mode)
{
    static uint8_t initialized      = 0;
    static float   electrical_offset = 0.0f;
    static uint8_t step             = 0;
    static float   bemf_previous    = 0.0f;
    static uint8_t crossed          = 0;

    if (!initialized) // Parks the rotor in initial position at startup
    {
        bldc_commutate(step_pwm[0], step_sink[0], step_float[0], ALIGN_DUTY_PERCENT);
        HAL_Delay(ALIGN_SETTLE_MS);
        as5600_pwm_to_angle();
        electrical_offset = encoder.angle * BLDC_POLE_PAIRS;

        /* If electrical angle is above 360 degrees (which it most likely is since elec_angle = mech_angle * BLDC_POLE_PAIRS
         * We find electrical offset by reducing by full circle in order to get electrical offset between 0 and 360 deg
         */
        while (electrical_offset >= 360.0f) electrical_offset -= 360.0f;
        initialized = 1;
    }

    bldc_commutate(step_pwm[step], step_sink[step], step_float[step], duty);
    back_emf_float_channel(step_float[step]);

    if (mode == ENCODER_MODE)
    {
    	// Calculate electrical angle
        float electrical_angle = encoder.angle * BLDC_POLE_PAIRS;
        // Wrap electrical angle so its between 0 and 360 deg
        while (electrical_angle >= 360.0f) electrical_angle -= 360.0f;
        // Apply calculated offset on wrapped angle
        electrical_angle -= electrical_offset;
        // Wrap if after offset angle is negative
        while (electrical_angle < 0.0f) electrical_angle += 360.0f;
        // Calculate step (1-6)
        step = (uint8_t)(electrical_angle / 60.0f) % 6;
    }
    else if (mode == BEMF_MODE)
    {
        static uint8_t blank = 0;

        if (blank > 0)
        {
            blank--;
        }
        else
        {
            float bemf = floating_phase_back_emf;

            if ((bemf_previous < 0.0f && bemf >= 0.0f) ||
                (bemf_previous >= 0.0f && bemf < 0.0f))
            {
                crossed = 1;
            }

            bemf_previous = bemf;
        }

        if (crossed)
        {
            // TODO: wait 30° electrical before advancing step
            step    = (step + 1) % 6;
            crossed = 0;
            blank   = BEMF_BLANK_SAMPLES;
        }
    }
}

// Drives BLDC like a stepper, cycling through 6 commutation steps with fixed delay
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
