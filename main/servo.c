#if !defined(CONFIG_PLUS)

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

#include "servo.h"


#define PWM_IO_COUNT      6

#define ENABLE_IO_NUM     2

#define PWM_0_OUT_IO_NUM  4   // Servo 1
#define PWM_1_OUT_IO_NUM  5   // Servo 2
#define PWM_2_OUT_IO_NUM  12  // Servo 3
#define PWM_3_OUT_IO_NUM  15  // Servo 4
#define PWM_4_OUT_IO_NUM  14  // Servo 5
#define PWM_5_OUT_IO_NUM  13  // Servo 6

// PWM period 20ms (50hz)
#define PWM_PERIOD    (20000)

//static const char *TAG = "m-link-servo";

// pwm pin number
static const uint32_t pin_num[PWM_IO_COUNT] = {
  PWM_0_OUT_IO_NUM,
  PWM_1_OUT_IO_NUM,
  PWM_2_OUT_IO_NUM,
  PWM_3_OUT_IO_NUM,
  PWM_4_OUT_IO_NUM,
  PWM_5_OUT_IO_NUM,
};

// duties table, real_duty = duties[x]/PERIOD
static uint32_t duties[PWM_IO_COUNT] = {
  1500, 1500, 1500, 1500, 1500, 1500,
};

// phase table, delay = (phase[x]/360)*PERIOD
static float phase[PWM_IO_COUNT] = {
  0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
};

void servo_init(void)
{
  ESP_ERROR_CHECK( pwm_init(PWM_PERIOD, duties, PWM_IO_COUNT, pin_num) );
  ESP_ERROR_CHECK( pwm_set_phases(phase) );
  ESP_ERROR_CHECK( pwm_start() );

  gpio_config_t config;
  config.pin_bit_mask = (1ull<<(ENABLE_IO_NUM));
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK( gpio_config(&config) );
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 1) );
}

void servo_enable(void)
{
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 0) );
}

void servo_disable(void)
{
  ESP_ERROR_CHECK( gpio_set_level(ENABLE_IO_NUM, 1) );
}

void servo_set(int channel, int pulsewidth_ms)
{
  if (pulsewidth_ms <= 1000)
  {
    duties[channel] = 1000;
  }
  else if (pulsewidth_ms >= 2000)
  {
    duties[channel] = 2000;
  }
  else
  {
    duties[channel] = pulsewidth_ms;
  }
}

void servo_refresh(void)
{
  pwm_set_duties(duties);
  pwm_start();
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

