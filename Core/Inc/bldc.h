#ifndef INC_BLDC_H_
#define INC_BLDC_H_

#include "main.h"
#include "pwm.h"

#define MAX_DUTY_CYCLE 50

typedef enum
{
    PHASE_U = 0,
    PHASE_V,
    PHASE_W

} BLDC_Phase_t;


#endif /* INC_BLDC_H_ */
