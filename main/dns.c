#include "dns.h"

#include "esp_err.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "mdns.h"

#include <sys/socket.h>
#include <netdb.h>

static const char* TAG = "mdns";

static char* generate_hostname()
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", "m-link", mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

static void initialise_mdns(void)
{
    char* hostname = generate_hostname();
    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set("M-Link Lite mDNS") );

    ESP_ERROR_CHECK( mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0) );

    free(hostname);
}

void mlink_dns_init(void)
{
  initialise_mdns();
}

