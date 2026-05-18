#include "as5600.h"
#include "i2c.h"

as5600 encoder;
volatile uint8_t measurement_ready;

static uint8_t i2c_rx_buf[2];

static float lp_filter(float signal, float LP_cutoff, float delta_t, float *state)
{
    float alpha = 2 * PI * delta_t * LP_cutoff / (1 + 2 * PI * delta_t * LP_cutoff);
    *state = alpha * signal + (1 - alpha) * (*state);
    return *state;
}

// Called from TIM3 ISR at 1 kHz — starts non-blocking I2C read
void as5600_trigger_read(void)
{
    if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_READY) {
        HAL_I2C_Mem_Read_IT(&hi2c1, AS5600_ADDRESS << 1, AS5600_RAW_ANGLE_REG,
                            I2C_MEMADD_SIZE_8BIT, i2c_rx_buf, 2);
    }
}

// Called by HAL when I2C read completes
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    encoder.angle_previous = encoder.angle;

    // Raw 12-bit angle: reg 0x0C[3:0] = bits 11:8, reg 0x0D = bits 7:0
    uint16_t raw = ((uint16_t)(i2c_rx_buf[0] & 0x0F) << 8) | i2c_rx_buf[1];
    float angle = (float)raw * 360.0f / 4096.0f;

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
        measurement_ready = 1;
        return;
    }

    // Unwrap to continuous so the LP filter doesn't see 360-0 jumps
    float delta = angle - prev_raw_angle;
    if (delta >  180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    prev_raw_angle = angle;
    unwrapped += delta;

    float filtered = lp_filter(unwrapped, ANGLE_LP_CUTOFF, AS5600_SAMPLE_PERIOD_S, &angle_filter_state);

    // Wrap back to 0-360
    filtered = filtered - 360.0f * (int)(filtered / 360.0f);
    if (filtered < 0.0f) filtered += 360.0f;

    encoder.angle = filtered;
    measurement_ready = 1;
}

void as5600_calculate_speed(void)
{
    static uint8_t skip = 50;
    if (skip > 0) {
        skip--;
        return;
    }

    float delta_angle = encoder.angle - encoder.angle_previous;
    if (delta_angle >  180.0f) delta_angle -= 360.0f;
    if (delta_angle < -180.0f) delta_angle += 360.0f;

    float angular_speed_raw = delta_angle / AS5600_SAMPLE_PERIOD_S;

    static float speed_filter_state;
    encoder.angular_speed = lp_filter(angular_speed_raw, SPEED_LP_CUTOFF,
                                      AS5600_SAMPLE_PERIOD_S, &speed_filter_state);
}
