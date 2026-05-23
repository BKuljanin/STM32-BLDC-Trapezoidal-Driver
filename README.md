# STM32 BLDC Trapezoidal Driver
### Six step Commutation + PI Speed Control + BEMF Sensorless (NUCLEO-F446RE + IHM07M1)

**Note:**
This project implements a brushless DC (BLDC) motor controller using **trapezoidal (six step) commutation** on the STM32F446RE and STM32 X-Nucleo IHM07 BLDC driver board. All peripheral drivers are **register-level** (TIM, ADC, I2C, GPIO). The firmware supports two commutation modes: **encoder based closed-loop** control using the AS5600 magnetic encoder over I2C, and **sensorless back EMF (BEMF)** zero crossing detection. 

---

## This project demonstrates

- Register level **TIM1 center aligned PWM** at 20 kHz for three phase gate driving
- **Six step trapezoidal commutation**
- **ADC1 injected channel** triggered at the center of the PWM pulse (ARR trigger via TIM1_CH4) for stable back EMF sampling
- **Back-EMF zero crossing detection** for sensorless commutation
- **AS5600 14 bit absolute magnetic encoder** via I2C1 at 1 kHz (TIM3 interrupt driven)
- Encoder **angle unwrapping** and **low pass filtering**
- Angular **speed estimation** from filtered encoder angle deltas with LP filter
- **PI speed controller**
- **Open loop startup** sequence for BEMF mode
- IHM07M1 motor shield **BEMF voltage divider** scaling and ADC offset correction

---

## Output Signals

This project produces:

- Motor **angular position** in **deg**
- Motor **angular speed** in **deg/s**
- **PI controller duty cycle output** in **%** (0–50%)
- **Back-EMF voltage** of the floating phase in **V** (converted from 12 bit ADC, scaled on IHM07 divider)
- **Commutation step** (0–5) defined by current electrical sector
- **Electrical angle** in **deg** (derived from encoder angle × pole pairs)

---

## Hardware Overview

### MCU: STM32F446RET6 (NUCLEO-F446RE)
- Core: ARM Cortex-M4

### Motor Driver: IHM07M1 (ST Motor Control Shield)
- Three half bridges for three phase BLDC
- Integrated BEMF detection circuit with voltage divider (10 kΩ + 2.2 kΩ)
- Phase enable pins control each gate driver independently

### Motor: A2208/14T 1400 KV Brushless Outrunner
- **Pole pairs:** 7

### Encoder: AS5600 (14 bit absolute magnetic, I2C)
- 12 bit output resolution: 4096 counts/revolution

### Power Supply
- VBUS: **12 V**

  ---

## High-Level Flow

### Encoder Mode (Closed Loop PI Control)

```
TIM3 IRQ (1 kHz)
  └─ as5600_trigger_read()        // start I2C read

I2C completion
  └─ measurement_ready = 1        // flag for main loop

Main loop (runs immediately when measurement_ready)
  ├─ as5600_calculate_speed()     // delta-angle → filtered speed
  ├─ dt = tim2_get_delta_us()     // time since last iteration
  ├─ ramp setpoint (100 deg/s²)
  ├─ duty = pi_controller(setpoint, speed, Kp, Ki, dt)
  └─ bldc_run(duty, ENCODER_MODE)
       └─ bldc_update_step()      // angle → sector → step → commutate
```

### BEMF Mode (Sensorless)

```
TIM1 CH4 (20 kHz, at ARR)
  └─ ADC1 injected trigger
       └─ ADC_IRQHandler
            └─ floating_phase_back_emf = (raw/4095 × 3.3 × 5.55) − 6V

Main loop (continuous)
  └─ bldc_run(20, BEMF_MODE)
       ├─ [blanked for short duration after commutation]
       ├─ detect sign change in floating_phase_back_emf
       ├─ crossing_time = read_time_us()
       ├─ half_time = crossing_time − last_commutation_time
       └─ when current_time ≥ crossing_time + half_time:
            ├─ step = (step + 1) % 6
            └─ bldc_commutate(step, duty)
```

### Open-Loop Startup (BEMF Prerequisite)

