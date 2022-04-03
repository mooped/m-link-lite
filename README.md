# M-Link ESPNOW

M-Link is a receiver and transmitter firmware for combat robotics based on the ESP8266 microcontroller and the ESPNOW 2.4Ghz protocol.

The M-Link hardware currently consists of a receiver and triple ESC PCB with two auxiliary servo outputs, and a transmitter module with a form factor to fit the Hitec Aurora 9 radio handset.

The M-Link protocol follows the following flow:

Transmitter:

                             Start
                               |<----------------------------------------
        ---------------------->|<--------------------------------        |
       |                       V                                 |       |
       |                  Bind Switch?                           |       |
       |    Y <----------------|----------------> N              |       |
       |    |                                     |              |       |
       |    V                                     V              |       |
    Broadcast TX Beacon            Y <-- Received RX Beacon? --> N       |
                                   |                                     |
                                   V                                     |
                         Unicast Control Data ---------------------------

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
