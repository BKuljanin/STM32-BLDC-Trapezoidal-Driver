#ifndef INC_BLDC_H_
#define INC_BLDC_H_

#include "main.h"

#define MAX_DUTY_CYCLE 50

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

#endif /* INC_BLDC_H_ */
