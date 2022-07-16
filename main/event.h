#pragma once

// Update the desired pulsewidth for a servo
void process_servo_event(int channel, int pulsewidth_ms);

// Update the desired failsafe pulsewidth for a servo
void process_failsafe_event(int channel, int pulsewidth_ms);

// Query battery voltage
int query_battery_voltage(void);
