/* pwm example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "tx-ppm.h"
#include "tx-pwm.h"
#include "rx-pwm.h"
#include "event.h"
#include "mlink.h"

#define ROLE_TX 0
#define ROLE_RX 1

#define ROLE ROLE_RX

#if ROLE == ROLE_RX
static const char *TAG = "m-link-rx-main";

xQueueHandle rx_control_queue = NULL;
SemaphoreHandle_t rx_send_semaphore = NULL;

typedef enum
{
  RX_EVENT_CONTROL_UPDATE,
  RX_EVENT_BEACON_RECEIVED,
}
rx_event_type_t;

typedef struct
{
  rx_event_type_t type;
  union
  {
    mlink_control_t controls;
    struct
    {
      uint8_t mac[ESP_NOW_ETH_ALEN];
    } beacon;
  };
}
rx_event_t;

static uint8_t tx_unicast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t tx_beacon_received = 0;

void parse_cb(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* data, size_t length)
{
  mlink_packet_t* packet = data;
  ESP_LOGI(TAG, "Got some data. Type: %d Length: %d", packet->type, length);
  rx_event_t event;
  if (!rx_control_queue)
  {
    ESP_LOGW(TAG, "Data received before control queue creation - ignoring.");
    return;
  }
  switch (packet->type)
  {
    case MLINK_BEACON:
    {
      if (packet->beacon.type & MLINK_BEACON_TX)
      {
        ESP_LOGI(TAG, "TX beacon received.");
        // Send an event to the RX task with the beacon MAC address
        event.type = RX_EVENT_BEACON_RECEIVED;
        memcpy(event.beacon.mac, mac_addr, sizeof(tx_unicast_mac));
        if (xQueueSend(rx_control_queue, &event, portMAX_DELAY) != pdTRUE)
        {
          ESP_LOGW(TAG, "Control event queue fail.");
        }
      }
      else
      {
        ESP_LOGI(TAG, "Non-TX beacon received - ignoring.");
      }
    } break;
    case MLINK_CONTROL:
    {
      // Send an event to the RX task with the control data
      event.type = RX_EVENT_CONTROL_UPDATE;
      memcpy(&event.controls, &(packet->control), sizeof(mlink_control_t));
      if (xQueueSend(rx_control_queue, &event, portMAX_DELAY) != pdTRUE)
      {
        ESP_LOGW(TAG, "Control event queue fail.");
      }
    } break;
    case MLINK_TELEMETRY:
    {
      ESP_LOGW(TAG, "Telemetry packet not expected by RX!");
    } break;
  }
}

void sent_cb(void)
{
  ESP_LOGI(TAG, "Packet Sent");
  xSemaphoreGive(rx_send_semaphore);
}

int motor_translate(uint16_t value, uint16_t brake_value)
{
  if (brake_value < 6000)
  {
    value = (value > 3500) ? (value - 3500) : 0;
    value = (value < 4096) ? (value / 4) : 1024;
    return ((int)value) - 512;
  }
  else
  {
    return 512;
  }
}

void rx_task(void* args)
{
  static rx_event_t event;

  for (;;)
  {
    // Wait for events, waking up every 500ms if nothing has happened
    if (xQueueReceive(rx_control_queue, &event, 500 / portTICK_PERIOD_MS))
    {
      switch (event.type)
      {
        // Received a control packet
        case RX_EVENT_CONTROL_UPDATE:
        {
          ESP_LOGI(TAG, "Control packet received %d %d %d %d %d -> %d %d %d",
            event.controls.motors[0],
            event.controls.motors[1],
            event.controls.motors[2],
            event.controls.servos[0],
            event.controls.servos[1],
            motor_translate(event.controls.motors[0], event.controls.brakes[0]),
            motor_translate(event.controls.motors[1], event.controls.brakes[1]),
            motor_translate(event.controls.motors[2], event.controls.brakes[2])
          );
          rx_pwm_set_motors(
            motor_translate(event.controls.motors[0], event.controls.brakes[0]),
            motor_translate(event.controls.motors[1], event.controls.brakes[1]),
            motor_translate(event.controls.motors[2], event.controls.brakes[2])
          );
          rx_pwm_update();
        } break;
        // Received a beacon packet from the TX, complete handshake
        case RX_EVENT_BEACON_RECEIVED:
        {
          // Turn on the LED
          rx_pwm_set_led(1);

          // Transmit RX beacon so the TX can start sending unicast
          // TODO: Only do this if we're bound to this TX
          if (/*!tx_beacon_received && */xSemaphoreTake(rx_send_semaphore, 0))
          {
            ESP_LOGI(TAG, "Send RX beacon.");
            memcpy(tx_unicast_mac, event.beacon.mac, sizeof(tx_unicast_mac));
            transport_add_peer(tx_unicast_mac);
            mlink_packet_t packet;
            packet.type = MLINK_BEACON;
            packet.beacon.type = MLINK_BEACON_RX;
            transport_send(tx_unicast_mac, &packet, sizeof(packet));
          }
          else
          {
            ESP_LOGI(TAG, "No RX beacon packet sent.");
          }

          tx_beacon_received = true;
        } break;
      }
    }
  }
}

