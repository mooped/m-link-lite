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

#if defined(CONFIG_PLUS)

#define ENABLE_IO_NUM             12
#define FEEDBACK_IO_NUM           13

#define PCA9685_CHANNEL_SLEEP     0
#define PCA9685_CHANNEL_FEEDBACK  2
#define PCA9685_CHANNEL_M1A       11
#define PCA9685_CHANNEL_M1B       10
#define PCA9685_CHANNEL_M2A       9
#define PCA9685_CHANNEL_M2B       8
#define PCA9685_CHANNEL_M3A       7
#define PCA9685_CHANNEL_M3B       6
#define PCA9685_CHANNEL_CH4       5
#define PCA9685_CHANNEL_CH5       4
#define PCA9685_CHANNEL_CH6       3

#define SERVO_NUM_CHANNELS        9

static const char *TAG = "pca9685-servo";

static uint32_t pulsewidths[SERVO_NUM_CHANNELS] = {
  1500, 1500, 1500, 1500, 1500, 1500,
};

static SemaphoreHandle_t xServoSemaphore = NULL;

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
    if (pulsewidth_ms == 0) // No output
    {
      pulsewidths[channel] = 0;
    }
    else if (pulsewidth_ms <= 1000)
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

static void motor_set(int channel_a, int channel_b, int pulsewidth)
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
    pca9685_set_raw(channel_b, ((pulsewidth - 1500) * 4095) / 500); // Scale from 0 - 500 to 0 - 4095
  }
  else if (pulsewidth < 1500) // Reverse
  {
    pca9685_set_raw(channel_a, ((1500 - pulsewidth) * 4095) / 500); // Scale from 0 - 500 to 0 - 4095
    pca9685_set_off(channel_b);
  }
}

void servo_set_all(int s1, int s2, int s3, int s4, int s5, int s6)
{
  servo_set(0, s1);
  servo_set(1, s2);
  servo_set(2, s3);
  servo_set(3, s4);
  servo_set(4, s5);
  servo_set(5, s6);
}

static void feedback_isr_handler(void* arg)
{
  // Wake up and yield to the servo task at the start of each frame
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(xServoSemaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }
}

void servo_task(void* args)
{
  ESP_LOGI(TAG, "Started servo task");

#if CONFIG_PCA9685_FEEDBACK
  // Enable feedback pulse
  pca9685_set_microseconds(PCA9685_CHANNEL_FEEDBACK, 1500);

  for (;;)
  {
    // Output pulse to first servo channel
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_microseconds(PCA9685_CHANNEL_CH4, pulsewidths[3]);
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_off(PCA9685_CHANNEL_CH4);

    // Output pulse to second servo channel
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_microseconds(PCA9685_CHANNEL_CH5, pulsewidths[4]);
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_off(PCA9685_CHANNEL_CH5);

    // Output pulse to third servo channel
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_microseconds(PCA9685_CHANNEL_CH6, pulsewidths[5]);
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    pca9685_set_off(PCA9685_CHANNEL_CH6);

    // Update motor channels on the next sync pulse
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    motor_set(PCA9685_CHANNEL_M1A, PCA9685_CHANNEL_M1B, pulsewidths[0]);
    motor_set(PCA9685_CHANNEL_M2A, PCA9685_CHANNEL_M2B, pulsewidths[1]);
    motor_set(PCA9685_CHANNEL_M3A, PCA9685_CHANNEL_M3B, pulsewidths[2]);

    // Three more sync pulses for a period of about 20ms
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
    xSemaphoreTake(xServoSemaphore, portMAX_DELAY);
  }
#else
  // Update motors every 20 ms
  const TickType_t interval = pdMS_TO_TICKS(18);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    // Update motor channels
    motor_set(PCA9685_CHANNEL_M1A, PCA9685_CHANNEL_M1B, pulsewidths[0]);
    motor_set(PCA9685_CHANNEL_M2A, PCA9685_CHANNEL_M2B, pulsewidths[1]);
    motor_set(PCA9685_CHANNEL_M3A, PCA9685_CHANNEL_M3B, pulsewidths[2]);

    // Update servo channels with pure PWM output
    pca9685_set_microseconds(PCA9685_CHANNEL_CH4, pulsewidths[3]);
    pca9685_set_microseconds(PCA9685_CHANNEL_CH5, pulsewidths[4]);
    pca9685_set_microseconds(PCA9685_CHANNEL_CH6, pulsewidths[5]);

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
#endif
}

void servo_init(void)
{
  // Create the semaphore
  xServoSemaphore = xSemaphoreCreateBinary();

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

  // Configure feedback pin and interrupt to detect PPM frame start
  config.pin_bit_mask = (1ull<<(FEEDBACK_IO_NUM));
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;

#if CONFIG_PCA9685_FEEDBACK
  config.intr_type = GPIO_INTR_POSEDGE;
  ESP_ERROR_CHECK( gpio_config(&config) );
  ESP_ERROR_CHECK( gpio_install_isr_service(0) );
  ESP_ERROR_CHECK( gpio_isr_handler_add(FEEDBACK_IO_NUM, feedback_isr_handler, (void*)FEEDBACK_IO_NUM) );
#else
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK( gpio_config(&config) );
#endif

  // Configure the sleep pin on the PCA9685 high to disable sleep when outputs are enabled
  pca9685_set_all_off();
  pca9685_set_on(PCA9685_CHANNEL_SLEEP);

  ESP_LOGI(TAG, "PCA9685 Initialised");
  xTaskCreate(servo_task, "servo-task", 2048, NULL, 11, NULL);
}

#endif

