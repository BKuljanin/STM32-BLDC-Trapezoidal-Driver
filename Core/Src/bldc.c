#include "bldc.h"
#include "pwm.h"

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
