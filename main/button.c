#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/gpio.h"

#include "settings.h"

#include "button.h"

#define BUTTON_IO_COUNT   1

#define BUTTON_0_NUM      GPIO_NUM_0  // DTR

#define BUTTON_INTERVAL   100         // 100ms update rate

static const char *TAG = "m-link-button";

typedef struct
{
  uint32_t gpio_num;
  uint32_t trigger_state;
  uint32_t trigger_time;
  uint32_t current_time;
  void (*handler_fn)(int);
}
button_config_t;

static void button_0_handler(int button_state)
{
  ESP_LOGI(TAG, "Restoring default settings");
  settings_reset_defaults();
  ESP_LOGI(TAG, "Rebooting...");
  esp_restart();
}

button_config_t button_configs[BUTTON_IO_COUNT] = {
  {
    .gpio_num = BUTTON_0_NUM,
    .trigger_state = 0,
    .trigger_time = 10000,
    .current_time = 0,
    .handler_fn = &button_0_handler,
  },
};

static void button_process(button_config_t* button_config)
{
  // Poll the button
  const int state = gpio_get_level(button_config->gpio_num);

  // Is the button in the desired state?
  if (state == button_config->trigger_state)
  {
    // Increment timer
    button_config->current_time += BUTTON_INTERVAL;

    // Have we been pressed for the required time?
    if (button_config->current_time > button_config->trigger_time)
    {
      // Do the relevant action
      button_config->handler_fn(state);

      // Reset counter
      button_config->current_time = 0;
    }
  }
  else
  {
    // Reset counter
    button_config->current_time = 0;
  }
}

static void button_task(void* pvParam)
{
  ESP_LOGI(TAG, "Started button task.");

  // Update buttons every 100 ms
  const TickType_t interval = pdMS_TO_TICKS(BUTTON_INTERVAL);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    // Process each button
    for (int button_idx = 0; button_idx < BUTTON_IO_COUNT; ++button_idx)
    {
      button_config_t* button_config = &button_configs[button_idx];
      button_process(button_config);
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

void button_init(void)
{
  // Initialise the GPIOs for the defined buttons
  for (int button_idx = 0; button_idx < BUTTON_IO_COUNT; ++button_idx)
  {
    button_config_t* button_config = &button_configs[button_idx];
    gpio_config_t config;
    config.pin_bit_mask = (1ull<<(button_config->gpio_num));
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config(&config) );
  }

  // Start the button handler task
  xTaskCreate(button_task, "button-task", 1024, NULL, 6, NULL);
}