void app_main()
{
  // Initialize NVS
  ESP_ERROR_CHECK( nvs_flash_init() );

  // Initialise M-Link ESPNOW transport
  ESP_ERROR_CHECK( transport_init(&parse_cb, &sent_cb) );

  // Initialise PWM driver for motors, servos, and LED
  rx_pwm_init();
  // Motors off, LED off
  rx_pwm_set_motors(0, 0, 0);
  rx_pwm_set_led(0);
  rx_pwm_update();

  // Initialise RX task
  rx_control_queue = xQueueCreate(10, sizeof(rx_event_t));
  rx_send_semaphore = xSemaphoreCreateMutex();
  xTaskCreate(rx_task, "rx-task", 2048, NULL, 10, NULL);
}
#elif ROLE == ROLE_TX
static const char *TAG = "m-link-tx-main";

xQueueHandle tx_control_queue = NULL;
SemaphoreHandle_t tx_send_semaphore = NULL;

typedef enum
{
  TX_EVENT_CONTROL_UPDATE,
  TX_EVENT_BEACON_RECEIVED,
}
tx_event_type_t;

typedef struct
{
  tx_event_type_t type;
  union
  {
    struct
    {
      uint32_t channels[PPM_NUM_CHANNELS];
    } controls;
    struct
    {
      uint8_t mac[ESP_NOW_ETH_ALEN];
    } beacon;
  };
}
tx_event_t;

#define CHANNEL_M1      0
#define CHANNEL_M2      1
#define CHANNEL_M3      2
#define CHANNEL_AUX1    3
#define CHANNEL_AUX2    4
#define CHANNEL_BRAKE1  5
#define CHANNEL_BRAKE2  6
#define CHANNEL_BRAKE3  7
#define CHANNEL_BIND    8

static uint8_t rx_unicast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t rx_beacon_received = 0;
static uint8_t bind_mode = 0;

static void parse_cb(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* data, size_t length)
{
  mlink_packet_t* packet = data;
  tx_event_t event;
  switch (packet->type)
  {
    case MLINK_BEACON:
    {
      if (packet->beacon.type & MLINK_BEACON_RX)
      {
        ESP_LOGI(TAG, "RX beacon received.");
        // Send an event to inform the TX control task
        event.type = TX_EVENT_BEACON_RECEIVED;
        memcpy(event.beacon.mac, mac_addr, sizeof(rx_unicast_mac));
        if (xQueueSend(tx_control_queue, &event, portMAX_DELAY) != pdTRUE)
        {
          ESP_LOGW(TAG, "Control event queue fail.");
        }
      }
      else
      {
        ESP_LOGI(TAG, "Non-RX beacon received - ignoring.");
      }
    } break;
    case MLINK_CONTROL:
    {
      ESP_LOGW(TAG, "Control packet not expected by TX!");
    } break;
    case MLINK_TELEMETRY:
    {
      ESP_LOGI(TAG, "Telemetry packet received - not yet implemented");
    } break;
  }
}

static void sent_cb(void)
{
  ESP_LOGI(TAG, "Packet Sent");
  xSemaphoreGive(tx_send_semaphore);
}

static void ppm_cb(uint32_t channels[PPM_NUM_CHANNELS])
{
  static uint32_t count = 0;

  // Log every 100 frames for debugging
  if ((count++ % 100) == 0) {
    ESP_LOGI(TAG, "Channels %d %d %d %d %d %d %d %d %d",
      channels[0], 
      channels[1], 
      channels[2], 
      channels[3], 
      channels[4], 
      channels[5], 
      channels[6], 
      channels[7], 
      channels[8]
    );
  }

  tx_event_t event;
  event.type = TX_EVENT_CONTROL_UPDATE;
  memcpy(event.controls.channels, channels, sizeof(event.controls.channels));
  if (xQueueSend(tx_control_queue, &event, portMAX_DELAY) != pdTRUE)
  {
    ESP_LOGW(TAG, "Control event queue fail.");
  }
}

