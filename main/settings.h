#pragma once

#include "esp_err.h"

const char* settings_get_name(void);
const char* settings_get_ssid(void);
const char* settings_get_password(void);

void settings_set_name(const char* in_name);
void settings_set_ssid(const char* in_ssid);
void settings_set_password(const char* in_password);

esp_err_t settings_read(void);
esp_err_t settings_write(void);
esp_err_t settings_apply_defaults(void);
esp_err_t settings_reset_defaults(void);

void settings_init(void);

