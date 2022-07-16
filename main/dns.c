#include "dns.h"

#include "esp_err.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "mdns.h"

#include <sys/socket.h>
#include <netdb.h>

#include "hostname.h"

static const char* TAG = "mdns";

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

    tcpip_adapter_dhcp_status_t status;
    tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_AP, &status);
    ESP_LOGI(TAG, "DHCPS Status: %d", status);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

    free(hostname);
}

void mlink_dns_init(void)
{
  initialise_mdns();
}

