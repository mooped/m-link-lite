/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "rom/ets_sys.h"
#include "rom/crc.h"
#include "event.h"

static const char *TAG = "m-link";

static xQueueHandle transport_event_queue;

static void (*transport_parse_cb)(uint8_t[ESP_NOW_ETH_ALEN], void*, size_t length) = NULL;
static void (*transport_sent_cb)(void) = NULL;

uint8_t mlink_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void transport_espnow_deinit(mlink_send_param_t *send_param);

static mlink_send_param_t* send_param = NULL;

/* WiFi should start before using ESPNOW */
static void transport_wifi_init(void)
{
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start());

    /* In order to simplify example, channel is set after WiFi started.
     * This is not necessary in real application if the two devices have
     * been already on the same channel.
     */
    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, 0) );
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void mlink_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    mlink_event_t evt;
    mlink_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL)
    {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(transport_event_queue, &evt, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void mlink_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    mlink_event_t evt;
    mlink_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(transport_event_queue, &evt, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
int mlink_data_parse(uint8_t mac_addr[ESP_NOW_ETH_ALEN], uint8_t *data, uint16_t data_len)
{
    mlink_data_t *buf = (mlink_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(mlink_data_t))
    {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    crc = buf->crc;
    buf->crc = 0;
    crc_cal = crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc)
    {
        if (transport_parse_cb)
        {
          (*transport_parse_cb)(mac_addr, buf->payload, data_len - sizeof(mlink_data_t));
        }
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void mlink_data_prepare(mlink_send_param_t *send_param, uint8_t* payload, size_t payload_length)
{
    mlink_data_t *buf = (mlink_data_t *)send_param->buffer;
    int i = 0;

    send_param->len = sizeof(mlink_data_t) + payload_length;

    assert(send_param->len >= sizeof(mlink_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? MLINK_DATA_BROADCAST : MLINK_DATA_UNICAST;
    buf->crc = 0;
    for (i = 0; i < send_param->len - sizeof(mlink_data_t); i++)
    {
        buf->payload[i] = payload[i];
    }
    buf->crc = crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void mlink_task(void *pvParameter)
{
    mlink_event_t evt;

    while (xQueueReceive(transport_event_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
            case ESPNOW_SEND_CB:
            {
                mlink_event_send_cb_t *send_cb = &evt.info.send_cb;
                ESP_LOGD(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);
                (*transport_sent_cb)();
            } break;
            case ESPNOW_RECV_CB:
            {
                mlink_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                mlink_data_parse(recv_cb->mac_addr, recv_cb->data, recv_cb->data_len);
                free(recv_cb->data);
            } break;
            case ESPNOW_SEND_REQUEST:
            {
              mlink_event_send_request_t* send_request = &evt.info.send_request;
              mlink_data_prepare(send_param, send_request->data, send_request->data_len);
              ESP_ERROR_CHECK( esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) );
              free(send_request->data);
            } break;
            case ESPNOW_ADD_PEER:
            {
              mlink_event_add_peer_t* add_peer = &evt.info.add_peer;
              // Add a peer if we've not already
              if (esp_now_is_peer_exist(add_peer->mac_addr) == false)
              {
                esp_now_peer_info_t* peer = malloc(sizeof(esp_now_peer_info_t));
                if (peer != NULL)
                {
             	    ESP_LOGI(TAG, "Add peer "MACSTR".", MAC2STR(add_peer->mac_addr));
                  memset(peer, 0, sizeof(esp_now_peer_info_t));
                  peer->channel = CONFIG_ESPNOW_CHANNEL;
                  peer->ifidx = ESPNOW_WIFI_IF;
                  peer->encrypt = false;
                  memcpy(peer->peer_addr, add_peer->mac_addr, ESP_NOW_ETH_ALEN);
                  ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                  free(peer);
                }
                else
                {
                  ESP_LOGE(TAG, "Malloc peer information fail.");
                }
              }
            } break;
            default:
            {
              ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            } break;
        }
    }
}

static esp_err_t transport_espnow_init(void)
{
    transport_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(mlink_event_t));
    if (transport_event_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(mlink_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(mlink_recv_cb) );

    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(transport_event_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, mlink_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(mlink_send_param_t));
    memset(send_param, 0, sizeof(mlink_send_param_t));
    if (send_param == NULL)
    {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(transport_event_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 0;
    send_param->magic = esp_random();
    send_param->count = CONFIG_ESPNOW_SEND_COUNT;
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (send_param->buffer == NULL)
    {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        vSemaphoreDelete(transport_event_queue);
        free(send_param);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, mlink_broadcast_mac, ESP_NOW_ETH_ALEN);
    mlink_data_prepare(send_param, NULL, 0);

    xTaskCreate(mlink_task, "mlink_task", 2048, send_param, 4, NULL);

    return ESP_OK;
}

/*
static void transport_espnow_deinit(mlink_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    send_param = NULL;
    vSemaphoreDelete(transport_event_queue);
    esp_now_deinit();
}
*/

esp_err_t transport_init(void (*parse_cb)(uint8_t[ESP_NOW_ETH_ALEN], void*, size_t), void (*sent_cb)(void))
{
  if (parse_cb == NULL || sent_cb == NULL)
  {
    return ESP_FAIL;
  }

  transport_parse_cb = parse_cb;
  transport_sent_cb = sent_cb;

  transport_wifi_init();
  return transport_espnow_init();
}

esp_err_t transport_add_peer(uint8_t mac_addr[ESP_NOW_ETH_ALEN])
{
  mlink_event_t event;
  event.id = ESPNOW_ADD_PEER;
  memcpy(event.info.add_peer.mac_addr, mac_addr, sizeof(event.info.add_peer.mac_addr));
  if (xQueueSend(transport_event_queue, &event, portMAX_DELAY) != pdTRUE)
  {
    ESP_LOGW(TAG, "Add peer queue fail");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t transport_send(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* packet, size_t len)
{
  mlink_event_t event;
  event.id = ESPNOW_SEND_REQUEST;
  memcpy(event.info.send_request.mac_addr, mac_addr, sizeof(event.info.send_request.mac_addr));
  event.info.send_request.data = malloc(len);
  if (!event.info.send_request.data)
  {
    ESP_LOGW(TAG, "Failed to allocate send request packet buffer");
    return ESP_FAIL;
  }
  event.info.send_request.data_len = len;
  memcpy(event.info.send_request.data, packet, len);
  if (xQueueSend(transport_event_queue, &event, portMAX_DELAY) != pdTRUE)
  {
    ESP_LOGW(TAG, "Send request queue fail");
    return ESP_FAIL;
  }
  return ESP_OK;
}

