#pragma once

typedef enum {
  MLINK_BEACON,
  MLINK_CONTROL,
  MLINK_TELEMETRY,
} mlink_packet_type_t;

typedef struct
{
  uint8_t type;                     // Broadcast or unicast, TX or RX
  uint16_t seq_num;                 // Sequence number of beacon
} __attribute__((packed)) mlink_beacon_t;

typedef struct
{
  uint16_t motors[3];               // Motor control data
  uint16_t servos[2];               // Servo control data
} __attribute__((packed)) mlink_control_t;

typedef struct
{
  uint16_t battery_voltage;         // RX battery voltage
} __attribute__((packed)) mlink_telemetry_t;