```
bldc_open_loop_run(duty)
  └─ for n steps:
       ├─ delay from 20 ms → 2 ms (linear ramp)
       ├─ step = (step + 1) % 6
       └─ bldc_commutate(step, duty)
  → motor reaches ~2000 RPM → hand off to BEMF loop
```

---

## Clock Configuration

| Clock   | Frequency | Notes                                   |
|--------:|----------:|-----------------------------------------|
| HSI     | 16 MHz    | Internal oscillator (PLL source)        |
| PLL     | —         | M=8, N=100, P=2                         |
| SYSCLK  | 100 MHz   |                                         |
| AHB     | 100 MHz   | DIV1                                    |
| APB2    | 50 MHz    | DIV2 (TIM1, ADC1)                       |
| APB1    | 25 MHz    | DIV4 (I2C1, TIM2, TIM3)                 |
| APB2 Timer Clock | 100 MHz | ×2 multiplier active           |
| APB1 Timer Clock | 50 MHz  | ×2 multiplier active           |

---

## Pinout

### PWM & Phase Enable

| Signal   | Pin  | Peripheral     | Description                  |
|---------:|------|---------------|------------------------------|
| PWM_U    | PA8  | TIM1_CH1      | Phase U high side PWM        |
| PWM_V    | PA9  | TIM1_CH2      | Phase V high side PWM        |
| PWM_W    | PA10 | TIM1_CH3      | Phase W high side PWM        |
| EN_U     | PC10 | GPIO Output   | Phase U gate driver enable   |
| EN_V     | PC11 | GPIO Output   | Phase V gate driver enable   |
| EN_W     | PC12 | GPIO Output   | Phase W gate driver enable   |
| GPIO_BEMF| PC9  | GPIO Output   | BEMF detection circuit enable|

### Back EMF ADC Inputs

| Phase | Pin | ADC Channel | Description               |
|------:|-----|-------------|---------------------------|
| U     | PC3 | ADC1_IN13   | Phase U back EMF voltage  |
| V     | PB0 | ADC1_IN8    | Phase V back EMF voltage  |
| W     | PA7 | ADC1_IN7    | Phase W back EMF voltage  |

### Encoder (AS5600, I2C1)

| Signal | Pin | Description            |
|-------:|-----|------------------------|
| SCL    | PB6 | I2C1 clock (100 kHz)  |
| SDA    | PB7 | I2C1 data             |

---

## PWM Configuration

| Parameter          | Value    | Notes                                              |
|-------------------:|---------:|-----------------------------------------------------|
| Frequency          | 20 kHz   |                                                     |
| Resolution         | 8 bit    | ARR = 255                                           |
| Mode               | Center aligned | CR1 CMS = 01 (counts up then down)          |
| Timer clock        | 100 MHz  | APB2 timer clock                                    |
| Prescaler          | 97       | PSC = (100 MHz / (20 kHz × 2 × 255)) − 1          |
| Max duty cycle     | 50%      | Limited duty cycle       |
| ADC trigger        | TIM1_CH4 | Toggle mode — fires ADC at ARR (pulse center)       |


Center aligned mode ensures the ADC is triggered exactly at the midpoint of the PWM pulse, where phase current has stabilized and noise is small.

---

## Commutation Table (Six-Step Sequence)

At each step, one phase is driven with PWM (high-side), one is pulled to ground (sink), and one is left floating for BEMF sensing.

| Step | PWM (drive) | Sink (GND) | Float (BEMF) | Electrical Sector |
|-----:|:-----------:|:----------:|:------------:|:-----------------:|
| 0    | U           | V          | W            | 0–60°             |
| 1    | U           | W          | V            | 60–120°           |
| 2    | V           | W          | U            | 120–180°          |
| 3    | V           | U          | W            | 180–240°          |
| 4    | W           | U          | V            | 240–300°          |
| 5    | W           | V          | U            | 300–360°          |

**Commutation action per phase:**
- **PWM phase:** Enable high side, apply duty cycle (CCR = `ARR − duty%`)
- **Sink phase:** Enable, set CCR = ARR (0% duty — low-side always ON)
- **Float phase:** Disable EN pin (both high and low MOSFETs OFF so the phase is floating)

---

## Commutation Modes

### Encoder Mode (Closed-Loop)

