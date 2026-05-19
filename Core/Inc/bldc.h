#ifndef INC_BLDC_H_
#define INC_BLDC_H_

#include "main.h"
#include "timer.h"

#define MAX_DUTY_CYCLE      50
#define BLDC_POLE_PAIRS     7 	 	// Number of poles of BLDC. In this project A2208/14T 1400KV was used
#define BEMF_BLANK_US       500   	// 500µs blanking after commutation
#define ALIGN_DUTY_PERCENT  15   	// Duty used to park rotor during alignment
#define ALIGN_SETTLE_MS     2000  	// Time to wait for rotor to settle

#define OPEN_LOOP_COM_NR	300		// Number of commutations in open loop
#define OPEN_LOOP_DUTY		10		// Duty cycle of open loop phase

#define STEP_HYSTERESIS_DEG  1.0f

extern volatile uint8_t step;
extern float electrical_angle;
extern uint32_t commutation_done;

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

typedef enum {
    BLDC_FORWARD = 0,
    BLDC_REVERSE = 1
} BLDC_Direction_t;

extern BLDC_Direction_t bldc_direction;

void bldc_update_step(void);
void bldc_test_run(uint32_t delay_ms, uint32_t duty_cycle);
void bldc_run(uint32_t duty, CommutationMode_t mode);
void bldc_init(CommutationMode_t mode);
void bldc_open_loop_run(uint32_t duty_cycle);
void test_adc_ch_switch(void);

#endif /* INC_BLDC_H_ */
