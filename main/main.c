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
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include <esp_http_server.h>
#include <esp_vfs.h>
#include <esp_spiffs.h>

#include "servo.h"
#include "led.h"
#include "battery.h"
#include "wifi.h"
#include "server.h"
#include "dns.h"
#include "event.h"

static const char *TAG = "m-link-lite-main";

nvs_handle_t rx_nvs_handle;

#define RX_LED_NUM  1

static led_config_t rx_led_config[RX_LED_NUM] = {
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_16,
    .period = pdMS_TO_TICKS(2000),
    .duty = pdMS_TO_TICKS(1000),
    .state = 0,
  },
};

typedef enum
{
  RX_LED_WAITING,
  RX_LED_BIND,
  RX_LED_ACTIVE,
  RX_LED_FAILSAFE,
  RX_LED_INITIAL,
}
rx_led_state_t;

rx_led_state_t rx_led_state = RX_LED_INITIAL;

void rx_led_set_state(rx_led_state_t new_state)
{
  if (new_state != rx_led_state)
  {
    switch (new_state)
    {
      case RX_LED_WAITING:
      {
        led_set(0, 1, 1000, 2000);
      } break;
      case RX_LED_BIND:
      {
        led_set(0, 1, 180, 200);
      } break;
      case RX_LED_ACTIVE:
      {
        led_set(0, 1, 1000, 1000);
      } break;
      case RX_LED_FAILSAFE:
      {
        led_set(0, 1, 500, 1000);
      } break;
      default:
      {
        ESP_LOGW(TAG, "Invalid LED state requested.");
      }
    }
    rx_led_state = new_state;
  }
}

void rx_telemetry_task(void* args)
{
  ESP_LOGI(TAG, "Started telemetry task.");

  // Initialise battery voltage and RSSI measurement
  ESP_ERROR_CHECK( battery_init() );

  // Update telemetry once a second
  const TickType_t interval = pdMS_TO_TICKS(1000);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    // Print battery level
    ESP_LOGI(TAG, "Battery Level: %d", battery_get_level());

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

static int _s1 = 1500;
static int _s2 = 1500;
static int _s3 = 1500;
static int _s4 = 1500;
static int _s5 = 1500;
static int _s6 = 1500;

void process_incoming_event(int s1, int s2, int s3, int s4, int s5, int s6)
{
  _s1 = s1;
  _s2 = s2;
  _s3 = s3;
  _s4 = s4;
  _s5 = s5;
  _s6 = s6;
}

void rx_task(void* args)
{
  ESP_LOGI(TAG, "Started servo task");

  // Wait 2 seconds on startup before enabling the servos
  vTaskDelay(pdMS_TO_TICKS(2000));

  ESP_LOGI(TAG, "Enable servos");
  servo_enable();

  // Update servos every 20 ms
  const TickType_t interval = pdMS_TO_TICKS(20);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    servo_set_all(_s1, _s2, _s3, _s4, _s5, _s6);

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

void app_main()
{
  // Initialise and immediately disable servo module as soon as possible
  servo_init();
  servo_disable();

  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW(TAG, "Failed to initialise NVS, erasing to reinitialise.");
    // If we can't initialise NVS, erase and recreate the partition
    ESP_ERROR_CHECK( nvs_flash_erase() );
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Set LED to boot state (2 second flash)
  ESP_ERROR_CHECK( led_init(rx_led_config, RX_LED_NUM) );
  rx_led_set_state(RX_LED_WAITING);

  // Initialise WiFi
  wifi_init_sta();

  // Initialise RX task
  xTaskCreate(rx_task, "rx-task", 2048, NULL, 10, NULL);

  // Initialise RX telemetry task
  xTaskCreate(rx_telemetry_task, "rx-telemetry-task", 2048, NULL, 7, NULL);

  // Start the webserver
  server_init();

  // Initialise mDNS
  mlink_dns_init();
}
