#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/gpio.h"

static const char* TAG = "m-link-rx-led";

#define LED_NUM   1

typedef struct
{
  xTimerHandle timer;
  gpio_num_t gpio_num;
  int period;
  int duty;
  int state;
}
timer_info_t;

static timer_info_t timers[LED_NUM] = {
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_2,
    .period = pdMS_TO_TICKS(2000),
    .duty = pdMS_TO_TICKS(1000),
    .state = 0,
    
  },
};

void rx_led_timer_callback(xTimerHandle xTimer)
{
  timer_info_t* info = (timer_info_t*)pvTimerGetTimerID(xTimer);

  if (info)
  {
    // Currently on
    if (info->state)
    {
      xTimerChangePeriod(info->timer, info->duty, 0);
      info->state = 0;
    }
    // Currently off
    else
    {
      xTimerChangePeriod(info->timer, info->period - info->duty, 0);
      info->state = 1;
    }

    // Update LED
    ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, info->state) );
  }
}

esp_err_t rx_led_init(void)
{
  for (int led_idx = 0; led_idx < LED_NUM; ++led_idx)
  {
    timer_info_t* info = &timers[led_idx];

    gpio_config_t config;
    config.pin_bit_mask = (1ull<<(info->gpio_num));
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config(&config) );
    ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, info->state) );

    info->timer = xTimerCreate("rx-led-timer", info->duty, pdTRUE, (void*)info, &rx_led_timer_callback);
    if (!info->timer)
    {
      ESP_LOGW(TAG, "Failed to create LED timer.");
      return ESP_FAIL;
    }
    if (xTimerStart(info->timer, 0) != pdPASS)
    {
      ESP_LOGW(TAG, "Failed to start LED timer.");
      return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initialised LED on pin %d.", info->gpio_num);
  }

  return ESP_OK;
}

void rx_led_set(int index, int duty, int period)
{
  if (index < LED_NUM && timers[index].timer)
  {
    timers[index].duty = pdMS_TO_TICKS(duty);
    timers[index].period = pdMS_TO_TICKS(period);
    timers[index].state = 0;
    xTimerChangePeriod(timers[index].timer, timers[index].duty, 0);
    xTimerReset(timers[index].timer, 0);
  }
}

