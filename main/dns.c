#include "dns.h"

#include "esp_err.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "mdns.h"

#include <sys/socket.h>
#include <netdb.h>

#include "captDns.h"

#include "settings.h"

static const char* TAG = "mdns";

static void initialise_mdns(void)
{
  // Get hostname (prefix + 4 bytes of MAC address)
  const char* hostname = settings_get_ap_ssid();

  // Initialize mDNS
  ESP_ERROR_CHECK( mdns_init() );

  // Set mDNS hostname so we can advertise services
  ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
  ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);

  // Set mDNS instance name
  ESP_ERROR_CHECK( mdns_instance_name_set("M-Link Lite mDNS") );

  // Add a HTTP service
  ESP_ERROR_CHECK( mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0) );
}

void mlink_dns_init(void)
{
  // Initialise multicast DNS
  initialise_mdns();

  // Initialise captive DNS also
  captdnsInit();
}

