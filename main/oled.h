// SSD1306 OLED driver
#pragma once

void oled_init(void);
void oled_at(uint8_t seg, uint8_t page);
void oled_clear(void);
void oled_print(const char* str);
void oled_glyph(const uint8_t* data, size_t length);
void oled_number_byte(uint8_t num);
void oled_number_half(uint16_t num);
void oled_number_voltage(uint16_t num);
void oled_print_mac(uint8_t mac_addr[6]);

