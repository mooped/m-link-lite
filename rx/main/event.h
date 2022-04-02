/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef EVENT_H
#define EVENT_H

#include "esp_now.h"

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_STATION_MODE
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

#define EVENT_QUEUE_SIZE  6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, mlink_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
  ESPNOW_SEND_CB,
  ESPNOW_RECV_CB,
} mlink_event_id_t;

typedef struct {
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
  esp_now_send_status_t status;
} mlink_event_send_cb_t;

typedef struct {
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
  uint8_t* data;
  int data_len;
} mlink_event_recv_cb_t;

typedef union {
  mlink_event_send_cb_t send_cb;
  mlink_event_recv_cb_t recv_cb;
} mlink_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
  mlink_event_id_t id;
  mlink_event_info_t info;
} mlink_event_t;

enum {
  MLINK_DATA_BROADCAST,
  MLINK_DATA_UNICAST,
  MLINK_DATA_MAX,
};

/* User defined field of ESPNOW data in this example. */
typedef struct {
  uint8_t type;                         // Broadcast or unicast ESPNOW data.
  uint16_t crc;                         // CRC16 value of ESPNOW data.
  uint8_t payload_type;                 // Payload packet type
  uint8_t length;                       // Length of payload data
  uint8_t payload[0];                   // Real payload of ESPNOW data.
} __attribute__((packed)) mlink_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
  bool unicast;                         //Send unicast ESPNOW data.
  bool broadcast;                       //Send broadcast ESPNOW data.
  uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
  uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
  uint16_t count;                       //Total count of unicast ESPNOW data to be sent.
  uint16_t delay;                       //Delay between sending two ESPNOW data, unit: ms.
  int len;                              //Length of ESPNOW data to be sent, unit: byte.
  uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
  uint8_t dest_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
} mlink_send_param_t;

esp_err_t transport_init(void (*parse_cb)(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void*, size_t), void (*sent_cb)(void));

esp_err_t transport_send(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* packet, size_t len);

#endif
