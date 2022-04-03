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

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/pwm.h"

#define PWM_IO_COUNT      3

#define PWM_0_OUT_IO_NUM  5   // LED 1
#define PWM_1_OUT_IO_NUM  4   // LED 2
#define PWM_2_OUT_IO_NUM  2   // LED 2

#define PWM_LED1_CHANNEL  0
#define PWM_LED2_CHANNEL  1
#define PWM_LEDM_CHANNEL  2

// PWM period 1024us(~1Khz)
#define PWM_PERIOD    (1024)

static const char *TAG = "m-link-rx-pwm";

// pwm pin number
const uint32_t pin_num[PWM_IO_COUNT] = {
  PWM_0_OUT_IO_NUM,
  PWM_1_OUT_IO_NUM,
  PWM_2_OUT_IO_NUM,
};

// duties table, real_duty = duties[x]/PERIOD
static uint32_t duties[PWM_IO_COUNT] = {
  0, 0, 0,
};

// phase table, delay = (phase[x]/360)*PERIOD
static float phase[PWM_IO_COUNT] = {
  0.f, 0.f, 0.f,
};

void tx_pwm_init(void)
{
  ESP_ERROR_CHECK( pwm_init(PWM_PERIOD, duties, PWM_IO_COUNT, pin_num) );
  ESP_ERROR_CHECK( pwm_set_phases(phase) );
  ESP_ERROR_CHECK( pwm_start() );
}

void tx_pwm_set_leds(unsigned int status1, unsigned int status2, unsigned int module)
{
  status1 = (status1 <= 1024) ? status1 : 1024;
  status2 = (status2 <= 1024) ? status2 : 1024;
  module = (module <= 1024) ? (1024 - module) : 0;
  ESP_LOGI(TAG, "PWM update duty %d, %d, %d\n", status1, status2, 1024 - module);
  duties[PWM_LED1_CHANNEL] = status1;
  duties[PWM_LED2_CHANNEL] = status2;
  duties[PWM_LEDM_CHANNEL] = module;
}

void tx_pwm_update(void)
{
  pwm_set_duties(duties);
  pwm_start();
}