void tx_task(void* args)
{
  static tx_event_t event;

  for (;;)
  {
    // Wait for events, waking up every 500ms if nothing has happened
    if (xQueueReceive(tx_control_queue, &event, 500 / portTICK_PERIOD_MS))
    {
      switch (event.type)
      {
        // Received a PPM frame
        case TX_EVENT_CONTROL_UPDATE:
        {
          // Extract the channel data
          const uint32_t* channels = event.controls.channels;
          // Set bind mode if requested
          bind_mode = channels[CHANNEL_BIND] >= 6000;
          // If we're not in bind mode and the RX handshake is complete, send a control packet
          if (!bind_mode && rx_beacon_received)
          {
            if (xSemaphoreTake(tx_send_semaphore, 0))
            {
              ESP_LOGI(TAG, "Send control packet.");
              // Fill out and send control packet
              mlink_packet_t packet;
              packet.type = MLINK_CONTROL;
              packet.control.motors[0] = channels[CHANNEL_M1];
              packet.control.motors[1] = channels[CHANNEL_M2];
              packet.control.motors[2] = channels[CHANNEL_M3];
              packet.control.brakes[0] = channels[CHANNEL_BRAKE1];
              packet.control.brakes[1] = channels[CHANNEL_BRAKE2];
              packet.control.brakes[2] = channels[CHANNEL_BRAKE3];
              packet.control.servos[0] = channels[CHANNEL_AUX1];
              packet.control.servos[1] = channels[CHANNEL_AUX2];
              transport_send(rx_unicast_mac, &packet, sizeof(packet));
            }
            else
            {
              ESP_LOGI(TAG, "No control packet sent.");
            }
          }
          // Otherwise send a TX beacon
          else if (!rx_beacon_received)
          {
            if (xSemaphoreTake(tx_send_semaphore, 0))
            {
              ESP_LOGI(TAG, "Send TX beacon.");
              mlink_packet_t packet;
              packet.type = MLINK_BEACON;
              packet.beacon.type = MLINK_BEACON_TX;
              if (bind_mode)
              {
                packet.beacon.type |= MLINK_BEACON_BIND;
              }
              transport_send(mlink_broadcast_mac, &packet, sizeof(packet));
            }
            else
            {
              ESP_LOGI(TAG, "No TX beacon sent.");
            }
          }
        } break;
        // Received a beacon from the RX, complete handshake
        case TX_EVENT_BEACON_RECEIVED:
        {
          // Add the RX as a peer
          rx_beacon_received = true;
          memcpy(rx_unicast_mac, event.beacon.mac, sizeof(rx_unicast_mac));
          transport_add_peer(rx_unicast_mac);
        } break;
      }
    }
    // No PPM packet or RX beacon received in 500ms send a TX beacon
    else if (!rx_beacon_received)
    {
      if (xSemaphoreTake(tx_send_semaphore, 0))
      {
        ESP_LOGI(TAG, "Send TX beacon (no PPM or RX beacon).");
        mlink_packet_t packet;
        packet.type = MLINK_BEACON;
        packet.beacon.type = MLINK_BEACON_TX;
        transport_send(mlink_broadcast_mac, &packet, sizeof(packet));
      }
      else
      {
        ESP_LOGI(TAG, "No control packet sent.");
      }
    }
  }
}

void app_main()
{
  // Initialize NVS
  ESP_ERROR_CHECK( nvs_flash_init() );

  // Initialise M-Link ESPNOW transport
  ESP_ERROR_CHECK( transport_init(&parse_cb, &sent_cb) );

  // Initialise PWM LED driver
  tx_pwm_init();

  // Initialise TX task
  tx_control_queue = xQueueCreate(10, sizeof(tx_event_t));
  tx_send_semaphore = xSemaphoreCreateMutex();
  xTaskCreate(tx_task, "tx-task", 2048, NULL, 10, NULL);

  // Initialise PPM decoder
  ESP_ERROR_CHECK( tx_ppm_init(&ppm_cb) );
}
#endif
