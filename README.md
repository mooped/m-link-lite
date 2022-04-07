# M-Link ESPNOW

M-Link is a receiver and transmitter firmware for combat robotics based on the ESP8266 microcontroller and the ESPNOW 2.4Ghz protocol.

The M-Link hardware currently consists of a receiver and triple ESC PCB with two auxiliary servo outputs, and a transmitter module with a form factor to fit the Hitec Aurora 9 radio handset.

Channel assignments:

Channels 1, 2, and 3 control motor outputs 1, 2, and 3.
Channels 4, and 5 control the auxiliary servo outputs 4, and 5.
Channels 6, 7, and 8 set the respective motors into brake mode.
Channel 9 is used by the transmitter to enter bind mode.

The M-Link protocol follows the following flow:

Transmitter:

                             Start
                               |<-----------------------------------------
        ---------------------->|<--------------------------------         |
       |                       V                                 |        |
       |                  Bind Switch?               Broadcast TX Beacon  |
       |    Y <----------------|----------------> N              |        |
       |    |                                     |              |        |
       |    V                                     V              |        |
 Broadcast TX | BIND Beacon        Y <-- Received RX Beacon? --> N        |
                                   |                                      |
                                   V                                      |
                         Unicast Control Data ----------------------------

Receiver:

                              Start
                                |<-----------------------------------
    --------------------------->|<------------------------           |
   |                            V                         |          |
   |                 Unicast Packet Received?             |          |
   |         N <----------------|-----------------> Y     |          |
   |         |                                      |     |          |
   |         V                                      V     |          |
   |    10s Elapsed?                     Process Control Inputs      |
   N <-------|-------> Y                                             |
         ------------->|                                             |
        |              |                                             |
        |              V                                             |
        |     TX Beacon Received?                                    |
        N <------------|-----------> Y                               |
                                     |                               |
                                     V                               |
                               Store TX MAC                          |
                                     |                               |
                                     V                               |
                               Send RX Beacon                        |
                                     |                               |
                                     --------------------------------

RX MAC: 30:83:98:9a:bf:79
TX MAC: a0:20:a6:17:b2:e4
