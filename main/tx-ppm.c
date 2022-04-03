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

// See https://sub.nanona.fi/esp8266/timing-and-ticks.html
static uint32_t get_ccount(void)
{
  uint32_t r;
  asm volatile ("rsr %0, ccount" : "=r"(r));
  return r;
}

typedef enum
{
  PPM_GAP,
  PPM_PULSE,
  PPM_WAITING,
} ppm_state_t;

static int32_t ppm_last_transition = 0;
static ppm_state_t ppm_state = PPM_GAP;
static uint32_t ppm_channel = 0;
static uint32_t ppm_data[PPM_NUM_CHANNELS] = { 0 };

static volatile uint32_t micros = 0;

static void tx_ppm_isr_handler(void* arg)
{
  // Capture the current time and calculate with width of the last pulse
  volatile uint32_t now = micros;//get_ccount();
  const uint32_t pulsewidth = (now >= ppm_last_transition) ? (now - ppm_last_transition) : (now + (0xffffffff - ppm_last_transition));
  ppm_last_transition = now;

  // If we get a pulse > 5ms then reset, throw away the last frame
  if (pulsewidth > 80)
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
          xQueueSendFromISR(tx_ppm_evt_queue, ppm_data, NULL);
        }
      } break;
      case PPM_WAITING:
      {
        // Waiting for sync pulse
      } break;
    }
  }
}

static void tx_ppm_timer_isr(void* arg)
{
  ++micros;
}

static void tx_ppm_task(void* args)
{
  static uint32_t frame_data[PPM_NUM_CHANNELS];
  static uint32_t count = 0;

  for (;;)
  {
    if (xQueueReceive(tx_ppm_evt_queue, &frame_data, portMAX_DELAY))
    {
      const uint32_t waiting = uxQueueMessagesWaiting(tx_ppm_evt_queue);
      if ((count++ % 10) == 0) {
      ESP_LOGI(TAG, "Received Frame %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d Waiting: %d\n",
        frame_data[0], 
        frame_data[1], 
        frame_data[2], 
        frame_data[3], 
        frame_data[4], 
        frame_data[5], 
        frame_data[6], 
        frame_data[7], 
        frame_data[8],
        frame_data[9], 
        frame_data[10], 
        frame_data[11], 
        frame_data[12], 
        frame_data[13], 
        frame_data[14], 
        frame_data[15], 
        frame_data[16], 
        frame_data[17],
        frame_data[18],
        frame_data[19],
        waiting
      );
      }
    }
  }
}

void tx_ppm_init(tx_ppm_callback_t ppm_cb)
{
  tx_ppm_cb = ppm_cb;
  tx_ppm_evt_queue = xQueueCreate(10, sizeof(uint32_t) * PPM_NUM_CHANNELS);

  xTaskCreate(tx_ppm_task, "tx-ppm-task", 2048, NULL, 10, NULL);

  ESP_ERROR_CHECK( hw_timer_init(tx_ppm_timer_isr, NULL) );
  ESP_ERROR_CHECK( hw_timer_set_clkdiv(TIMER_CLKDIV_1) );
  ESP_ERROR_CHECK( hw_timer_alarm_us(51, pdTRUE) );

  ESP_LOGI(TAG, "Initialising PPM on pin %d.\n", PPM_IO_NUM);
  gpio_config_t config;
  config.pin_bit_mask = (1ull<<PPM_IO_NUM);
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_ANYEDGE;
  ESP_ERROR_CHECK( gpio_install_isr_service(0) );
  ESP_ERROR_CHECK( gpio_isr_handler_add(PPM_IO_NUM, tx_ppm_isr_handler, (void*)PPM_IO_NUM) );
  ESP_ERROR_CHECK( gpio_config(&config) );
}
