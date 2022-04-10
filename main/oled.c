#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/i2c.h"

#include "oled.h"
#include "font.h"

#define OLED_SDA_GPIO_NUM   13
#define OLED_SCL_GPIO_NUM   12
#define OLED_MASTER_NUM     0

#define WRITE_BIT   0
#define READ_BIT    1

#define ACK_CHECK_EN    0x1
#define ACK_CHECK_DIS   0x0
#define ACK_VAL         0x0
#define NACK_VAL        0x1
#define LAST_NACK_VAL   0x2

esp_err_t twi_ssd1306_init(void)
{
  int i2c_master_port = OLED_MASTER_NUM;
  i2c_config_t config;
  config.mode = I2C_MODE_MASTER;
  config.sda_io_num = OLED_SDA_GPIO_NUM;
  config.sda_pullup_en = 1;
  config.scl_io_num = OLED_SCL_GPIO_NUM;
  config.scl_pullup_en = 1;
  config.clk_stretch_tick = 300;
  ESP_ERROR_CHECK( i2c_driver_install(i2c_master_port, config.mode) );
  ESP_ERROR_CHECK( i2c_param_config(i2c_master_port, &config) );
  return ESP_OK;
}

inline esp_err_t twi_write_addr(i2c_cmd_handle_t cmd, uint8_t addr)
{
  return i2c_master_write_byte(cmd, addr | WRITE_BIT, ACK_CHECK_EN);
}

esp_err_t twi_ssd1306_command(uint8_t addr, uint8_t command)
{
  /*
  twi_start();
  twi_write_addr(addr);
  twi_write(0x00);
  twi_write(command);
  twi_stop();
  */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, addr);
  i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, command, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(OLED_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

esp_err_t twi_ssd1306_commands(uint8_t addr, const uint8_t* commands, size_t count)
{
  /*
  twi_start();
  twi_write_addr(addr);
  twi_write(0x00);
  for (size_t i = 0; i < count; ++i)
  {
    twi_write(commands[i]);
  }
  twi_stop();
  */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, addr);
  i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
  for (size_t i = 0; i < count; ++i)
  {
    i2c_master_write_byte(cmd, commands[i], ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(OLED_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

esp_err_t twi_ssd1306_send_data(uint8_t addr, const uint8_t* data, size_t count)
{
  /*
  twi_start();
  twi_write_addr(addr);
  twi_write(0x40);
  for (size_t i = 0; i < count; ++i)
  {
    twi_write(data[i]);
  }
  twi_stop();
  */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, addr);
  i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
  for (size_t i = 0; i < count; ++i)
  {
    i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(OLED_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

esp_err_t twi_ssd1306_send_data_glyph(uint8_t addr, const uint8_t* data)
{
  /*
  twi_start();
  twi_write_addr(addr);
  twi_write(0x40);
  size_t i = 0;
  while (1) {
    uint8_t byte = data[i++];
    twi_write(byte);
    if (byte == 0)
    {
      break;
    }
  }
  twi_stop();
  */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, addr);
  i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
  size_t i = 0;
  while (1)
  {
    uint8_t byte = data[i++];
    i2c_master_write_byte(cmd, byte, ACK_CHECK_EN);
    if (byte == 0)
    {
      break;
    }
  }
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(OLED_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

esp_err_t twi_ssd1306_send_data_repeated(uint8_t addr, uint8_t data, size_t count)
{
  /*
  twi_start();
  twi_write_addr(addr);
  twi_write(0x40);
  for (size_t i = 0; i < count; ++i)
  {
    twi_write(data);
  }
  twi_stop();
  */
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  twi_write_addr(cmd, addr);
  i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
  for (size_t i = 0; i < count; ++i)
  {
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(OLED_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

const uint8_t contrast_packet[] = {
  0x81,
  0x00,
};

void oled_init(void)
{
  // Initialise I2C driver
  ESP_ERROR_CHECK( twi_ssd1306_init() );

  // Initialise the OLED
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xae) ); // Display off
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xd5) ); // Clock dividier
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x80) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xa8) ); // Multiplex ratio
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x3f) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xd3) ); // Display offset
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x00) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x8d) ); // Charge pump
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x14) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xa1) ); // Rotate 180 (seg remap)
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xc8) ); // Rotate 180 (com remap)
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xda) ); // Com pin sequence (sequential, left to right remap)
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x22) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x81) ); // Set contrast
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xcf) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xd9) ); // Set precharge
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xf1) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xdb) ); // Set vcomdetect
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x40) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xa4) ); // Set output to RAM contents
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xa6) ); // Set normaldisplay
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xaf) ); // Set display on

  oled_clear();
}

void oled_at(uint8_t seg, uint8_t page)
{
  // Set segment
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x00 | (seg & 0x0f)) );
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0x10 | ((seg >> 4) & 0x0f)) );

  // Set page
  ESP_ERROR_CHECK( twi_ssd1306_command(OLED_ADDR, 0xB0 | (page & 0x07)) );
}

void oled_clear(void)
{
  // Clear the screen
  for (uint8_t page = 0; page < 8; ++page)
  {
    oled_at(0, page);

    // Clear the row
    ESP_ERROR_CHECK( twi_ssd1306_send_data_repeated(OLED_ADDR, 0x00, 128) );
  }
}

void oled_print(const char* str)
{
  while (*str)
  {
    // 32 is a space, anything below is non-printable
    if (*str < 33)
    {
      ESP_ERROR_CHECK( twi_ssd1306_send_data_repeated(OLED_ADDR, 0x00, 3) );
    }
    // Send bytes from character data in progmem (null terminated)
    else
    {
      const uint8_t* fa = font[*str - 33];
      ESP_ERROR_CHECK( twi_ssd1306_send_data_glyph(OLED_ADDR, fa) );
    }

    ++str;
  };
}

void oled_number_byte(uint8_t num)
{
  // Parse number to a string starting from the end
  char buffer[4] = {
    0x00
  };
  char* ptr = &buffer[7];
  uint8_t digit = 0;
  do {
    digit = num % 10;
    num = num / 10;
    (*--ptr) = '0' + digit;
  } while (num);

  // Print the number
  oled_print(ptr);
}

void oled_number_half(uint16_t num)
{
  // Parse number to a string starting from the end
  char buffer[6] = {
    0x00
  };
  char* ptr = &buffer[7];
  uint16_t digit = 0;
  do {
    digit = num % 10;
    num = num / 10;
    (*--ptr) = '0' + digit;
  } while (num);

  // Print the number
  oled_print(ptr);
}

