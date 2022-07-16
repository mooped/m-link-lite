#include "hostname.h"

#include "esp_netif.h"

#define HOSTNAME_PREFIX CONFIG_HOSTNAME_PREFIX

char* generate_hostname()
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", HOSTNAME_PREFIX, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

