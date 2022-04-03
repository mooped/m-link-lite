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

#define ROLE ROLE_TX

#if ROLE == ROLE_RX
static const char *TAG = "m-link-rx-main";

xQueueHandle rx_control_queue = NULL;
SemaphoreHandle_t rx_send_semaphore = NULL;

typedef enum
{
  RX_EVENT_CONTROL_UPDATE,
  RX_EVENT_BEACON_RECEIVED,
}
rx_event_t;

static uint8_t tx_unicast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t tx_beacon_received = 0;
static mlink_control_t rx_controls;

void parse_cb(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* data, size_t length)
{
  mlink_packet_t* packet = data;
  ESP_LOGI(TAG, "Got some data. Type: %d Length: %d", packet->type, length);
  rx_event_t event;
  switch (packet->type)
  {
    case MLINK_BEACON:
    {
      if (packet->beacon.type & MLINK_BEACON_TX)
      {
        ESP_LOGI(TAG, "TX beacon received.");
        // Copy TX mac address
        memcpy(tx_unicast_mac, mac_addr, sizeof(tx_unicast_mac));
        // Send an event to inform the TX control task
        event = RX_EVENT_BEACON_RECEIVED;
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
      memcpy(&rx_controls, &(packet->control), sizeof(mlink_control_t));
      event = RX_EVENT_CONTROL_UPDATE;
      if (xQueueSend(rx_control_queue, &event, portMAX_DELAY) != pdTRUE)
      {
        ESP_LOGW(TAG, "Control event queue fail.");
      }
    } break;
    case MLINK_TELEMETRY:
    {
      ESP_LOGI(TAG, "Telemetry packet received - ignoring");
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
      switch (event)
      {
        // Received a control packet
        case RX_EVENT_CONTROL_UPDATE:
        {
          ESP_LOGI(TAG, "Control packet received %d %d %d %d %d -> %d %d %d",
            rx_controls.motors[0],
            rx_controls.motors[1],
            rx_controls.motors[2],
            rx_controls.servos[0],
            rx_controls.servos[1],
            motor_translate(rx_controls.motors[0], rx_controls.brakes[0]),
            motor_translate(rx_controls.motors[1], rx_controls.brakes[1]),
            motor_translate(rx_controls.motors[2], rx_controls.brakes[2])
          );
          rx_pwm_set_motors(
            motor_translate(rx_controls.motors[0], rx_controls.brakes[0]),
            motor_translate(rx_controls.motors[1], rx_controls.brakes[1]),
            motor_translate(rx_controls.motors[2], rx_controls.brakes[2])
          );
          rx_pwm_update();
        } break;
        // Received a beacon packet from the TX, complete handshake
        case RX_EVENT_BEACON_RECEIVED:
        {
          rx_pwm_set_led(1);
          tx_beacon_received = true;
        } break;
      }
    }
    /*
    // No control packet or TX beacon received in 500ms send a RX beacon
    else
    {
      if (xSemaphoreTake(rx_send_semaphore, 0))
      {
        ESP_LOGI(TAG, "Send RX beacon (no TX beacon received).");
        mlink_packet_t packet;
        packet.type = MLINK_BEACON;
        packet.beacon.type = MLINK_BEACON_RX;
        transport_send(mlink_broadcast_mac, &packet, sizeof(packet));
      }
      else
      {
        ESP_LOGI(TAG, "No control packet sent.");
      }
    }
    */
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

static uint32_t controls[PPM_NUM_CHANNELS] = { 0 };
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
        // Copy RX mac address
        memcpy(rx_unicast_mac, mac_addr, sizeof(rx_unicast_mac));
        // Send an event to inform the TX control task
        event = TX_EVENT_BEACON_RECEIVED;
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
      ESP_LOGI(TAG, "Control packet received - ignoring");
    } break;
    case MLINK_TELEMETRY:
    {
      ESP_LOGI(TAG, "Telemetry packet received - ignoring");
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

  tx_event_t event = TX_EVENT_CONTROL_UPDATE;
  memcpy(controls, channels, sizeof(controls));
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
      switch (event)
      {
        // Received a PPM frame
        case TX_EVENT_CONTROL_UPDATE:
        {
          // If we're not in bind mode and the RX handshake is complete, send a control packet
          if (!bind_mode)// && rx_beacon_received)
          {
            if (xSemaphoreTake(tx_send_semaphore, 0))
            {
              ESP_LOGI(TAG, "Send control packet.");
              mlink_packet_t packet;
              packet.type = MLINK_CONTROL;
              packet.control.motors[0] = controls[CHANNEL_M1];
              packet.control.motors[1] = controls[CHANNEL_M2];
              packet.control.motors[2] = controls[CHANNEL_M3];
              packet.control.brakes[0] = controls[CHANNEL_BRAKE1];
              packet.control.brakes[1] = controls[CHANNEL_BRAKE2];
              packet.control.brakes[2] = controls[CHANNEL_BRAKE3];
              packet.control.servos[0] = controls[CHANNEL_AUX1];
              packet.control.servos[1] = controls[CHANNEL_AUX2];
              transport_send(rx_unicast_mac, &packet, sizeof(packet));
            }
            else
            {
              ESP_LOGI(TAG, "No control packet sent.");
            }
          }
          // Otherwise send a TX beacon
          else
          {
            if (xSemaphoreTake(tx_send_semaphore, 0))
            {
              ESP_LOGI(TAG, "Send TX beacon.");
              mlink_packet_t packet;
              packet.type = MLINK_BEACON;
              packet.beacon.type = MLINK_BEACON_TX;
              if (controls[CHANNEL_BIND] >= 6000)
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
          rx_beacon_received = true;
        } break;
      }
    }
    // No PPM packet or RX beacon received in 500ms send a TX beacon
    else
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
