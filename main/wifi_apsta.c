#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "settings.h"

/* The examples use WiFi configuration that you can set via project configuration menu
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define MLINK_WIFI_SSID "mywifissid"
*/
#define MLINK_ESP_MAXIMUM_RETRY    CONFIG_ESP_MAXIMUM_RETRY
#define MLINK_WIFI_AP_PASSWORD     CONFIG_ESP_WIFI_AP_PASSWORD
#define MLINK_MAX_STA_CONN         CONFIG_ESP_MAX_STA_CONN

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi-apsta";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MLINK_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip: %s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_apsta_impl(void)
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config_sta = {
        .sta = {
        }
    };
    wifi_config_t wifi_config_ap = {
        .ap = {
            .password = MLINK_WIFI_AP_PASSWORD,
            .max_connection = MLINK_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        }
    };

    // Set STA SSID and password from settings
    strcpy((char*)wifi_config_sta.sta.ssid, settings_get_ssid());
    strcpy((char*)wifi_config_sta.sta.password, settings_get_password());

    // Set AP SSID and password from settings
    strcpy((char*)wifi_config_ap.ap.ssid, settings_get_ap_ssid());
    strcpy((char*)wifi_config_ap.ap.password, settings_get_ap_password());

    /* Setting a password implies station will connect to all security modes including WEP/WPA.
        * However these modes are deprecated and not advisable to be used. Incase your Access point
        * doesn't support WPA2, these mode can be enabled by commenting below line */

    if (strlen((char *)wifi_config_sta.sta.password))
    {
        wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    // Start STA only if we have an SSID
    const bool use_sta = strlen((char*)wifi_config_sta.sta.ssid) > 0;

    if (use_sta)
    {
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
    }
    else
    {
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
    }

    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap);
    if (use_sta)
    {
      ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
    }
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_ap%s finished.", use_sta ? "sta" : "");

    ESP_LOGI(TAG, "AP SSID:%s, password:%s", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);

    tcpip_adapter_ip_info_t ap_ip;
    ESP_ERROR_CHECK( tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_ip) );
    
    uint32_t ip = (uint32_t)(ap_ip.ip.addr);
    ESP_LOGI(TAG, "tcpip_adapter AP ip: %d.%d.%d.%d",
        (ip) & 0xff,
        (ip >> 8) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 24) & 0xff
    );

    // STA not configured, we are done
    if (!use_sta)
    {
      return;
    }

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK( tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ap_ip) );
    
    ip = (uint32_t)(ap_ip.ip.addr);
    ESP_LOGI(TAG, "tcpip_adapter STA ip: %d.%d.%d.%d",
        (ip) & 0xff,
        (ip >> 8) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 24) & 0xff
    );

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}

void wifi_init_apsta(void)
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_APSTA");
    wifi_init_apsta_impl();
}
