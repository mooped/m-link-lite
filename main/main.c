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

#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>

#include "captDns.h"

#include "battery.h"
#include "dns.h"
#include "event.h"
#include "led.h"
#include "server.h"
#include "servo.h"
#include "settings.h"
#include "wifi.h"

static const char *TAG = "m-link-lite-main";

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
      case RX_LED_ACTIVE:
      {
        led_set(0, 1, 1000, 1000);
      } break;
      case RX_LED_FAILSAFE:
      {
        led_set(0, 1, 100, 500);
      } break;
      default:
      {
        ESP_LOGW(TAG, "Invalid LED state requested.");
      }
    }
    rx_led_state = new_state;
  }
}

static int battery_level = 0;

int query_battery_voltage(void)
{
  return battery_level;
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
    battery_level = battery_get_level();
    //ESP_LOGI(TAG, "Battery Level: %d", battery_level);

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

xTimerHandle rx_failsafe_timer = NULL;
bool failsafe_elapsed = false;

#define SERVO_NUM   6
static int servos[SERVO_NUM] = { 1500, 1500, 1500, 1500, 1500, 1500 };
static int failsafes[SERVO_NUM] = { 1500, 1500, 1500, 1500, 1500, 1500 };

void process_servo_event(int channel, int pulsewidth_ms)
{
  if (channel >= 0 && channel < SERVO_NUM)
  {
    // Update the channel
    servos[channel] = pulsewidth_ms;
    
    // Reset failsafe
    xTimerReset(rx_failsafe_timer, 0);
    if (failsafe_elapsed)
    {
      // Log the first time we reset
      ESP_LOGW(TAG, "Failsafe disengaged.");

      // Set LED state to active
      rx_led_set_state(RX_LED_ACTIVE);
    }
    failsafe_elapsed = false;
  }
  else
  {
    ESP_LOGW(TAG, "Ignoring request to set out of range servo %d to %d ms.", channel, pulsewidth_ms);
  }
}

void process_failsafe_event(int channel, int pulsewidth_ms)
{
  if (channel >= 0 && channel < SERVO_NUM)
  {
    // Update the failsafe
    failsafes[channel] = pulsewidth_ms;
  }
  else
  {
    ESP_LOGW(TAG, "Ignoring request to set out of range failsafe value %d to %d ms.", channel, pulsewidth_ms);
  }
}

void rx_failsafe_callback(xTimerHandle xTimer)
{
  // Print a message when failsafe elapses
  if (!failsafe_elapsed)
  {
    ESP_LOGW(TAG, "Failsafe engaged - send an update to disengage!");

    // Set LED to indicate failsafe
    rx_led_set_state(RX_LED_FAILSAFE);
  }

  // Stop servo updates from rx_task
  failsafe_elapsed = true;

  // Set failsafe values to the servos
  for (int channel = 0; channel < SERVO_NUM; ++channel)
  {
    // Negative failsafe values indicate that the channel should be held
    // Positive values are the pulsewidth to set
    if (failsafes[channel] >= 0)
    {
      servo_set(channel, failsafes[channel]);
    }
  }
  servo_refresh();
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
    if (!failsafe_elapsed)
    {
      // Update the servo driver if not in failsafe mode
      for (int channel = 0; channel < SERVO_NUM; ++channel)
      {
        servo_set(channel, servos[channel]);
      }
      servo_refresh();
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

void app_main()
{
  // Initialise and immediately disable servo module as soon as possible to avoid glitches
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

  // Initialise settings
  settings_init();

  // Set LED to boot state (2 second flash)
  ESP_ERROR_CHECK( led_init(rx_led_config, RX_LED_NUM) );
  rx_led_set_state(RX_LED_WAITING);

  // Create event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialise WiFi
  wifi_init_apsta();

  // Start the webserver
  server_init();

  // Initialise RX task
  xTaskCreate(rx_task, "rx-task", 2048, NULL, 10, NULL);

  // Initialise RX telemetry task
  xTaskCreate(rx_telemetry_task, "rx-telemetry-task", 2048, NULL, 7, NULL);

  // Initialise mDNS
  mlink_dns_init();

  // Captive DNS
  captdnsInit();

  // Start failsafe timer
  rx_failsafe_timer = xTimerCreate("rx-failsafe-timer", pdMS_TO_TICKS(500), pdTRUE, NULL, rx_failsafe_callback);
  xTimerStart(rx_failsafe_timer, 0);
}
