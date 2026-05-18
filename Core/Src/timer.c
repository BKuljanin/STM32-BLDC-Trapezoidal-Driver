#include "stm32f4xx.h"
#include "timer.h"
#include "as5600.h"

#define TIM3EN   (1U<<1)
#define TIM2EN   (1U<<0)
#define CR1_CEN  (1U<<0)
#define DIER_UIE (1U<<0)
#define SR_UIF   (1U<<0)

// TIM3 periodic interrupt at 1 kHz — triggers I2C angle read
void tim3_1khz_it_init(void)
{
    RCC->APB1ENR |= TIM3EN;

    // APB1 timer clock = 50 MHz (APB1 = 25 MHz, x2 because APB prescaler != 1)
    // 50 MHz / 50 / 1000 = 1 kHz
    TIM3->PSC = 50 - 1;
    TIM3->ARR = 1000 - 1;

    TIM3->DIER |= DIER_UIE;
    TIM3->CR1  |= CR1_CEN;

    NVIC_EnableIRQ(TIM3_IRQn);
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & SR_UIF) {
        TIM3->SR &= ~SR_UIF;
        as5600_trigger_read();
    }
}

// TIM2: free-running 1 MHz counter for control loop timing
void tim2_1mhz_init(void)
{
    RCC->APB1ENR |= TIM2EN;

    TIM2->PSC = 50 - 1;
    TIM2->ARR = 0xFFFFFFFF;
    TIM2->CR1 = CR1_CEN;
}

uint32_t read_time_us(void)
{
    return TIM2->CNT;
}

uint32_t tim2_get_delta_us(void)
{
    static uint32_t last_time = 0;
    static uint8_t first = 1;
    uint32_t now = TIM2->CNT;
    if (first) {
        first = 0;
        last_time = now;
        return 0;
    }
    uint32_t delta = now - last_time;
    last_time = now;
    return delta;
}
