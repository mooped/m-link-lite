#pragma once

/// Initialise the PWM/servo control module
void servo_init(void);

/// Enable/disable all servos
void servo_enable(void);
void servo_disable(void);

/// Set desired servo pulse width in ms
void servo_set_all(int s1, int s2, int s3, int s4, int s5, int s6);

