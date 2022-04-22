#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/i2c.h"

#include "twi.h"

esp_err_t twi_init(void)
{
  int i2c_master_port = I2C_MASTER_NUM;
  i2c_config_t config;
  config.mode = I2C_MODE_MASTER;
  config.sda_io_num = I2C_SDA_GPIO_NUM;
  config.sda_pullup_en = 1;
  config.scl_io_num = I2C_SCL_GPIO_NUM;
  config.scl_pullup_en = 1;
  config.clk_stretch_tick = 300;
  ESP_ERROR_CHECK( i2c_driver_install(i2c_master_port, config.mode) );
  ESP_ERROR_CHECK( i2c_param_config(i2c_master_port, &config) );
  return ESP_OK;
}

esp_err_t twi_read_bytes(uint8_t addr, uint8_t* buffer, size_t count)
{
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read(cmd, buffer, count, LAST_NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

