#pragma once

typedef enum {
  MLINK_BEACON,
  MLINK_CONTROL,
  MLINK_TELEMETRY,
} mlink_packet_type_t;

typedef enum {
  MLINK_BEACON_TX =   0x01,
  MLINK_BEACON_RX =   0x02,
  MLINK_BEACON_BIND = 0x80,
} mlink_beacon_type_t;

typedef struct
{
  uint8_t type;                     // Beacon type
} __attribute__((packed)) mlink_beacon_t;

typedef struct
{
  uint16_t motors[3];               // Motor control data
  uint16_t brakes[3];                // Motor brake data
  uint16_t servos[2];               // Servo control data
} __attribute__((packed)) mlink_control_t;

typedef struct
{
  uint16_t battery_voltage;         // RX battery voltage
} __attribute__((packed)) mlink_telemetry_t;

typedef struct
{
  mlink_packet_type_t type;         // Packet type
  uint16_t seq_num;                 // Sequence number
  union
  {
    mlink_beacon_t    beacon;
    mlink_control_t   control;
    mlink_telemetry_t telemetry;
  };
} __attribute__((packed)) mlink_packet_t;
