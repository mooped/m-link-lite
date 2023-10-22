#pragma once

/// Enable/disable all servos
void servo_enable(void);
void servo_disable(void);

// Set desired servo pulse width in ms
void servo_set(int channel, int pulsewidth_ms);

/// Initialise the PWM/servo control module
void servo_init(void);

