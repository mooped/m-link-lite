#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/gpio.h"
#include "driver/pwm.h"

#include "pca9685.h"

#include "servo.h"

#if 1

#define ENABLE_IO_NUM          12

#define PCA9685_CHANNEL_SLEEP 0
#define PCA9685_CHANNEL_M1A   11
#define PCA9685_CHANNEL_M1B   10
#define PCA9685_CHANNEL_M2A   9
#define PCA9685_CHANNEL_M2B   8
#define PCA9685_CHANNEL_M3A   7
#define PCA9685_CHANNEL_M3B   6
#define PCA9685_CHANNEL_CH4   5
#define PCA9685_CHANNEL_CH5   4
#define PCA9685_CHANNEL_CH6   3

#define SERVO_NUM_CHANNELS 9

static const char *TAG = "pca9685-servo";

static uint32_t pulsewidths[SERVO_NUM_CHANNELS] = {
  1500, 1500, 1500, 1500, 1500, 1500,
};

void servo_init(void)
{
  // Initialize the PCA 9685
  pca9685_initialize();

  // Configure the enable pin and pull high to disable
  gpio_config_t config;
  config.pin_bit_mask = (1ull<<(ENABLE_IO_NUM));
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK( gpio_config(&config) );
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 1) );

  // Configure the sleep pin on the PCA9685 high to disable sleep when outputs are enabled
  pca9685_set_all_off();
  pca9685_set_on(PCA9685_CHANNEL_SLEEP);

  ESP_LOGI(TAG, "PCA9685 Initialised");
}

void servo_enable(void)
{
  // Set enable low
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 0) );
}

void servo_disable(void)
{
  // Set enable high
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 1) );
}

void servo_set(int channel, int pulsewidth_ms)
{
  if (channel >= 0 && channel < SERVO_NUM_CHANNELS)
  {
    // Store pulsewidth for the channel
    if (pulsewidth_ms <= 1000)
    {
      pulsewidths[channel] = 1000;
    }
    else if (pulsewidth_ms >= 2000)
    {
      pulsewidths[channel] = 2000;
    }
    else
    {
      pulsewidths[channel] = pulsewidth_ms;
    }
  }
}

static void motor_refresh(int channel_a, int channel_b, int pulsewidth)
{
  if (pulsewidth < 1000 || pulsewidth > 2000) // Coast
  {
    pca9685_set_off(channel_a);
    pca9685_set_off(channel_b);
  }
  else if (pulsewidth == 1500) // Brake
  {
    pca9685_set_on(channel_a);
    pca9685_set_on(channel_b);
  }
  else if (pulsewidth > 1500) // Forward
  {
    pca9685_set_off(channel_a);
    pca9685_set_microseconds(channel_b, ((pulsewidth - 1500) * 8192) / 250); // Scale from 0 - 500 to 0 - 16384
  }
  else if (pulsewidth < 1500) // Reverse
  {
    pca9685_set_microseconds(channel_a, ((1500 - pulsewidth) * 8192) / 250); // Scale from 0 - 500 to 0 - 16384
    pca9685_set_off(channel_b);
  }
}

void servo_refresh(void)
{
  // Motor channels
  motor_refresh(PCA9685_CHANNEL_M1A, PCA9685_CHANNEL_M1B, pulsewidths[0]);
  motor_refresh(PCA9685_CHANNEL_M2A, PCA9685_CHANNEL_M2B, pulsewidths[1]);
  motor_refresh(PCA9685_CHANNEL_M3A, PCA9685_CHANNEL_M3B, pulsewidths[2]);

  // Servo channels
  pca9685_set_microseconds(PCA9685_CHANNEL_CH4, pulsewidths[3]);
  pca9685_set_microseconds(PCA9685_CHANNEL_CH5, pulsewidths[4]);
  pca9685_set_microseconds(PCA9685_CHANNEL_CH6, pulsewidths[5]);
}

void servo_set_all(int s1, int s2, int s3, int s4, int s5, int s6)
{
  servo_set(0, s1);
  servo_set(1, s2);
  servo_set(2, s3);
  servo_set(3, s4);
  servo_set(4, s5);
  servo_set(5, s6);

  servo_refresh();
}

#endif

