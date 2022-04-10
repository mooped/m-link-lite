#pragma once

void rssi_init(int create_monitor_task);

void rssi_recv(uint16_t seq_num);

uint16_t rssi_get_seq_num(void);
uint16_t rssi_get_packet_count(void);
uint16_t rssi_get_dropped(void);
uint16_t rssi_get_out_of_sequence(void);
