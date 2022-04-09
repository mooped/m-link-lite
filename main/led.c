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

typedef struct
{
  xTimerHandle timer;
  gpio_num_t gpio_num;
  int period;
  int duty;
  int state;
}
led_config_t;

static led_config_t* config = NULL;
static size_t led_num = 0;

void led_timer_callback(xTimerHandle xTimer)
{
  led_config_t* info = (led_config_t*)pvTimerGetTimerID(xTimer);

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

esp_err_t led_init(led_config_t* in_config, size_t in_led_num)
{
  if (!in_config)
  {
    ESP_LOGW(TAG, "led_init received null config!");
    return ESP_FAIL;
  }

  config = in_config;
  led_num = in_led_num;

  for (int led_idx = 0; led_idx < led_num; ++led_idx)
  {
    led_config_t* info = &config[led_idx];

    gpio_config_t config;
    config.pin_bit_mask = (1ull<<(info->gpio_num));
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config(&config) );
    ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, info->state) );

    info->timer = xTimerCreate("rx-led-timer", info->duty, pdTRUE, (void*)info, &led_timer_callback);
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

void led_set(int index, int state, int duty, int period)
{
  if (index < led_num)
  {
    led_config_t* info = &config[index];
    if (info->timer)
    {
      info->duty = pdMS_TO_TICKS(duty);
      info->period = pdMS_TO_TICKS(period);
      info->state = state;
      // Always off - avoid using a timer
      if (duty <= 0)
      {
        if (xTimerStop(info->timer, 0) != pdPASS)
        {
          ESP_LOGW(TAG, "Timer stop failed.");
        }
        ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, 0) );
      }
      // Always on - avoid using a timer
      else if (duty >= period)
      {
        if (xTimerStop(info->timer, 0) != pdPASS)
        {
          ESP_LOGW(TAG, "Timer stop failed.");
        }
        ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, 1) );
      }
      // Blinking - using a timer
      else
      {
        if (info->state)
        {
          if (xTimerChangePeriod(info->timer, info->duty, 0) != pdPASS)
          {
            ESP_LOGW(TAG, "Timer change period failed.");
          }
        }
        else
        {
          if (xTimerChangePeriod(info->timer, info->period - info->duty, 0) != pdPASS)
          {
            ESP_LOGW(TAG, "Timer change period failed.");
          }
        }
        if (xTimerReset(info->timer, 0) != pdPASS)
        {
          ESP_LOGW(TAG, "Timer restart failed.");
        }
        ESP_ERROR_CHECK( gpio_set_level(info->gpio_num, info->state) );
      }
    }
  }
}

