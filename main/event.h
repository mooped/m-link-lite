#pragma once

// Query the number of supported channels
int query_supported_channels(void);

// Update the desired pulsewidth for a servo
void process_servo_event(int channel, int pulsewidth_ms);

// Update the desired failsafe pulsewidth for a servo
void process_failsafe_event(int channel, int pulsewidth_ms);

// Query the desired failsafe
int query_failsafe(int channel);

// Query if failsafe is active
bool query_failsafe_engaged();

// Query battery voltage
int query_battery_voltage(void);

