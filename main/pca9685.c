#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/i2c.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/gpio.h"

#include "twi.h"

#include "pca9685.h"

#define PCA9685_ADDR (0x40<<1) // All address inputs set to zero

#define PCA9685_NUM_CHANNELS   16

typedef enum
{
  PCA9685_MODE1 = 0,
  PCA9685_MODE2 = 1,
  PCA9685_SUBADR1 = 2,
  PCA9685_SUBADR2 = 3,
  PCA9685_SUBADR3 = 4,
  PCA9685_ALLCALLADR = 5,
  PCA9685_LED0_ON_L = 6,
  PCA9685_LED0_ON_H = 7,
  PCA9685_LED0_OFF_L = 8,
  PCA9685_LED0_OFF_H = 9,
  PCA9685_LED1_ON_L = 10,
  PCA9685_LED1_ON_H = 11,
  PCA9685_LED1_OFF_L = 12,
  PCA9685_LED1_OFF_H = 13,
  PCA9685_LED2_ON_L = 14,
  PCA9685_LED2_ON_H = 15,
  PCA9685_LED2_OFF_L = 16,
  PCA9685_LED2_OFF_H = 17,
  PCA9685_LED3_ON_L = 18,
  PCA9685_LED3_ON_H = 19,
  PCA9685_LED3_OFF_L = 20,
  PCA9685_LED3_OFF_H = 21,
  PCA9685_LED4_ON_L = 22,
  PCA9685_LED4_ON_H = 23,
  PCA9685_LED4_OFF_L = 24,
  PCA9685_LED4_OFF_H = 25,
  PCA9685_LED5_ON_L = 26,
  PCA9685_LED5_ON_H = 27,
  PCA9685_LED5_OFF_L = 28,
  PCA9685_LED5_OFF_H = 29,
  PCA9685_LED6_ON_L = 30,
  PCA9685_LED6_ON_H = 31,
  PCA9685_LED6_OFF_L = 32,
  PCA9685_LED6_OFF_H = 33,
  PCA9685_LED7_ON_L = 34,
  PCA9685_LED7_ON_H = 35,
  PCA9685_LED7_OFF_L = 36,
  PCA9685_LED7_OFF_H = 37,
  PCA9685_LED8_ON_L = 38,
  PCA9685_LED8_ON_H = 39,
  PCA9685_LED8_OFF_L = 40,
  PCA9685_LED8_OFF_H = 41,
  PCA9685_LED9_ON_L = 42,
  PCA9685_LED9_ON_H = 43,
  PCA9685_LED9_OFF_L = 44,
  PCA9685_LED9_OFF_H = 45,
  PCA9685_LED10_ON_L = 46,
  PCA9685_LED10_ON_H = 47,
  PCA9685_LED10_OFF_L = 48,
  PCA9685_LED10_OFF_H = 49,
  PCA9685_LED11_ON_L = 50,
  PCA9685_LED11_ON_H = 51,
  PCA9685_LED11_OFF_L = 52,
  PCA9685_LED11_OFF_H = 53,
  PCA9685_LED12_ON_L = 54,
  PCA9685_LED12_ON_H = 55,
  PCA9685_LED12_OFF_L = 56,
  PCA9685_LED12_OFF_H = 57,
  PCA9685_LED13_ON_L = 58,
  PCA9685_LED13_ON_H = 59,
  PCA9685_LED13_OFF_L = 60,
  PCA9685_LED13_OFF_H = 61,
  PCA9685_LED14_ON_L = 62,
  PCA9685_LED14_ON_H = 63,
  PCA9685_LED14_OFF_L = 64,
  PCA9685_LED14_OFF_H = 65,
  PCA9685_LED15_ON_L = 66,
  PCA9685_LED15_ON_H = 67,
  PCA9685_LED15_OFF_L = 68,
  PCA9685_LED15_OFF_H = 69,
  // ...RESERVED...
  PCA9685_ALL_LED_ON_L = 250,
  PCA9685_ALL_LED_ON_H = 251,
  PCA9685_ALL_LED_OFF_L = 252,
  PCA9685_ALL_LED_OFF_H = 253,
  PCA9685_PRE_SCALE = 254,
  //PCA9685_TestMode = 255, // Reserved
} PCA9685_register_t;

