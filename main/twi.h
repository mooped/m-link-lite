#pragma once

#define WRITE_BIT   0
#define READ_BIT    1

#define ACK_CHECK_EN    0x1
#define ACK_CHECK_DIS   0x0
#define ACK_VAL         0x0
#define NACK_VAL        0x1
#define LAST_NACK_VAL   0x2

#define I2C_SDA_GPIO_NUM   13
#define I2C_SCL_GPIO_NUM   12
#define I2C_MASTER_NUM     0

esp_err_t twi_init(void);

esp_err_t twi_read_bytes(uint8_t addr, uint8_t* buffer, size_t count);

