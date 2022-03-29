/* pwm example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "rx-pwm.h"
#include "event.h"

static const char *TAG = "m-link-rx-pwm";

void app_main()
{
  // Initialize NVS
  ESP_ERROR_CHECK( nvs_flash_init() );

  ESP_ERROR_CHECK( transport_init() );

  rx_pwm_init();

  int16_t count = 0;

  while (1)
  {
      if (count >= 512)
      {
        count = 0;
      }

      ESP_LOGI(TAG, "Count %d\n", count);
      rx_pwm_set_led(count % 2);
      rx_pwm_set_motors(count, -count, count);
      rx_pwm_update();

      count++;
      vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

