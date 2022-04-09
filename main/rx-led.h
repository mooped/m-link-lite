#pragma once

// Initialise LED driver
esp_err_t rx_led_init(void);

// Set LED duty and period
void rx_led_set(int index, int duty, int period);