The AS5600 encoder provides absolute mechanical angle at 1 kHz. The firmware converts this to an electrical angle, determines the commutation sector, and applies the correct step:

```
electrical_angle = mechanical_angle × BLDC_POLE_PAIRS (7)
sector           = (electrical_angle + 30°) / 60°
step             = (sector + 1) % 6
```

The 30° advance corrects for the phase lead inherent in trapezoidal commutation. The commutation step is recalculated on every encoder reading, keeping the drive phase locked to rotor position regardless of speed.

The PI controller outputs a duty cycle (0–50%) based on the error between the speed setpoint and measured angular speed. A ramp generator limits acceleration to prevent step loss or overcurrent.

### BEMF Mode (Sensorless)

The floating phase generates a back EMF proportional to rotor speed. As the rotor passes the midpoint of each electrical sector, VEMF crosses zero. The firmware detects this crossing and schedules the next commutation halfway through the remaining sector:

```
crossing_time    = read_time_us()                         // zero crossing timestamp
half_time        = crossing_time − commutation_time       // half period estimate
next_commutation = crossing_time + half_time              // schedule next step
```

A **500 µs blanking period** suppresses false crossings from switching transients immediately after commutation.

**Open loop startup** spins the motor to a speed where BEMF amplitude is large enough to detect before handing off to sensorless tracking.

---

## ADC Configuration

| Parameter        | Value           | Notes                                           |
|-----------------:|----------------:|-------------------------------------------------|
| Resolution       | 12 bit          | 4095 counts                        |
| Mode             | Injected channel| Single conversion per trigger                   |
| Trigger source   | TIM1_CH4        | Both edges (JEXTEN = 11b)                       |
| Sampling time    | 28 ADC cycles   | ≈ 2.24 µs at 12.5 MHz ADC clock                |
| ADC clock        | 25 MHz          | APB2 / 2 (CR2 ADCPRE)                          |
| Channel selected | Per step        | Switches to the current floating phase channel   |
| ISR              | ADC_IRQHandler  | Reads JDR1 (injected ADC data register), converts, stores        |

**Voltage conversion:**

$$V_{bemf} = \frac{ADC_{raw}}{4095} \times 3.3\,\text{V} \times 5.55 - 6\,\text{V}$$

The −6 V offset centers the result on 0 V (VBUS/2 = 6 V), so a zero crossing reads as exactly 0 V.

**Zero crossing detection:** Sign change in `floating_phase_back_emf` between consecutive ADC samples.

---

## AS5600 Encoder Interface

| Parameter          | Value       | Notes                                         |
|-------------------:|------------:|-----------------------------------------------|
| Interface          | I2C1        | Register-level, 100 kHz                       |
| I2C address        | 0x36        | 7 bit                                         |
| Angle register     | 0x0C–0x0D  | 12 bit raw angle (MSB in [3:0] of 0x0C)      |
| Read rate          | 1 kHz       | TIM3 interrupt triggers non-blocking I2C read |
| Resolution         | 4096 cnt/rev| 0.0879° per count                          |
| Angle LP filter    | 30 Hz       | First order, removes quantization jitter      |
| Speed LP filter    | 20 Hz       | Applied to angle delta / dt                   |
| Speed units        | deg/s       |                                               |
| Direction          | Configurable| `BLDC_REVERSE` parameter determines direction measured        |

**Angle processing pipeline:**
1. Read raw 12 bit value from I2C
2. Convert to degrees: `raw × 360 / 4096`
3. Unwrap (detect and correct >180° jumps)
4. Apply LP filter
5. Re-wrap to 0–360° for commutation use
6. Compute delta to previous sample → raw angular speed → 20 Hz LP filter

---

## PI Speed Controller

| Parameter            | Value | Units  |
|---------------------:|------:|--------|
| Kp                   | 0.1   | %·s/deg|
| Ki                   | 0.01  | %/deg  |
| Integral saturation  | 50    | %      |
| Output lower limit   | 0     | %      |
| Output upper limit   | 50    | %      |
| Update rate          | 1 kHz | (encoder read rate) |


---

## Initialization Sequence