typedef enum
{
  PCA9685_M1_RESTART  = 0x80,
  PCA9685_M1_EXTCLK   = 0x40,
  PCA9685_M1_AI       = 0x20,
  PCA9685_M1_SLEEP    = 0x10,
  PCA9685_M1_SUB1     = 0x08,
  PCA9685_M1_SUB2     = 0x04,
  PCA9685_M1_SUB3     = 0x02,
  PCA9685_M1_ALLCALL  = 0x01,
} PCA9685_mode1_t;

typedef enum
{
  // BITS 7 to 5 RESERVED
  PCA9685_M2_INVRT    = 0x10,
  PCA9685_M2_OCH      = 0x08,
  PCA9685_M2_OUTDRV   = 0x04,
  PCA9685_M2_OUTNE1   = 0x02,
  PCA9685_M2_OUTNE0   = 0x01,
} PCA9685_mode2_t;

#define PCA9685_HIGH_MASK   0x0f    // Mask for the high byte of LED ON/OFF times
#define PCA9685_FULL_ON     0x10    // Special 'full on' value when used in *_ON_H register
#define PCA9685_FULL_OFF    0x10    // Special 'full off' value when used in *_OFF_H register

inline esp_err_t twi_write_addr(i2c_cmd_handle_t cmd, uint8_t addr)
{
  return i2c_master_write_byte(cmd, addr | WRITE_BIT, ACK_CHECK_EN);
}

static esp_err_t twi_pca9685_reset(void)
{
  /*
     twi_start();
     twi_write_addr(0x00); // Software reset address
     twi_write(0x06);      // Reset magic number
     twi_stop();
     */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, 0x00);
  i2c_master_write_byte(cmd, 0x06, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

static esp_err_t twi_pca9685_write_register(uint8_t reg, uint8_t value)
{
  /*
     twi_start();
     twi_write_addr(addr);
     twi_write(register)
     twi_write(value);
     twi_stop();
     */ 
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, PCA9685_ADDR);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

static esp_err_t twi_pca9685_write_registers(uint8_t reg, const uint8_t* values, size_t count)
{
  /*
     twi_start();
     twi_write_addr(addr);
     twi_write(register);
     for (size_t i = 0; i < count; ++i)
     {
     twi_write(values[i]);
     }
     twi_stop();
     */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, PCA9685_ADDR);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  for (size_t i = 0; i < count; ++i)
  {
    i2c_master_write_byte(cmd, values[i], ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

#if 0
static esp_err_t twi_pca9685_read_registers(uint8_t reg, uint8_t* values, size_t count)
{
  // TODO: Implement me
  uint8_t buffer[] = { reg };
  if (I2C_BB_WriteRead(PCA9685_ADDR, buffer, 1, values, count))
  {
    while (I2C_BB_IsBusy())
    {

    }
    return true;
  }
  return false;
}
#endif

void pca9685_initialize(void)
{
  // Software reset
  ESP_ERROR_CHECK(twi_pca9685_reset());

  // Write to the mode registers setting auto-increment with the first write
  // MODE1: Stay in sleep, do not enable restart, external clock, or any of the extra addresses
  // MODE2: No inversion, outputs change on stop, outputs use totem pole drive, standard OE behaviour
  uint8_t mode_buffer[] = { PCA9685_M1_AI | PCA9685_M1_SLEEP, PCA9685_M2_OUTDRV };
  ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_MODE1, mode_buffer, sizeof(mode_buffer)));

  // Set all outputs to always off duty cycle
  // Set 500hz frequency
  // prescale_value = round(osc_clock / (4096 * update_rate)) - 1
  // Internal oscillator is 25mhz
  uint8_t pwm_buffer[] = { 0x00, 0x00, 0x00, PCA9685_FULL_OFF, 12};    // { ALL_ON_L, ALL_ON_H, ALL_OFF_L, ALL_OFF_H, PRE_SCALE }
ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_ALL_LED_ON_L, pwm_buffer, sizeof(pwm_buffer)));

