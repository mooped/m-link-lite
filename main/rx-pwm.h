#pragma once

/// Initialise the PWM/motor control module
void rx_pwm_init(void);

/// Set desired motor states -511 - 511
/// 0 - coast
/// 512 - brake
void rx_pwm_set_motors(int m1, int m2, int m3);

/// Apply desired states
void rx_pwm_update(void);

