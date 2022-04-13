#pragma once

#define MOTOR_MAX     500
#define MOTOR_COAST   0
#define MOTOR_BRAKE   501

/// Initialise the PWM/motor control module
void motor_init(void);

/// Set desired motor states -511 - 511
/// 0 - coast
/// 512 - brake
void motor_set_all(int m1, int m2, int m3);