1. HAL_Init, SystemClock_Config (100 MHz SYSCLK via PLL)
2. I2C1 init → AS5600 init (encoder mode only)
3. GPIO init: EN_U, EN_V, EN_W (PC10–12), BEMF enable (PC9)
4. TIM1 PWM init (20 kHz, 8-bit, center aligned)
5. TIM1 CH4 toggle mode, ADC injected trigger
6. ADC1 injected channel init (BEMF sensing)
7. TIM2 init (1 MHz microsecond counter)
8. TIM3 init (1 kHz encoder read interrupt — encoder mode only)
9. **Motor parking:** Commutate to step 0 at 15% duty, wait for rotor to settle
10. **Reference zeroing** (encoder mode): Read first angle, call `as5600_set_reference()` to set 0 of the encoder
11. Enter main control loop


---

## Back EMF Sensing Circuit (IHM07M1)

The IHM07M1 includes a resistive voltage divider on each phase:

$$V_{ADC} = V_{phase} \times \frac{R_2}{R_1 + R_2} = V_{phase} \times \frac{2.2}{12.2} \approx \frac{V_{phase}}{5.55}$$

This maps the 0–12 V floating phase voltage into the 0–3.3 V ADC input range. A zero crossing of the back EMF (at VBUS/2 = 6 V) maps to an ADC code of ≈ 2048. After applying the conversion formula, `floating_phase_back_emf` crosses zero at the same moment as the actual BEMF zero crossing.

---

## File Structure

### `main.c`
System initialization and main control loops.
- Clock configuration (100 MHz SYSCLK via PLL: M=8, N=100, P=2)
- Peripheral initialization sequence (GPIO, I2C, TIM1, ADC, TIM2, TIM3)
- Encoder mode: 1 kHz closed-loop PI speed control with ramp
- BEMF mode: continuous sensorless commutation at fixed duty

---

### `bldc.h / bldc.c`
Core motor control logic.
- `bldc_init()` : Parks rotor, zeros encoder reference, sets initial step
- `bldc_commutate()` : Applies PWM/sink/float assignments for a given step
- `bldc_run()` : Run function, dispatches to encoder or BEMF logic
- `bldc_update_step()` : Converts encoder angle to electrical sector and commutation step
- `bldc_open_loop_run()` : Open loop ramp for BEMF startup
- `bldc_test_run()` : Fixed delay stepper mode for manual testing
- `bldc_noise_floor_test()` : Disables all phases, measures ADC noise on one channel
- `back_emf_float_channel()` : Returns the ADC channel number for the current floating phase

---

### `pwm.h / pwm.c`
TIM1 center-aligned PWM configuration.
- `tim1_pwm_init()` : Configures TIM1 CH1–3 (PA8–10) and CH4 (ADC trigger) at 20 kHz, 8 bit
- `tim1_pwm_set_duty_percent()` : Sets CCR with 50% hard ceiling
- `tim1_trig_adc()` : Configures CH4 in toggle mode to trigger ADC at ARR

---

### `adc.h / adc.c`
ADC1 injected channel for back-EMF sensing.
- `back_emf_adc_init()` : Configures ADC1, injected sequence, external trigger from TIM1_CH4
- `adc_select_channel()` : Switches injected sequence to the current floating phase channel
- `adc_to_volts()` : Converts raw 12 bit count to floating phase voltage (V), applying divider ratio and VBUS/2 offset
- `ADC_IRQHandler()` : JEOC ISR reads JDR1 (injected ADC data register), converts, updates `floating_phase_back_emf`

---

### `as5600.h / as5600.c`
AS5600 14 bit absolute magnetic encoder driver.
- `as5600_init()` : Verifies device presence, reads initial angle
- `as5600_trigger_read()` : Starts I2C burst read of angle registers (0x0C–0x0D)
- `as5600_process_reading()` : Converts raw count to degrees, unwraps, applies LP filter
- `as5600_calculate_speed()` : Computes angular speed from filtered angle delta, applies LP filter
- `as5600_set_reference()` : Records current angle as zero reference for the motor parking position

---

### `pid.h / pid.c`
Anti-windup PI speed controller.
- `pi_controller()` : Computes duty cycle from speed error with integral clamping and output saturation

---

### `setpoint_generator.h / setpoint_generator.c`
Speed ramp generator.
- `ramp_speed_setpoint()` : Advances current setpoint toward target at a fixed rate (deg/s²), called with measured dt

