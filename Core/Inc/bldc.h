#ifndef INC_BLDC_H_
#define INC_BLDC_H_

#include "main.h"

#define MAX_DUTY_CYCLE      50
#define BLDC_POLE_PAIRS     7 	 // Number of poles of BLDC. In this project A2208/14T 1400KV was used
#define BEMF_BLANK_SAMPLES  10   // 10 samples × 50µs = 500µs blanking after commutation
#define ALIGN_DUTY_PERCENT  15   // Duty used to park rotor during alignment
#define ALIGN_SETTLE_MS     2000  // Time to wait for rotor to settle

extern uint8_t step;
extern float electrical_angle;

typedef enum
{
    PHASE_U = 0,
    PHASE_V,
    PHASE_W

} BLDC_Phase_t;

typedef enum {
      ENCODER_MODE,
      BEMF_MODE
} CommutationMode_t;

void bldc_test_run(uint32_t delay_ms, uint32_t duty_cycle);
void bldc_run(uint32_t duty, CommutationMode_t mode);

#endif /* INC_BLDC_H_ */
