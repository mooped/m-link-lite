#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/gpio.h"
#include "driver/pwm.h"

#include "pca9685.h"

#include "servo.h"

#if defined(CONFIG_HEXAPOD)

static const char *TAG = "pca9685-servo";

# define ENABLE_IO_NUM            12

// Local - channels 16 - 31
#define PCA9685_LOCAL_ADDR        (0x40<<1)
#define PCA9685_LOCAL_NUM         3
#define PCA9685_LOCAL_START       16
#define PCA9685_LOCAL_OFFSET      3

// Remote - channels 0 - 16
#define PCA9685_REMOTE_ADDR       (0x41<<1)
#define PCA9685_REMOTE_NUM        16
#define PCA9685_REMOTE_START      0
#define PCA9685_REMOTE_OFFSET     0

# define SERVO_NUM_CHANNELS       19

static int pulsewidths[SERVO_NUM_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int last_pulsewidths[SERVO_NUM_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int servo_enabled = 0;

static SemaphoreHandle_t xServoSemaphore = NULL;

void servo_enable(void)
{
  servo_enabled = 1;
  for (int i = 0; i < SERVO_NUM_CHANNELS; ++i)
  {
    last_pulsewidths[i] = -1;
  }
}

void servo_disable(void)
{
  servo_enabled = 0;
  for (int i = 0; i < SERVO_NUM_CHANNELS; ++i)
  {
    last_pulsewidths[i] = -1;
  }
}

void servo_set(int channel, int pulsewidth_ms)
{
  if (channel >= 0 && channel < SERVO_NUM_CHANNELS)
  {
    // Store pulsewidth for the channel
    if (pulsewidth_ms == 0) // No output
    {
      pulsewidths[channel] = 0;
    }
    else if (pulsewidth_ms <= 1000)
    {
      pulsewidths[channel] = 1000 / 5;
    }
    else if (pulsewidth_ms >= 2000)
    {
      pulsewidths[channel] = 2000 / 5;
    }
    else
    {
      pulsewidths[channel] = pulsewidth_ms / 5;
    }
  }
}

void servo_task(void* args)
{
  ESP_LOGI(TAG, "Started hexapod servo task");

  // Update servos every 20 ms
  const TickType_t interval = pdMS_TO_TICKS(20);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    int updates = 0;

    // Local
    pca9685_set_addr(PCA9685_LOCAL_ADDR);
    for (int i = 0; i < PCA9685_LOCAL_NUM; ++i)
    {
      const int index = i + PCA9685_LOCAL_START;
      if (pulsewidths[index] != last_pulsewidths[index])
      {
        pca9685_set_microseconds(i + PCA9685_LOCAL_OFFSET, servo_enabled ? pulsewidths[index] : 0);
        last_pulsewidths[index] = pulsewidths[index];
        ++updates;
      }
    }
    
    // Remote
    pca9685_set_addr(PCA9685_REMOTE_ADDR);
    for (int i = 0; i < PCA9685_REMOTE_NUM; ++i)
    {
      const int index = i + PCA9685_REMOTE_START;
      if (pulsewidths[index] != last_pulsewidths[index])
      {
        pca9685_set_microseconds(i + PCA9685_REMOTE_OFFSET, servo_enabled ? pulsewidths[index] : 0);
        last_pulsewidths[index] = pulsewidths[index];
        ++updates;
      }
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

void servo_init(void)
{
  // Create the semaphore
  xServoSemaphore = xSemaphoreCreateBinary();

  // Configure the enable pin and pull low to enable all outputs
  gpio_config_t config;
  config.pin_bit_mask = (1ull<<(ENABLE_IO_NUM));
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK( gpio_config(&config) );
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 0) );

  // Initialize the PCA 9685s - 100hz frequency for simple servos
  pca9685_reset_all();
  pca9685_set_addr(PCA9685_LOCAL_ADDR);
  pca9685_initialize(60);
  pca9685_set_addr(PCA9685_REMOTE_ADDR);
  pca9685_initialize(60);

  // Turn off all outputs
  pca9685_set_addr(PCA9685_LOCAL_ADDR);
  pca9685_set_all_off();
  pca9685_set_addr(PCA9685_REMOTE_ADDR);
  pca9685_set_all_off();

  ESP_LOGI(TAG, "PCA9685 Initialised");
  xTaskCreate(servo_task, "servo-task", 2048, NULL, 11, NULL);
}

#endif

