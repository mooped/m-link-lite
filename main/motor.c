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

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/pwm.h"

#include "motor.h"

#include "motor.h"

#include "motor.h"

#include "motor.h"

#define PWM_IO_COUNT      6

#define PWM_0_OUT_IO_NUM  4   // Motor 1A
#define PWM_1_OUT_IO_NUM  5   // Motor 1B
#define PWM_2_OUT_IO_NUM  15  // Motor 2A
#define PWM_3_OUT_IO_NUM  13  // Motor 2B
#define PWM_4_OUT_IO_NUM  12  // Motor 3A
#define PWM_5_OUT_IO_NUM  14  // Motor 3B

#define PWM_M1A_CHANNEL 0
#define PWM_M1B_CHANNEL 1
#define PWM_M2A_CHANNEL 2
#define PWM_M2B_CHANNEL 3
#define PWM_M3A_CHANNEL 4
#define PWM_M3B_CHANNEL 5

// PWM period 1000us (1Khz)
#define PWM_PERIOD    (1000)

static const char *TAG = "m-link-motor";

// pwm pin number
const uint32_t pin_num[PWM_IO_COUNT] = {
  PWM_0_OUT_IO_NUM,
  PWM_1_OUT_IO_NUM,
  PWM_2_OUT_IO_NUM,
  PWM_3_OUT_IO_NUM,
  PWM_4_OUT_IO_NUM,
  PWM_5_OUT_IO_NUM,
};

// duties table, real_duty = duties[x]/PERIOD
uint32_t duties[PWM_IO_COUNT] = {
  0, 0, 0, 0, 0, 0,
};

// phase table, delay = (phase[x]/360)*PERIOD
float phase[PWM_IO_COUNT] = {
  0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
};

void motor_init(void)
{
  ESP_ERROR_CHECK( pwm_init(PWM_PERIOD, duties, PWM_IO_COUNT, pin_num) );
  ESP_ERROR_CHECK( pwm_set_phases(phase) );
  ESP_ERROR_CHECK( pwm_start() );
}

void motor_set(int speed, int channel_a, int channel_b)
{
  // Coast
  if (speed == MOTOR_COAST)
  {
    duties[channel_a] = 0;
    duties[channel_b] = 0;
  }
  // Brake
  else if (speed == MOTOR_BRAKE)
  {
    duties[channel_a] = PWM_PERIOD;
    duties[channel_b] = PWM_PERIOD;
  }
  // Drive
  else
  {
    if (speed > MOTOR_COAST)
    {
      duties[channel_a] = 0;
      duties[channel_b] = (speed > MOTOR_MAX) ? MOTOR_MAX : speed;
    }
    else
    {
      duties[channel_a] = (speed < -MOTOR_MAX) ? MOTOR_MAX : -speed;
      duties[channel_b] = 0;
    }
  }
}

void motor_set_all(int m1, int m2, int m3)
{
  motor_set(m1, PWM_M1A_CHANNEL, PWM_M1B_CHANNEL);
  motor_set(m2, PWM_M2A_CHANNEL, PWM_M2B_CHANNEL);
  motor_set(m3, PWM_M3A_CHANNEL, PWM_M3B_CHANNEL);

  pwm_set_duties(duties);
  pwm_start();
}

