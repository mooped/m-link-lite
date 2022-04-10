#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "rssi.h"

static const char* TAG = "m-link-rssi";

static uint16_t recv_started = 0;
static uint16_t recv_seq_first = 0;
static uint16_t recv_seq_num = 0;
static uint16_t dropped_packets = 0;
static uint16_t sequence_errors = 0;

static void rssi_task(void* pvParam)
{
  ESP_LOGI(TAG, "Started RSSI task.");

  // Report RSSI stats once a second
  const TickType_t interval = pdMS_TO_TICKS(1000);
  TickType_t previous_wake_time = xTaskGetTickCount();

  for (;;)
  {
    ESP_LOGI(TAG, "Sequence Num: %d Packets: %d Dropped Packets: %d Sequence Errors: %d", recv_seq_num, recv_seq_num - recv_seq_first, dropped_packets, sequence_errors);

    // Wait for the next interval
    vTaskDelayUntil(&previous_wake_time, interval);
  }
}

void rssi_init(int create_monitor_task)
{
  recv_started = 0;
  if (create_monitor_task)
  {
    xTaskCreate(rssi_task, "rssi-task", 1024, NULL, 5, NULL);
  }
}

void rssi_recv(uint16_t seq_num)
{
  if (recv_started)
  {
    if (seq_num > recv_seq_num + 1)
    {
      dropped_packets += (seq_num - recv_seq_num);
    }
    if (seq_num < recv_seq_num)
    {
      sequence_errors += 1;
    }
    recv_seq_num = seq_num;
  }
  else
  {
    recv_started = 1;
    recv_seq_num = recv_seq_first = seq_num;
  }
}

uint16_t rssi_get_seq_num(void)
{
  return recv_seq_num;
}

uint16_t rssi_get_packet_count(void)
{
  if (recv_started)
  {
    return recv_seq_num - recv_seq_first;
  }
  else
  {
    return 0;
  }
}

uint16_t rssi_get_dropped(void)
{
  return dropped_packets;
}
uint16_t rssi_get_out_of_sequence(void)
{
  return sequence_errors;
}

