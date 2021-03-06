#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "hostname.h"

#define MLINK_WIFI_AP_PASSWORD      CONFIG_ESP_WIFI_AP_PASSWORD
#define MLINK_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "wifi-softap";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap()
{
    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config_ap = {
        .ap = {
            .password = MLINK_WIFI_AP_PASSWORD,
            .max_connection = MLINK_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(MLINK_WIFI_AP_PASSWORD) == 0) {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set SSID from MAC address
    strcpy((char*)wifi_config_ap.ap.ssid, generate_hostname());

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap);
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);

    tcpip_adapter_ip_info_t ap_ip;
    ESP_ERROR_CHECK( tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_ip) );
    
    uint32_t ip = (uint32_t)(ap_ip.ip.addr);
    ESP_LOGI(TAG, "tcpip_adapter ap ip: %d.%d.%d.%d",
        (ip) & 0xff,
        (ip >> 8) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 24) & 0xff
    );
}

void wifi_init_ap(void)
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}

