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

#include "twi.h"
#include "ppm.h"
//#include "ppm_offload.h"
#include "motor.h"
#include "led.h"
#include "battery.h"
#include "rssi.h"
#include "oled.h"
#include "event.h"
#include "mlink.h"

#define ROLE_TX 0
#define ROLE_RX 1

#define ROLE ROLE_TX

uint16_t send_seq_num = 0;

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

#define RX_LED_NUM  1

static led_config_t rx_led_config[RX_LED_NUM] = {
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_2,
    .period = pdMS_TO_TICKS(2000),
    .duty = pdMS_TO_TICKS(1000),
    .state = 0,
  },
};

typedef enum
{
  RX_LED_WAITING,
  RX_LED_BIND,
  RX_LED_ACTIVE,
  RX_LED_FAILSAFE,
  RX_LED_INITIAL,
}
rx_led_state_t;

rx_led_state_t rx_led_state = RX_LED_INITIAL;

void rx_led_set_state(rx_led_state_t new_state)
{
  if (new_state != rx_led_state)
  {
    switch (new_state)
    {
      case RX_LED_WAITING:
      {
        led_set(0, 1, 1000, 2000);
      } break;
      case RX_LED_BIND:
      {
        led_set(0, 1, 180, 200);
      } break;
      case RX_LED_ACTIVE:
      {
        led_set(0, 1, 1000, 1000);
      } break;
      case RX_LED_FAILSAFE:
      {
        led_set(0, 1, 500, 1000);
      } break;
      default:
      {
        ESP_LOGW(TAG, "Invalid LED state requested.");
      }
    }
    rx_led_state = new_state;
  }
}