// Disable sleep mode now PRE_SCALE is set
ESP_ERROR_CHECK(twi_pca9685_write_register(PCA9685_MODE1, PCA9685_M1_AI));
}

void pca9685_set_raw(int channel, int value)
{
  if (channel < PCA9685_NUM_CHANNELS)
  {
    if (value <= 0)
    {
      pca9685_set_off(channel);
      return;
    }
    uint8_t buffer[] = { 0x00, 0x00, value & 0xff, (value >> 8) & PCA9685_HIGH_MASK};
    ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_LED0_ON_L + 4 * channel, buffer, sizeof(buffer)));
  }
}

void pca9685_set_all_raw(int value)
{
  if (value <= 0)
  {
    pca9685_set_all_off();
    return;
  }
  uint8_t buffer[] = { 0x00, 0x00, value & 0xff, (value >> 8) & PCA9685_HIGH_MASK};
  ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_ALL_LED_ON_L, buffer, sizeof(buffer)));
}

void pca9685_set_microseconds(int channel, int pulsewidth)
{
  if (channel < PCA9685_NUM_CHANNELS)
  {
    const int period = 1000000 / 500; // 1,000,000 microseconds in a second, our period is a 500th of that
    int value = (pulsewidth * 4095) / period;
    if (value <= 0)
    {
      pca9685_set_off(channel);
      return;
    }
    uint8_t buffer[] = { 0x00, 0x00, value & 0xff, (value >> 8) & PCA9685_HIGH_MASK};
    ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_LED0_ON_L + 4 * channel, buffer, sizeof(buffer)));
  }
}

void pca9685_set_all_microseconds(int pulsewidth)
{
  const int period = 1000000 / 500; // 1,000,000 microseconds in a second, our period is a 500th of that
  int value = (pulsewidth * 4095) / period ;
  if (value <= 0)
  {
    pca9685_set_all_off();
    return;
  }
  uint8_t buffer[] = { 0x00, 0x00, value & 0xff, (value >> 8) & PCA9685_HIGH_MASK};
  ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_ALL_LED_ON_L, buffer, sizeof(buffer)));
}

void pca9685_set_on(int channel)
{
  if (channel < PCA9685_NUM_CHANNELS)
  {
    uint8_t buffer[] = { 0x00, PCA9685_FULL_ON, 0x00, 0x00 };    // { ALL_ON_L, ALL_ON_H, ALL_OFF_L, ALL_OFF_H }
  ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_LED0_ON_L + 4 * channel, buffer, sizeof(buffer)));
  }
}

void pca9685_set_all_on(void)
{
  uint8_t buffer[] = { 0x00, PCA9685_FULL_ON, 0x00, 0x00 };    // { ALL_ON_L, ALL_ON_H, ALL_OFF_L, ALL_OFF_H }
  ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_ALL_LED_ON_L, buffer, sizeof(buffer)));
}

void pca9685_set_off(int channel)
{
  if (channel < PCA9685_NUM_CHANNELS)
  {
    uint8_t buffer[] = { 0x00, 0x00, 0x00, PCA9685_FULL_OFF };    // { ALL_ON_L, ALL_ON_H, ALL_OFF_L, ALL_OFF_H }
    ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_LED0_ON_L + 4 * channel, buffer, sizeof(buffer)));
  }
}

void pca9685_set_all_off(void)
{
  uint8_t buffer[] = { 0x00, 0x00, 0x00, PCA9685_FULL_OFF };    // { ALL_ON_L, ALL_ON_H, ALL_OFF_L, ALL_OFF_H }
ESP_ERROR_CHECK(twi_pca9685_write_registers(PCA9685_ALL_LED_ON_L, buffer, sizeof(buffer)));
}
