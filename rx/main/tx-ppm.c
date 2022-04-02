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
static inline int32_t asm_ccount(void)
{
  int32_t r;
  asm volatile ("rsr %0, ccount" : "=r"(r));
  return r;
}

static void tx_ppm_isr_handler(void* arg)
{
  static int32_t last = 0;
  uint32_t ccount = hw_timer_get_count_data();//asm_ccount();
  xQueueSendFromISR(tx_ppm_evt_queue, &ccount - last, NULL);
  last = ccount;
}

static void tx_ppm_timer_isr(void* arg)
{

}

static void tx_ppm_task(void* args)
{
  uint32_t ccount;

  for (;;)
  {
    if (xQueueReceive(tx_ppm_evt_queue, &ccount, portMAX_DELAY))
    {
      const int state = gpio_get_level(gpio_get_level(PPM_IO_NUM));
      ESP_LOGI(TAG, "Level %d, Cycles %d\n", state, ccount);
    }
  }
}

void tx_ppm_init(tx_ppm_callback_t ppm_cb)
{
  tx_ppm_cb = ppm_cb;
  tx_ppm_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  xTaskCreate(tx_ppm_task, "tx-ppm-task", 2048, NULL, 10, NULL);

  ESP_ERROR_CHECK( hw_timer_init(tx_ppm_timer_isr, NULL) );
  ESP_ERROR_CHECK( hw_timer_set_clkdiv(TIMER_CLKDIV_1) );

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