void parse_cb(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* data, size_t length)
{
  mlink_packet_t* packet = data;
  ESP_LOGD(TAG, "Got some data. Type: %d Length: %d", packet->type, length);
  rx_event_t event;

  rssi_recv(packet->seq_num);

  switch (packet->type)
  {
    case MLINK_BEACON:
    {
      if (packet->beacon.type & MLINK_BEACON_TX)
      {
        ESP_LOGD(TAG, "TX beacon received.");
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
        ESP_LOGD(TAG, "Non-TX beacon received - ignoring.");
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
  ESP_LOGD(TAG, "Packet Sent");
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
    ESP_LOGI(TAG, "Entered bind mode.");
    bind_mode = RX_BIND_BINDING;

    // Update LEDs to binding
    rx_led_set_state(RX_LED_BIND);
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
  // PPM pulses run from 5500 - 9500 clock ticks, so subtract 5500 and divide by 4
  // Clamp if out of range
  value = (value >= 5500) ? (value - 5500) : 0;
  value = (value < 4000) ? (value / 4) : 1000;

  // Check deadzone
  const int deadzone_width = 20;
  if (value > 500 - deadzone_width && value < 500 + deadzone_width)
  {
    // Center braking - if enabled
    if (brake_value > 6500)
    {
      return MOTOR_BRAKE;
    }
    // Otherwise coast in the deadzone
    else
    {
      return MOTOR_COAST;
    }
  }
  return ((int)value) - 500;
}

void rx_telemetry_task(void* args)
{
  ESP_LOGI(TAG, "Started telemetry task.");

  // Initialise battery voltage and RSSI measurement
  ESP_ERROR_CHECK( battery_init() );
  rssi_init(false);

  // Update telemetry once a second
  const TickType_t interval = pdMS_TO_TICKS(1000);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    // Report RSSI stats to console
    ESP_LOGD(TAG, "Sequence Num: %d Packets: %d Dropped Packets: %d Sequence Errors: %d", rssi_get_seq_num(), rssi_get_packet_count(), rssi_get_dropped(), rssi_get_out_of_sequence());

    // Create and send a telemetry packet
    if (xSemaphoreTake(rx_send_semaphore, pdMS_TO_TICKS(1000)))
    {
      ESP_LOGD(TAG, "Send telemetry packet.");
      mlink_packet_t packet;
      packet.type = MLINK_TELEMETRY;
      packet.seq_num = send_seq_num++;
      packet.telemetry.battery_voltage = battery_get_level();
      packet.telemetry.packet_count = rssi_get_packet_count();
      packet.telemetry.packet_loss = rssi_get_dropped();
      packet.telemetry.sequence_errors = rssi_get_out_of_sequence();
      transport_send(tx_unicast_mac, &packet, sizeof(packet));
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
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
          // Only respond to our bound TX and only outside of bind mode
          if (bind_mode != RX_BIND_BINDING && memcmp(event.controls.mac, tx_unicast_mac, ESP_NOW_ETH_ALEN) == 0)
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
            motor_set_all(
              motor_translate(controls->motors[0], controls->brakes[0]),
              motor_translate(controls->motors[1], controls->brakes[1]),
              motor_translate(controls->motors[2], controls->brakes[2])
            );

            // Update LEDs to active mode
            rx_led_set_state(RX_LED_ACTIVE);
          }
          else
          {
            ESP_LOGW(TAG, "Unexpected control packet from "MACSTR" rejected.", MAC2STR(event.controls.mac));
          }
        } break;
        // Received a beacon packet from the TX, complete handshake
        case RX_EVENT_BEACON_RECEIVED:
        {
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

              // Transmit RX beacon with bind flag
              if (xSemaphoreTake(rx_send_semaphore, pdMS_TO_TICKS(1000)))
              {
                ESP_LOGI(TAG, "Send RX | BIND beacon.");
                mlink_packet_t packet;
                packet.type = MLINK_BEACON;
                packet.seq_num = send_seq_num++;
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
            static uint16_t rx_beacon_counter = 0;
            // If we're not in bind mode, only send a response if we're bound to this TX
            if (event.beacon.type == MLINK_BEACON_TX && memcmp(tx_unicast_mac, event.beacon.mac, ESP_NOW_ETH_ALEN) == 0)
            {
              // Acknowledge TX beacon with an RX beacon
              if (rx_beacon_counter % 100 == 0)
              {
                if (xSemaphoreTake(rx_send_semaphore, 0))
                {
                  ESP_LOGD(TAG, "Send RX beacon.");
                  mlink_packet_t packet;
                  packet.type = MLINK_BEACON;
                  packet.seq_num = send_seq_num++;
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
              else
              {
                ESP_LOGW(TAG, "Not acknowledging TX beacon!");
              }

              ++rx_beacon_counter;
            }
          }

          // Transmit RX beacon so the TX can start sending unicast
          // TODO: Only do this if we're bound to this TX
          if (send_beacon && xSemaphoreTake(rx_send_semaphore, 0))
          {
            ESP_LOGD(TAG, "Send RX beacon.");
            mlink_packet_t packet;
            packet.type = MLINK_BEACON;
            packet.seq_num = send_seq_num++;
            packet.beacon.type = MLINK_BEACON_RX;
            transport_send(tx_unicast_mac, &packet, sizeof(packet));
          }
          else
          {
            ESP_LOGW(TAG, "No RX beacon packet sent.");
          }

          tx_beacon_received = true;
        } break;
        case RX_EVENT_FAILSAFE:
        {
          ESP_LOGW(TAG, "Failsafe triggered - setting motors to coast!");
          // Set motors to coast
          motor_set_all(0, 0, 0);
          // Update LEDs to failsafe
          rx_led_set_state(RX_LED_FAILSAFE);
        }
      }
    }
  }
}

void app_main()
{
  // Initialise PWM driver for motors, set motors off
  motor_init();
  motor_set_all(0, 0, 0);

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
  ESP_ERROR_CHECK( led_init(rx_led_config, RX_LED_NUM) );
  rx_led_set_state(RX_LED_WAITING);

  // TODO: Initialise servo driver

  // Create failsafe timer
  rx_failsafe_timer = xTimerCreate("rx-failsafe-timer", pdMS_TO_TICKS(500), pdTRUE, NULL, rx_failsafe_callback);

  // Initialise RX task
  xTaskCreate(rx_task, "rx-task", 2048, NULL, 10, NULL);

  // Initialise RX telemetry task
  xTaskCreate(rx_telemetry_task, "rx-telemetry-task", 2048, NULL, 7, NULL);
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
      uint16_t channels[PPM_NUM_CHANNELS];
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

#define TX_LED_NUM  3

static led_config_t tx_led_config[TX_LED_NUM] = {
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_2,
    .period = pdMS_TO_TICKS(4000),
    .duty = pdMS_TO_TICKS(500),
    .state = 0,
  },
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_5,
    .period = pdMS_TO_TICKS(2000),
    .duty = pdMS_TO_TICKS(1000),
    .state = 1,
  },
  {
    .timer = NULL,
    .gpio_num = GPIO_NUM_4,
    .period = pdMS_TO_TICKS(2000),
    .duty = pdMS_TO_TICKS(1000),
    .state = 0,
  },
};

