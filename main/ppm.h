#pragma once

#define PPM_NUM_CHANNELS    9

typedef void (*ppm_callback_t)(const uint32_t channels[PPM_NUM_CHANNELS]);

esp_err_t ppm_init(ppm_callback_t ppm_cb);
