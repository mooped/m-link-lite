#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"

#include "ppm.h"

static const char *TAG = "m-link-ppm";

#define PPM_IO_NUM    14

#define TIMER_MAX_DELAY   0x199999

static ppm_callback_t ppm_cb = NULL;

xQueueHandle ppm_evt_queue = NULL;

typedef enum
{
  PPM_GAP,
  PPM_PULSE,
  PPM_WAITING,
} ppm_state_t;

static int32_t ppm_last_transition = 0;
static ppm_state_t ppm_state = PPM_WAITING;
static uint32_t ppm_channel = 0;
static uint16_t ppm_gap[PPM_NUM_CHANNELS] = { 0 };
static uint16_t ppm_data[PPM_NUM_CHANNELS] = { 0 };

static volatile uint32_t ppm_rollover = 0;
static volatile uint32_t ppm_last_rollover = 0;

static void ppm_isr_handler(void* arg)
{
  // Capture the current time and calculate with width of the last pulse
  volatile uint32_t now = (TIMER_MAX_DELAY - hw_timer_get_count_data()) + ppm_rollover * TIMER_MAX_DELAY;
  const uint32_t pulsewidth = now - ppm_last_transition;
  ppm_last_transition = now;

  // If the counter rolled over, just throw away the frame and wait for a new sync pulse
  if (ppm_rollover != ppm_last_rollover)
  {
    ppm_state = PPM_WAITING;
    ppm_channel = 0;
  }
  // If we get a long pulse it is a sync pulse, so reset and throw away the last frame
  else if (pulsewidth > 20000)
  {
    ppm_state = PPM_GAP;
    ppm_channel = 0;
  }
  else
  {
    switch (ppm_state)
    {
      // Waiting for the next pulse
      case PPM_GAP:
      {
        ppm_gap[ppm_channel] = pulsewidth;
        ppm_state = PPM_PULSE;

        // If we got a really long or short gap, throw away the frame
        if (ppm_gap[ppm_channel] <= 1980 || ppm_gap[ppm_channel] > 2020)
        {
          ppm_state = PPM_WAITING;
          ppm_channel = 0;
        }
      } break;
      // Timing a pulse
      case PPM_PULSE:
      {
        ppm_data[ppm_channel] = ppm_gap[ppm_channel] + pulsewidth;
        ++ppm_channel;
        if (ppm_channel < PPM_NUM_CHANNELS)
        {
          ppm_state = PPM_GAP;
        }
        else
        {
          ppm_state = PPM_WAITING;
          ppm_channel = 0;
          xQueueOverwriteFromISR(ppm_evt_queue, ppm_data, NULL);
        }
      } break;
      // Waiting for sync pulse
      case PPM_WAITING: break;
    }
  }
  ppm_last_rollover = ppm_rollover;
}

static void ppm_timer_isr(void* arg)
{
  // Count when the hardware timer resets
  ++ppm_rollover;
}

static void ppm_task(void* args)
{
  uint16_t frame_data[PPM_NUM_CHANNELS];

  for (;;)
  {
    // Wait for events from the PPM interrupt
    xQueueReceive(ppm_evt_queue, &frame_data, portMAX_DELAY);

    // Send the PPM callback
    (*ppm_cb)(frame_data);

    /*
    // Print gaps
    ESP_LOGI(TAG, "Gaps %d %d %d %d %d %d %d %d %d",
      ppm_gap[0],
      ppm_gap[1],
      ppm_gap[2],
      ppm_gap[3],
      ppm_gap[4],
      ppm_gap[5],
      ppm_gap[6],
      ppm_gap[7],
      ppm_gap[8]
    );
    ESP_LOGI(TAG, "Channels %d %d %d %d %d %d %d %d %d",
      frame_data[0], 
      frame_data[1], 
      frame_data[2], 
      frame_data[3], 
      frame_data[4], 
      frame_data[5], 
      frame_data[6], 
      frame_data[7], 
      frame_data[8]
    );
    */
  }
}

esp_err_t ppm_init(ppm_callback_t in_ppm_cb)
{
  // Assign callback
  if (!in_ppm_cb)
  {
    return ESP_FAIL;
  }
  ppm_cb = in_ppm_cb;

  // Create queue and task to receive messages from the PPM interrupt
  ppm_evt_queue = xQueueCreate(1, sizeof(uint32_t) * PPM_NUM_CHANNELS);
  xTaskCreate(ppm_task, "ppm-task", 2048, NULL, 10, NULL);

  // Start hardware timer to count time between callbacks
  ESP_ERROR_CHECK( hw_timer_init(ppm_timer_isr, NULL) );
  ESP_ERROR_CHECK( hw_timer_alarm_us(0x199999, pdTRUE) );

  // Setup pin change interrupt to detect PPM signal edges
  gpio_config_t config;
  config.pin_bit_mask = (1ull<<PPM_IO_NUM);
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_ANYEDGE;
  ESP_ERROR_CHECK( gpio_install_isr_service(0) );
  ESP_ERROR_CHECK( gpio_isr_handler_add(PPM_IO_NUM, ppm_isr_handler, (void*)PPM_IO_NUM) );
  ESP_ERROR_CHECK( gpio_config(&config) );

  // Log that PPM has been initialised
  ESP_LOGI(TAG, "Initialised PPM on pin %d.\n", PPM_IO_NUM);

  return ESP_OK;
}
