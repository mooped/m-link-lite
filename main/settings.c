#include "settings.h"

#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "nvs_flash.h"

#include "hostname.h"

static char name[32];
static char ap_ssid[64];
static char ap_password[64];
static char ssid[64];
static char password[64];

const char* settings_get_name(void)
{
  return name;
}

const char* settings_get_ap_ssid(void)
{
  return ap_ssid;
}

const char* settings_get_ap_password(void)
{
  return ap_password;
}

const char* settings_get_ssid(void)
{
  return ssid;
}

const char* settings_get_password(void)
{
  return password;
}

void settings_set_name(const char* in_name)
{
  strncpy(name, in_name, sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
}

void settings_set_ap_ssid(const char* in_ap_ssid)
{
  strncpy(ap_ssid, in_ap_ssid, sizeof(ap_ssid) - 1);
  ap_ssid[sizeof(ap_ssid) - 1] = 0;
}

void settings_set_ap_password(const char* in_ap_password)
{
  strncpy(ap_password, in_ap_password, sizeof(ap_password) - 1);
  ap_password[sizeof(ap_password) - 1] = 0;
}

void settings_set_ssid(const char* in_ssid)
{
  strncpy(ssid, in_ssid, sizeof(ssid) - 1);
  ssid[sizeof(ssid) - 1] = 0;
}

void settings_set_password(const char* in_password)
{
  strncpy(password, in_password, sizeof(password) - 1);
  password[sizeof(password) - 1] = 0;
}

// Read all settings from NVS
esp_err_t settings_read(void)
{
  esp_err_t err;
  size_t length;
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK( nvs_open("nvs", NVS_READWRITE, &nvs_handle) );

  length = sizeof(name);
  err = nvs_get_str(nvs_handle, "bot_name", name, &length);
  if (err != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_ERROR_CHECK(err);
  }

  length = sizeof(ap_ssid);
  err = nvs_get_str(nvs_handle, "bot_ap_ssid", ap_ssid, &length);
  if (err != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_ERROR_CHECK(err);
  }

  length = sizeof(ap_password);
  err = nvs_get_str(nvs_handle, "bot_ap_password", ap_password, &length);
  if (err != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_ERROR_CHECK(err);
  }

  length = sizeof(ssid);
  err = nvs_get_str(nvs_handle, "bot_ssid", ssid, &length);
  if (err != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_ERROR_CHECK(err);
  }

  length = sizeof(password);
  err = nvs_get_str(nvs_handle, "bot_password", password, &length);
  if (err != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_ERROR_CHECK(err);
  }

  nvs_close(nvs_handle);

  return ESP_OK;
}

// Write current settings to NVS
esp_err_t settings_write(void)
{
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK( nvs_open("nvs", NVS_READWRITE, &nvs_handle) );
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "bot_name", name));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "bot_ap_ssid", ap_ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "bot_ap_password", ap_password));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "bot_ssid", ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "bot_password", password));
  ESP_ERROR_CHECK(nvs_commit(nvs_handle));
  nvs_close(nvs_handle);

  return ESP_OK;
}

// Apply default settings, in memory only
esp_err_t settings_apply_defaults(void)
{
  strcpy(name, CONFIG_BOT_NAME);
  char* hostname = generate_hostname();
  strcpy(ap_ssid, hostname);
  free(hostname);
  hostname = NULL;
  strcpy(ap_password, generate_password());
  strcpy(ssid, CONFIG_ESP_WIFI_SSID);
  strcpy(password, CONFIG_ESP_WIFI_PASSWORD);
  return ESP_OK;
}

// Apply default settings and erase NVS storage
esp_err_t settings_reset_defaults(void)
{
  // Erase NVS
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &nvs_handle));
  ESP_ERROR_CHECK(nvs_erase_all(nvs_handle));
  ESP_ERROR_CHECK(nvs_commit(nvs_handle));
  nvs_close(nvs_handle);

  // Apply default settings
  settings_apply_defaults();

  return ESP_OK;
}

void settings_init(void)
{
  settings_apply_defaults();
  settings_read();
}

