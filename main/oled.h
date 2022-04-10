// SSD1306 OLED driver
#pragma once

#define OLED_ADDR (0x3c<<1)

esp_err_t twi_ssd1306_command(uint8_t addr, uint8_t command);
esp_err_t twi_ssd1306_commands(uint8_t addr, const uint8_t* commands, size_t count);
esp_err_t twi_ssd1306_send_data(uint8_t addr, const uint8_t* data, size_t count);
esp_err_t twi_ssd1306_send_data_glyph(uint8_t addr, const uint8_t* data);
esp_err_t twi_ssd1306_send_data_repeated(uint8_t addr, uint8_t data, size_t count);

void oled_init(void);
void oled_at(uint8_t seg, uint8_t page);
void oled_clear(void);
void oled_print(const char* str);
void oled_number_byte(uint8_t num);
void oled_number_half(uint16_t num);

