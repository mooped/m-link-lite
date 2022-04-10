#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"

#include "tx-ppm.h"

static const char *TAG = "m-link-tx-ppm";

#define PPM_IO_NUM    14

static tx_ppm_callback_t tx_ppm_cb = NULL;

xQueueHandle tx_ppm_evt_queue = NULL;

typedef enum
{
  PPM_GAP,
  PPM_PULSE,
  PPM_WAITING,
} ppm_state_t;

static int32_t ppm_last_transition = 0;
static ppm_state_t ppm_state = PPM_WAITING;
static uint32_t ppm_channel = 0;
static uint32_t ppm_data[PPM_NUM_CHANNELS] = { 0 };

static volatile uint32_t ppm_rollover = 0;

static void tx_ppm_isr_handler(void* arg)
{
  // Capture the current time and calculate with width of the last pulse
  volatile uint32_t now = (0x199999 - hw_timer_get_count_data()) + (ppm_rollover << 23);
  const uint32_t pulsewidth = now - ppm_last_transition;
  ppm_last_transition = now;

  // If the counter rolled over, just throw away the frame and wait for a new sync pulse
  if (now < ppm_last_transition)
  {
    ppm_state = PPM_WAITING;
    ppm_channel = 0;
  }
  // If we get a long pulse it is a sync pulse, so reset and throw away the last frame
  if (pulsewidth > 20000)
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
        ppm_state = PPM_PULSE;
      } break;
      // Timing a pulse
      case PPM_PULSE:
      {
        ppm_data[ppm_channel++] = pulsewidth;
        if (ppm_channel < PPM_NUM_CHANNELS)
        {
          ppm_state = PPM_GAP;
        }
        else
        {
          ppm_state = PPM_WAITING;
          ppm_channel = 0;
          xQueueOverwriteFromISR(tx_ppm_evt_queue, ppm_data, NULL);
        }
      } break;
      // Waiting for sync pulse
      case PPM_WAITING: break;
    }
  }
}

static void tx_ppm_timer_isr(void* arg)
{
  // Count when the hardware timer resets
  ++ppm_rollover;
}

static void tx_ppm_task(void* args)
{
  static uint32_t frame_data[PPM_NUM_CHANNELS];

  for (;;)
  {
    // Wait for events from the PPM callback and hand off to the user callback
    if (xQueueReceive(tx_ppm_evt_queue, &frame_data, portMAX_DELAY))
    {
      (*tx_ppm_cb)(frame_data);
    }
  }
}

esp_err_t tx_ppm_init(tx_ppm_callback_t ppm_cb)
{
  // Assign callback
  if (!ppm_cb)
  {
    return ESP_FAIL;
  }
  tx_ppm_cb = ppm_cb;

  // Create queue and task to receive messages from the PPM interrupt
  tx_ppm_evt_queue = xQueueCreate(1, sizeof(uint32_t) * PPM_NUM_CHANNELS);
  xTaskCreate(tx_ppm_task, "tx-ppm-task", 2048, NULL, 10, NULL);

  // Start hardware timer to count time between callbacks
  ESP_ERROR_CHECK( hw_timer_init(tx_ppm_timer_isr, NULL) );
  ESP_ERROR_CHECK( hw_timer_alarm_us(0x199999, pdTRUE) );

  // Setup pin change interrupt to detect PPM signal edges
  gpio_config_t config;
  config.pin_bit_mask = (1ull<<PPM_IO_NUM);
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_ANYEDGE;
  ESP_ERROR_CHECK( gpio_install_isr_service(0) );
  ESP_ERROR_CHECK( gpio_isr_handler_add(PPM_IO_NUM, tx_ppm_isr_handler, (void*)PPM_IO_NUM) );
  ESP_ERROR_CHECK( gpio_config(&config) );

  // Log that PPM has been initialised
  ESP_LOGI(TAG, "Initialised PPM on pin %d.\n", PPM_IO_NUM);

  return ESP_OK;
}