typedef enum
{
  TX_LED_WAITING,
  TX_LED_RX_FOUND,
  TX_LED_BINDING,
  TX_LED_INITIAL,
}
tx_led_state_t;

tx_led_state_t tx_led_state = TX_LED_INITIAL;

void tx_led_set_state(tx_led_state_t new_state)
{
  if (new_state != tx_led_state)
  {
    switch (new_state)
    {
      case TX_LED_WAITING:
      {
        led_set(0, 1, 500, 4000);
        led_set(1, 1, 1000, 2000);
        led_set(2, 0, 1000, 2000);
      } break;
      case TX_LED_RX_FOUND:
      {
        led_set(1, 1, 1000, 1000);
        led_set(2, 1, 1000, 1000);
      } break;
      case TX_LED_BINDING:
      {
        led_set(1, 0, 20, 200);
        led_set(1, 0, 20, 200);
      } break;
      default:
      {
        ESP_LOGW(TAG, "Invalid LED state requested.");
      }
    }
    tx_led_state = new_state;
  }
}

mlink_telemetry_t tx_telemetry_buffer;
SemaphoreHandle_t tx_telemetry_mutex = NULL;
SemaphoreHandle_t tx_i2c_mutex = NULL;

static void parse_cb(uint8_t mac_addr[ESP_NOW_ETH_ALEN], void* data, size_t length)
{
  mlink_packet_t* packet = data;
  tx_event_t event;

  rssi_recv(packet->seq_num);

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
      ESP_LOGI(TAG, "Telemetry packet received.");
      if (xSemaphoreTake(tx_telemetry_mutex, 0) == pdTRUE)
      {
        // Copy telemetry data into the buffer
        memcpy(&tx_telemetry_buffer, &packet->telemetry, sizeof(mlink_telemetry_t));
        xSemaphoreGive(tx_telemetry_mutex);
      }
      else
      {
        ESP_LOGW(TAG, "Failed to acquire telemetry mutex.");
      }
    } break;
  }
}

static void sent_cb(void)
{
  ESP_LOGD(TAG, "Packet Sent");
  xSemaphoreGive(tx_send_semaphore);
}

