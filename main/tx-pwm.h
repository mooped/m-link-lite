#pragma once

/// Initialise the PWM/LED control module
void tx_pwm_init(void);

/// Set desired LED state 0 - off, 1024 - on
void tx_pwm_set_leds(unsigned int status1, unsigned int status2, unsigned int module);

/// Apply desired states
void tx_pwm_update(void);

