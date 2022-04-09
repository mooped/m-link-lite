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
#include "rx-led.h"
#include "event.h"
#include "mlink.h"

#define ROLE_TX 0
#define ROLE_RX 1

#define ROLE ROLE_RX

#if ROLE == ROLE_RX
static const char *TAG = "m-link-rx-main";

nvs_handle_t rx_nvs_handle;

xQueueHandle rx_control_queue = NULL;
SemaphoreHandle_t rx_send_semaphore = NULL;

xTimerHandle rx_failsafe_timer = NULL;

xTimerHandle rx_bind_timer = NULL;

typedef enum
{
  RX_BIND_WAITING,
  RX_BIND_BINDING,
  RX_BIND_CANCELLED,
  RX_BIND_BOUND,
}
rx_bind_mode_t;

rx_bind_mode_t bind_mode = RX_BIND_WAITING;

typedef enum
{
  RX_EVENT_CONTROL_UPDATE,
  RX_EVENT_BEACON_RECEIVED,
  RX_EVENT_FAILSAFE,
}
rx_event_type_t;

typedef struct
{
  rx_event_type_t type;
  union
  {
    struct
    {
      mlink_control_t controls;
      uint8_t mac[ESP_NOW_ETH_ALEN];
    } controls;
    struct
    {
      uint8_t type;
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
        event.beacon.type = packet->beacon.type;
        memcpy(event.beacon.mac, mac_addr, ESP_NOW_ETH_ALEN);
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
      memcpy(&event.controls.controls, &(packet->control), sizeof(mlink_control_t));
      memcpy(event.controls.mac, mac_addr, ESP_NOW_ETH_ALEN);
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

void rx_failsafe_callback(xTimerHandle xTimer)
{
  rx_event_t event;
  event.type = RX_EVENT_FAILSAFE;
  if (xQueueSend(rx_control_queue, &event, portMAX_DELAY) != pdTRUE)
  {
    ESP_LOGW(TAG, "Control event queue fail.");
  }
}

void rx_bind_timer_callback(xTimerHandle xTimer)
{
  ESP_LOGI(TAG, "Bind timer elapsed.");

  if (bind_mode == RX_BIND_WAITING)
  {
    ESP_LOGW(TAG, "Entered bind mode.");
    bind_mode = RX_BIND_BINDING;

    // Update LEDs to binding
    rx_led_set(0, 20, 100);
  }
  else
  {
    ESP_LOGW(TAG, "Bind timer elapsed, but should have been cancelled by an incoming beacon.");
  }
}

esp_err_t rx_bind_begin_wait(void)
{
  // Start the timer to enter bind mode if our TX doesn't respond in 10 seconds
  bind_mode = RX_BIND_WAITING;
  rx_bind_timer = xTimerCreate("rx-bind-timer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, &rx_bind_timer_callback);
  if (!rx_bind_timer)
  {
    ESP_LOGW(TAG, "Failed to create bind timer.");
    return ESP_FAIL;
  }
  if (xTimerStart(rx_bind_timer, 0) != pdPASS)
  {
    ESP_LOGW(TAG, "Failed to start bind timer.");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t rx_bind_cancel(void)
{
  BaseType_t ret = xTimerStop(rx_bind_timer, 0);
  bind_mode = RX_BIND_CANCELLED;
  if (ret != pdPASS)
  {
    ESP_LOGW(TAG, "Failed to stop bind timer.");
    return ESP_FAIL;
  }
  return ESP_OK;
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

  ESP_ERROR_CHECK( rx_bind_begin_wait() );

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
          // Only respond to our bound TX
          if (memcmp(event.controls.mac, tx_unicast_mac, ESP_NOW_ETH_ALEN) == 0)
          {
            const mlink_control_t* controls = &event.controls.controls;
            ESP_LOGI(TAG, "Control packet received %d %d %d %d %d -> %d %d %d",
              controls->motors[0],
              controls->motors[1],
              controls->motors[2],
              controls->servos[0],
              controls->servos[1],
              motor_translate(controls->motors[0], controls->brakes[0]),
              motor_translate(controls->motors[1], controls->brakes[1]),
              motor_translate(controls->motors[2], controls->brakes[2])
            );
  
            // Reset failsafe timer
            xTimerReset(rx_failsafe_timer, 0);
  
            // Update motors
            rx_pwm_set_motors(
              motor_translate(controls->motors[0], controls->brakes[0]),
              motor_translate(controls->motors[1], controls->brakes[1]),
              motor_translate(controls->motors[2], controls->brakes[2])
            );
            rx_pwm_update();

            // Update LEDs to active mode
            rx_led_set(0, 100, 200);
          }
          else
          {
            ESP_LOGW(TAG, "Unexpected control packet from "MACSTR" rejected.", MAC2STR(event.controls.mac));
          }
        } break;
        // Received a beacon packet from the TX, complete handshake
        case RX_EVENT_BEACON_RECEIVED:
        {
          // Turn on the LED
          //rx_pwm_set_led(1);
          rx_pwm_update();

          int send_beacon = 0;

          if (bind_mode == RX_BIND_BINDING)
          {
            // If in bind mode, and the TX beacon has the bind flag, then bind to this TX, then exit bind mode
            if (event.beacon.type == (MLINK_BEACON_TX | MLINK_BEACON_BIND))
            {
              ESP_LOGW(TAG, "Binding to "MACSTR".", MAC2STR(event.beacon.mac));

              // Record that we're bound
              bind_mode = RX_BIND_BOUND;

              // Store TX MAC address
              memcpy(tx_unicast_mac, event.beacon.mac, ESP_NOW_ETH_ALEN);
              transport_add_peer(tx_unicast_mac);

              // Write TX MAC address to flash
              ESP_LOGI(TAG, "Writing "MACSTR" to NVS...", MAC2STR(tx_unicast_mac));
              ESP_ERROR_CHECK( nvs_set_blob(rx_nvs_handle, "tx_mac", tx_unicast_mac, ESP_NOW_ETH_ALEN) );
              ESP_ERROR_CHECK( nvs_commit(rx_nvs_handle) );
              ESP_LOGI(TAG, "Written "MACSTR" to NVS.", MAC2STR(tx_unicast_mac));

              // Transmit RX beacon with bind flag
              if (xSemaphoreTake(rx_send_semaphore, pdMS_TO_TICKS(1000)))
              {
                ESP_LOGI(TAG, "Send RX | BIND beacon.");
                mlink_packet_t packet;
                packet.type = MLINK_BEACON;
                packet.beacon.type = MLINK_BEACON_RX | MLINK_BEACON_BIND;
                transport_send(tx_unicast_mac, &packet, sizeof(packet));
              }
              else
              {
                ESP_LOGW(TAG, "Failed to acknowledge binding!");
              }
            }
            else
            {
              ESP_LOGW(TAG, "Received non-bind mode beacon while in bind mode.");
            }
          }
          else
          {
            // If we're not in bind mode, only send a response if we're bound to this TX
            if (event.beacon.type == MLINK_BEACON_TX && memcmp(tx_unicast_mac, event.beacon.mac, ESP_NOW_ETH_ALEN) == 0)
            {
              // Acknowledge TX beacon with an RX beacon
              if (xSemaphoreTake(rx_send_semaphore, 0))
              {
                ESP_LOGI(TAG, "Send RX beacon.");
                mlink_packet_t packet;
                packet.type = MLINK_BEACON;
                packet.beacon.type = MLINK_BEACON_RX;
                transport_send(tx_unicast_mac, &packet, sizeof(packet));

                // Cancel bind timer
                rx_bind_cancel();
              }
              else
              {
                ESP_LOGW(TAG, "Failed to acknowledge TX beacon!");
              }
            }
          }

          // Transmit RX beacon so the TX can start sending unicast
          // TODO: Only do this if we're bound to this TX
          if (send_beacon && xSemaphoreTake(rx_send_semaphore, 0))
          {
            ESP_LOGI(TAG, "Send RX beacon.");
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
        case RX_EVENT_FAILSAFE:
        {
          ESP_LOGW(TAG, "Failsafe triggered - setting motors to coast!");
          // Set motors to coast
          rx_pwm_set_motors(0, 0, 0);
          rx_pwm_update();
          // Update LEDs to failsafe
          rx_led_set(0, 1000, 2000);
        }
      }
    }
  }
}

void app_main()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW(TAG, "Failed to initialise NVS, erasing to reinitialise.");
    // If we can't initialise NVS, erase and recreate the partition
    ESP_ERROR_CHECK( nvs_flash_erase() );
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Extract bound MAC address from NVS
  size_t length = 6;
  ESP_ERROR_CHECK( nvs_open("nvs", NVS_READWRITE, &rx_nvs_handle) );
  err = nvs_get_blob(rx_nvs_handle, "tx_mac", tx_unicast_mac, &length);
  if (err == ESP_ERR_NVS_NOT_FOUND)
  {
    // Write the broadcast address if tx_mac key is not present
    ESP_LOGW(TAG, "Initialised tx_mac to "MACSTR"", MAC2STR(tx_unicast_mac));
    ESP_ERROR_CHECK( nvs_set_blob(rx_nvs_handle, "tx_mac", tx_unicast_mac, length) );
    ESP_ERROR_CHECK( nvs_commit(rx_nvs_handle) );
  }
  else
  {
    ESP_ERROR_CHECK(err);
  }
  ESP_LOGI(TAG, "Bound TX MAC is "MACSTR"", MAC2STR(tx_unicast_mac));

  // Create objects for the control task
  rx_control_queue = xQueueCreate(10, sizeof(rx_event_t));
  rx_send_semaphore = xSemaphoreCreateMutex();

  // Initialise M-Link ESPNOW transport
  ESP_ERROR_CHECK( transport_init(&parse_cb, &sent_cb) );

  // Add the bound MAC address as a peer
  transport_add_peer(tx_unicast_mac);

  // Set LED to boot state (2 second flash)
  ESP_ERROR_CHECK( rx_led_init() );

  // Initialise PWM driver for motors, set motors off
  rx_pwm_init();
  rx_pwm_set_motors(0, 0, 0);
  rx_pwm_update();
  // Initialise LED 
  // TODO: Initialise servo driver

  // Create failsafe timer
  rx_failsafe_timer = xTimerCreate("rx-failsafe-timer", pdMS_TO_TICKS(500), pdTRUE, NULL, rx_failsafe_callback);

  // Initialise RX task
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
      uint8_t type;
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
        event.beacon.type = packet->beacon.type;
        memcpy(event.beacon.mac, mac_addr, ESP_NOW_ETH_ALEN);
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
          // If we're not in bind mode and we have a unicast address for the rx, send a control packet
          if (!bind_mode && rx_beacon_received)
          {
            if (!IS_BROADCAST_ADDR(rx_unicast_mac) && xSemaphoreTake(tx_send_semaphore, 0))
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
              mlink_packet_t packet;
              packet.type = MLINK_BEACON;
              packet.beacon.type = MLINK_BEACON_TX;
              if (bind_mode)
              {
                ESP_LOGI(TAG, "Send TX | BIND beacon.");
                packet.beacon.type |= MLINK_BEACON_BIND;
              }
              else
              {
                ESP_LOGI(TAG, "Send TX beacon.");
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
          // Add the RX as a peer if we've not completed the handshake
          if (!rx_beacon_received)
          {
            ESP_LOGW(TAG, "Handshake complete with RX "MACSTR"", MAC2STR(event.beacon.mac));
            rx_beacon_received = true;
            memcpy(rx_unicast_mac, event.beacon.mac, ESP_NOW_ETH_ALEN);
            transport_add_peer(rx_unicast_mac);
          }
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

  // Create objects for the control task
  tx_control_queue = xQueueCreate(10, sizeof(tx_event_t));
  tx_send_semaphore = xSemaphoreCreateMutex();

  // Initialise M-Link ESPNOW transport
  ESP_ERROR_CHECK( transport_init(&parse_cb, &sent_cb) );

  // Initialise PWM LED driver
  tx_pwm_init();

  // Initialise TX task
  xTaskCreate(tx_task, "tx-task", 2048, NULL, 10, NULL);

  // Initialise PPM decoder
  ESP_ERROR_CHECK( tx_ppm_init(&ppm_cb) );
}
#endif
