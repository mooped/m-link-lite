#pragma once

#include "driver/gpio.h"

typedef struct
{
  xTimerHandle timer;
  gpio_num_t gpio_num;
  int period;
  int duty;
  int state;
}
led_config_t;

// Initialise LED driver
esp_err_t led_init(led_config_t* config, size_t led_num);

// Set LED duty and period
void led_set(int index, int duty, int period);

