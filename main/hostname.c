#include "hostname.h"

#include "esp_netif.h"

#define HOSTNAME_PREFIX         CONFIG_HOSTNAME_PREFIX
#define PASSWORD_ONETIMEPAD     CONFIG_ESP_WIFI_AP_PASSWORD

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

char* generate_password()
{
  uint8_t mac[8];
  uint8_t password[8];
  uint8_t secret[8];
  static char* password;
  memset(mac, 0, 8);
  memset(password, 0, 8);
  memset(secret, 0, 8);
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
}