static void ppm_debug(const uint16_t channels[PPM_NUM_CHANNELS])
{
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

bool ppm_offload_available = true;

typedef struct
{
  uint16_t channels[PPM_NUM_CHANNELS];
  uint16_t capture_time;
  uint16_t syncwidth;
} ppm_offload_t;

static void ppm_cb(const uint16_t channels[PPM_NUM_CHANNELS])
{
  tx_event_t event;
  event.type = TX_EVENT_CONTROL_UPDATE;

  // Try PPM offload until it fails
  if (ppm_offload_available)
  {
    if (xSemaphoreTake(tx_i2c_mutex, portMAX_DELAY))
    {
      ppm_offload_t packet = { 0 };
      // Receive PPM data from the offload helper into the event
      esp_err_t ret = twi_read_bytes(0x55, (void*)&packet, sizeof(packet));
      xSemaphoreGive(tx_i2c_mutex);
      if (ret == ESP_OK)
      {
        memcpy(event.controls.channels, packet.channels, sizeof(event.controls.channels));
      }
      else
      {
        ESP_LOGW(TAG, "No response from PPM offload, falling back to onboard PPM capture.");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        //ppm_offload_available = false;
      }
    }
  }

  // Fallback to onboard capture
  if (!ppm_offload_available)
  {
    // Copy the PPM data we captured into the event
    memcpy(event.controls.channels, channels, sizeof(event.controls.channels));
  }

  // Send the event
  if (xQueueSend(tx_control_queue, &event, portMAX_DELAY) != pdTRUE)
  {
    ESP_LOGW(TAG, "Control event queue fail.");
  }
}

const uint8_t glyph_battery[] = {
  0b00111100,
  0b01000010,
  0b01000010,
  0b01000010,
  0b01000010,
  0b01000010,
  0b01000010,
  0b00111100,
  0b00011000,
};

const uint8_t glyph_antenna[] = {
  0b01100000,
  0b00000000,
  0b01110000,
  0b00000000,
  0b01111000,
  0b00000000,
  0b01111100,
  0b00000000,
  0b01111110,
};

void tx_telemetry_task(void* args)
{
  ESP_LOGI(TAG, "Started telemetry task.");

  // Initialise battery voltage and RSSI measurement
  ESP_ERROR_CHECK( battery_init() );
  rssi_init(false);

  // Initialise OLED
  oled_init();

  // Update telemetry display once every 1500ms
  const TickType_t interval = pdMS_TO_TICKS(1500);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    mlink_telemetry_t telemetry_data;
    // Wait up to one interval for the data
    if (xSemaphoreTake(tx_telemetry_mutex, interval) == pdTRUE)
    {
      // Copy telemetry buffer
      memcpy(&telemetry_data, &tx_telemetry_buffer, sizeof(mlink_telemetry_t));
      xSemaphoreGive(tx_telemetry_mutex);

      // Report local telemetry stats
      ESP_LOGI(TAG, "TX Sequence Num: %d Packets: %d Dropped Packets: %d Sequence Errors: %d",
          rssi_get_seq_num(),
          rssi_get_packet_count(),
          rssi_get_dropped(),
          rssi_get_out_of_sequence()
      );
      // Report remote telemetry stats
      ESP_LOGI(TAG, "RX                  Packets: %d Dropped Packets: %d Sequence Errors: %d", 
          telemetry_data.packet_count,
          telemetry_data.packet_loss,
          telemetry_data.sequence_errors
      );
      ESP_LOGI(TAG, "RX Battery Voltage: %d", telemetry_data.battery_voltage);

      if (xSemaphoreTake(tx_i2c_mutex, portMAX_DELAY))
      {
        // Update OLED
        oled_at(16, 0);
        oled_print("RX:  ");
        oled_number_voltage(telemetry_data.battery_voltage);
        oled_print("v    ");
  
        oled_at(72, 0);
        oled_print("TX:  ");
        oled_number_voltage(battery_get_level());
        oled_print("v     ");
  
        oled_at(0, 0);
        oled_glyph(glyph_battery, sizeof(glyph_battery));
  
        oled_at(16, 1);
        oled_print("RX:  ");
        uint16_t sig = 0;
        if (telemetry_data.packet_count)
        {
          float fraction = 1.f - (float)(telemetry_data.packet_loss + telemetry_data.sequence_errors) / (float)(telemetry_data.packet_count);
          sig = (uint16_t)(fraction * 100.f);
        }
        oled_number_half(sig);
        oled_print("%     ");
  
        oled_at(72, 1);
        oled_print("TX:  ");
        sig = 0;
        if (telemetry_data.packet_count)
        {
          float fraction = 1.f - (float)(rssi_get_dropped() + rssi_get_out_of_sequence()) / (float)(rssi_get_packet_count());
          sig = (uint16_t)(fraction * 100.f);
        }
        oled_number_half(sig);
        oled_print("%     ");
        oled_at(0, 1);
        oled_glyph(glyph_antenna, sizeof(glyph_antenna));
  
        oled_at(0, 3);
        oled_print("RX MAC: ");
        oled_print_mac(rx_unicast_mac);

        xSemaphoreGive(tx_i2c_mutex);
      }
    }

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
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
          uint16_t* channels = event.controls.channels;
          /*
          static uint16_t average[PPM_NUM_CHANNELS] = { 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, };
          for (int c = 0; c < PPM_NUM_CHANNELS; ++c)
          {
            average[c] = (channels[c] + average[c]) / 2;
            channels[c] = average[c];
          }
          */
          // Set bind mode if requested
          bind_mode = channels[CHANNEL_BIND] >= 8000;
          // Update LED state if we're in bind mode
          if (bind_mode)
          {
            tx_led_set_state(TX_LED_BINDING);
          }
          // If we're not in bind mode and we have a unicast address for the rx, send a control packet
          if (!bind_mode && rx_beacon_received)
          {
            if (!IS_BROADCAST_ADDR(rx_unicast_mac) && xSemaphoreTake(tx_send_semaphore, 0))
            {
              ESP_LOGI(TAG, "Send control packet.");
              // Fill out and send control packet
              mlink_packet_t packet;
              packet.type = MLINK_CONTROL;
              packet.seq_num = send_seq_num++;
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
              ESP_LOGW(TAG, "No control packet sent.");
            }
          }
          // Otherwise send a TX beacon
          else if (!rx_beacon_received)
          {
            if (xSemaphoreTake(tx_send_semaphore, 0))
            {
              mlink_packet_t packet;
              packet.type = MLINK_BEACON;
              packet.seq_num = send_seq_num++;
              packet.beacon.type = MLINK_BEACON_TX;
              if (bind_mode)
              {
                ESP_LOGD(TAG, "Send TX | BIND beacon.");
                packet.beacon.type |= MLINK_BEACON_BIND;
              }
              else
              {
                ESP_LOGD(TAG, "Send TX beacon.");
              }
              transport_send(mlink_broadcast_mac, &packet, sizeof(packet));
            }
            else
            {
              ESP_LOGW(TAG, "No TX beacon sent.");
            }
          }
          // Output for debugging - use bind mode to trigger
          if (bind_mode)
          {
            ppm_debug(channels);
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
          // Update LED state
          tx_led_set_state(TX_LED_RX_FOUND);
        } break;
      }
    }
    // No PPM packet or RX beacon received in 500ms send a TX beacon
    else
    {
      if (xSemaphoreTake(tx_send_semaphore, 0))
      {
        ESP_LOGD(TAG, "Send TX beacon (no PPM or RX beacon).");
        mlink_packet_t packet;
        packet.type = MLINK_BEACON;
        packet.seq_num = send_seq_num++;
        packet.beacon.type = MLINK_BEACON_TX;
        transport_send(mlink_broadcast_mac, &packet, sizeof(packet));
      }
      else
      {
        //ESP_LOGW(TAG, "No TX beacon sent.");
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

  // Create objects for the telemetry task
  tx_telemetry_mutex = xSemaphoreCreateMutex();
  tx_i2c_mutex = xSemaphoreCreateMutex();

  // Initialise M-Link ESPNOW transport
  ESP_ERROR_CHECK( transport_init(&parse_cb, &sent_cb) );

  // Initialise LED driver
  ESP_ERROR_CHECK( led_init(tx_led_config, TX_LED_NUM) );
  tx_led_set_state(TX_LED_WAITING);

  // Initialise TX task
  xTaskCreate(tx_task, "tx-task", 2048, NULL, 9, NULL);

  // Initialise I2C driver for OLED and PPM offload
  ESP_ERROR_CHECK( twi_init() );

  // Initialise telemetry task
  xTaskCreate(tx_telemetry_task, "tx-telemetry-task", 2048, NULL, 7, NULL);

  // Initialise PPM decoder
  ESP_ERROR_CHECK( ppm_init(&ppm_cb) );
}
#endif
