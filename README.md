# M-Link Lite

M-Link Lite is a receiver for combat robotics based on the ESP8266 microcontroller. It acts as a WiFi access point and can be controlled from a WebSocket.

M-Link Lite hosts a web server with a basic page for controlling a simple combat robot.

Additional files can be uploaded to control more complex robots with up to 6 channels.

The servo pulsewidth data sent to the WebSocket is passed straight through to the outputs to avoid limiting possibilities.

M-Link Lite has a configurable failsafe so if no updates are received for 500ms the outputs will be put into a safe state.