---

### `timer.h / timer.c`
TIM2 and TIM3 initialization and ISRs.
- `tim2_1mhz_init()` : Configures TIM2 as 1 MHz microsecond counter
- `tim3_1khz_it_init()` : Configures TIM3 at 1 kHz with interrupt
- `TIM3_IRQHandler()` : Triggers AS5600 I2C read every 1 ms
- `read_time_us()` : Returns current TIM2 count (µs)
- `tim2_get_delta_us()` : Returns elapsed time since last call (µs)

---

### `i2c.h / i2c.c`
Register-level I2C1 initialization for AS5600.
- `I2C1_init()` : Configures I2C1 on PB6/PB7 at 100 kHz (CR2=25, CCR=125, TRISE=26 for 25 MHz APB1)
- `I2C1_read_bytes()` : Read data bytes

---

### `gpio.h / gpio.c`
Phase enable GPIO configuration.
- `en_uvw_init()` : Configures PC10, PC11, PC12 as push-pull outputs (phase enable signals)
- `bemf_gpio_init()` : Configures PC9 for BEMF detection circuit enable

---

## Sampling Rates

| Signal                 | Rate    | Method                              |
|-----------------------:|--------:|-------------------------------------|
| PWM switching          | 20 kHz  | TIM1 center-aligned                 |
| Back-EMF ADC sampling  | 20 kHz  | TIM1_CH4 toggle → ADC injected (both edges) |
| Encoder angle read     | 1 kHz   | TIM3 interrupt → interrupt based I2C   |
| Speed calculation      | 1 kHz   | On every encoder reading            |
| PI controller update   | 1 kHz   | On every speed calculation          |
| BEMF zero crossing check  | 40 kHz  | Continuous in main loop after each ADC ISR |

---

## Key Constants

| Constant                | Value    | Units    | Purpose                                     |
|------------------------:|---------:|:--------:|---------------------------------------------|
| `BLDC_POLE_PAIRS`       | 7        | —        | A2208/14T motor parameter                   |
| `MAX_DUTY_CYCLE`        | 50       | %        | Hard output saturation                      |
| `ALIGN_DUTY_PERCENT`    | 15       | %        | Duty during rotor parking                   |
| `ALIGN_SETTLE_MS`       | 5000     | ms       | Time to wait for mechanical settling        |
| `BEMF_BLANK_US`         | 500      | µs       | Post commutation blanking window            |
| `OPEN_LOOP_COM_NR`      | 300      | steps    | Open loop startup commutation count         |
| `OPEN_LOOP_DUTY`        | 10       | %        | Startup duty cycle                          |
| `PI_KP`                 | 0.1      | % · s/deg| Proportional gain                           |
| `PI_KI`                 | 0.01     | %/deg    | Integral gain                               |
| `PI_INTEGRAL_SAT`       | 50       | %        | Integral term clamp                         |
| `ANGLE_LP_CUTOFF`       | 30       | Hz       | Encoder angle low-pass filter               |
| `SPEED_LP_CUTOFF`       | 20       | Hz       | Speed estimate low-pass filter              |
| `AS5600_SAMPLE_PERIOD_S`| 0.001    | s        | Encoder update period                       |
| `DIVIDER_RATIO`         | 5.55     | —        | IHM07 BEMF voltage divider (12.2 / 2.2)    |
| `VBUS`                  | 12       | V        | Motor supply voltage                        |
| `VREF`                  | 3.3      | V        | ADC reference voltage                       |
| Speed setpoint          | 1000     | deg/s    | Target angular speed (encoder mode)         |
| Ramp rate               | 100      | deg/s²   | Speed setpoint acceleration limit           |

---

## Reference Materials

All register settings, peripheral configurations, and GPIO modes are implemented based on:

- **Reference Manual:** RM0390 Rev 7 (STM32F446xx)
- **Datasheet:** DS10693 Rev 10 (STM32F446RE)
- **User Manual:** UM1724 Rev 17 (NUCLEO-F446RE)
- **Cortex-M4 Generic User Guide:** DUI0553A

**Peripheral / Sensor Documentation:**
- AS5600 Datasheet — ams AG
- IHM07M1 UM1943 Rev 4
- A2208/14T Motor Datasheet
