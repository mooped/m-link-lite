#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/adc.h"

#include "battery.h"

static const char* TAG = "m-link-battery";

static volatile uint16_t battery_level = -1;

static void battery_task(void* pvParam)
{
  ESP_LOGI(TAG, "Started battery task.");

  // Update battery reading once per second
  const TickType_t interval = pdMS_TO_TICKS(1000);
  TickType_t previous_wake_time = xTaskGetTickCount();

  uint16_t data;
  for (;;)
  {
    // Read battery level from the ADC
    esp_err_t ret;
    ret = adc_read(&data);
    if (ret == ESP_OK)
    {
      // Convert into a voltage (using 10/1 potential divider resulting in divide by 11)
      data = ((uint32_t)data * 11 * 1023) / 1000;
 
      // Write battery level
      battery_level = data;

      ESP_LOGI(TAG, "Read battery level: %d", battery_level);
    }
    else
    {
      ESP_ERROR_CHECK(ret);
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

esp_err_t battery_init(void)
{
  esp_err_t ret;
  adc_config_t config;
  config.mode = ADC_READ_TOUT_MODE;
  config.clk_div = 8;
  ret = adc_init(&config);

  if (ret == ESP_OK)
  {
    xTaskCreate(battery_task, "battery-task", 1024, NULL, 5, NULL);
  }

  return ret;
}

uint16_t battery_get_level(void)
{
  return battery_level;
}

