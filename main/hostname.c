#include <string.h>

#include "esp_netif.h"

#include "hostname.h"

#define HOSTNAME_PREFIX         CONFIG_HOSTNAME_PREFIX
#define PASSWORD_ONETIMEPAD     CONFIG_ESP_WIFI_AP_PASSWORD
#define LENGTH 8

char* generate_hostname()
{
    uint8_t mac[6];
    static char* hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", HOSTNAME_PREFIX, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

char* hex = "0123456789abcdef";

int hexlookup(char h)
{
  const char lower = tolower(h);
  for (int i = 0; i < 16; ++i)
  {
    if (hex[i] == lower)
    {
      return i;
    }
  }
  return 0;
}

char* generate_password()
{
  uint8_t raw_mac[6];
  esp_read_mac(raw_mac, ESP_MAC_WIFI_STA);

  char mac[LENGTH + 1];
  static char password[LENGTH + 1];
  char secret[LENGTH + 1];

  memset(mac, 0, sizeof(mac));
  sprintf(mac, "%02X%02X%02X00", raw_mac[3], raw_mac[4], raw_mac[5]);

  memset(secret, 0, sizeof(secret));
  memcpy(secret, PASSWORD_ONETIMEPAD, LENGTH);
  memset(password, 0, sizeof(password));

  for (int i = 0; i < LENGTH; ++i)
  {
    password[i] = hex[hexlookup(mac[i]) ^ hexlookup(secret[i])];
  }

  return password;
}

