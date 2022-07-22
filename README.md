# M-Link Lite

M-Link Lite is a receiver for combat robotics based on the ESP8266 microcontroller. It acts as a WiFi access point and can be controlled from a WebSocket.

M-Link Lite hosts a web server with a basic page for controlling a simple combat robot.

Additional files can be uploaded to control more complex robots with up to 6 channels.

The servo pulsewidth data sent to the WebSocket is passed straight through to the outputs to avoid limiting possibilities.

M-Link Lite has a configurable failsafe so if no updates are received for 500ms the outputs will be put into a safe state.

When turned on M-Link will create a WiFi Access Point named m-link-XXXXXX where XXXXXX is a unique set of digits that identifies the device. The device should be labelled with this ID. A single user can connect to this access point and the controller page should be accessible by navigating to http://m-link-XXXXXXX.local or if wildcard DNS is enabled to any url e.g. http://m-link/ .

When connected to the access point the device will always be available at http://192.168.4.1 . If you are having trouble when using the access point from a mobile device, turning off mobile internet so WiFi is the only option may help.

Once configured via the access point you can enter details for your own access point and the device will connect to it from then on.

## Extending M-Link Lite

The default Drive page on the M-Link Lite device provides a single virtual joystick that directly drives the first two channels. It performs no mixing and is designed for a dual ESC with onboard mixing.

If you wish to use all 6 channels or other features you can upload additional files through the File Manager page that make use of these features.

As a starting point it is recommended to download the default 'Drive' page from [main/joystick.html](main/joystick.html), modify it, and upload to the file manager. For quick tests you can change `websocket = 'ws://' + location.host + '/ws';` on line 12 to `websocket = 'ws://m-link-XXXXXX.local/ws'; where m-link-XXXXXX is the ID if your device, then load the page directly in your browser without uploading to the device.

The on-screen Joystick is implemented with [jeromeetienne/virtualjoystick.js](https://github.com/jeromeetienne/virtualjoystick.js) which has its own documentation and examples for adding additional joysticks.

The page talks to the M-Link Lite by sending JSON messages over a websocket and the device will send JSON messages back in response.

The code at line 40 of the Drive page sends an initial packet to center all outputs, and it also sends a set of failsafe values which will be applied if a packet is not received for 500 milliseconds. This is intended to avoid the robot going out of control if the user closes their browser, turns of the controlling device, or the connection is otherwise disrupted.

```
        ws.onopen = function() {
          ws.send(
            JSON.stringify(
              {
                servos: [
                  1500,
                  1500,
                  1500,
                  1500,
                  1500,
                  1500
                ],
                failsafes: [
                  1500,
                  1500,
                  1500,
                  1500,
                  1500,
                  1500
                ],
                query: "settings"
              }
            )
          );
        };
```

The default failsafes will stop a brushed ESC that has a reverse feature and center a servo. If you intend to build a robot using non-reversible ESCs or your servo driven mechanisms require a different position on failsafe you will need to update these values.

The values sent in the `servos` and `failsafes` array are the pulse width that will be sent to the servo in microseconds. 1000 microseconds is usually fully left, 2000 microseconds is fully right, and 1500 microseconds is centered. You can send 0 microseconds to stop sending a signal, which will cause many servos to idle, but depending on the servos you will need to be careful with other out of range values which may cause unexpected behaviour and/or damage.

For the `failsafes` array a value of -1 can be set instead of a pulsewidth to hold the channel at its last position when in failsafe.

The code between lines 103 and 124 is responsible for sending updated values to the M-Link Lite 30 times a second. By altering the values to the `servos` array you can make use of all 6 channels.

```
      setInterval(function(){
        const x = Math.round(joystick.deltaX() * 5);
        const y = Math.round(joystick.deltaY() * 5);
        $msg = JSON.stringify({
          servos: [
            parseInt(1500 - y),
            parseInt(1500 + x),
            1500,
            1500,
            1500,
            1500,
          ]
        });
        if ($('#send').val() != $msg || (++counter % keepalive) === 0) {
          $('#send').val($msg);
          ws.send($msg);
        }

        var outputEl  = document.getElementById('result');
        outputEl.innerHTML = $('#wsresponse').val();

      }, 1/30 * 1000);
```

For example you could declare a second VirtualJoystick instance and have it respond to a second area of the screen, and use it to drive 1 or 2 of the extra channels.

These extensions are likely to depend on the intended features of the robot so the system has designed to leave these up to the user while providing maximum flexibility.

For ease of experimentation the default Drive page declares a `mlink` object in the global scope which contains a reference to the WebSocket. You can use this from the browser's Javascript console to send messages manually.

For example, to request the M-Link device send back the current battery voltage reading:

```
mlink.ws.send(JSON.stringify({
  query: "battery"
});
```

## WebSocket API

The WebSocket API expects a JSON message and responds based on the keys contained in the message.

### Sending servo pulsewidths to drive the robot:

For example Ch 1 center/brake, Ch 2 left/reverse, Ch 3 right/forward, Ch 4 center/brake, Ch 5 left/reverse, Ch 6 right/forward

```
{
  servos: [
    1500,
    1000,
    2000,
    1500,
    1000,
    2000
  ]
}
```

The M-Link will read as many channels from the array as it supports and then ignore any additional values. If fewer values are sent than are supported the remaining channels will be left alone.

### Setting Failsafe Positions

For example Ch 1 center/brake, Ch2 left, Ch3 right, Ch4/5/6 hold position

```
{
  failsafes: [
    1500,
    1000,
    2000,
    -1
    -1
    -1
  ]
}
```

The `failsafes` key is handled in the same way as a `servos` key with the exception that -1 is interpreted as hold position.

### Updating settings

```
{
  settings: {
    name: "My Robot",
    ssid: "my-ssid",
    password: "my-password"
  }
}
```

The settings object can contain any or all of the keys `name`, `ssid`, `password` and the settings relating to any keys provided will be updated upon receipt of the packet.

If the SSID or Password for the WiFi are changed the settings will be saved immediately but the device must be rebooted before they will take effect.

The SSID and Password fields refer to an external Access Point the device will try to connect to. The details for the M-Link's Access Point are fixed and cannot be changed.

### Resetting Settings

```
{
  reset_settings: "sgnittes_teser"
}
```

The `reset_settings` key will cause a reset of all settings to factory defaults as soon as it is received, so long as the value is the string `"reset_settings"` backwards to prevent accidental resets.

### Rebooting

```
{
  reboot: "toober"
}
```

The `reboot` key will cause an immediate reboot of the device so long as the value is the string `"reboot"` backwards, to prevent accidental reboots.

This can be used after updating settings to apply any that require a reboot to take effect.

### Query

The query key can be used to read various values back from the M-Link.

```
{
  query: "battery"
}
```

The device will respond to a `battery` query with an object containing the value read on the ADC which may be connected to an external battery of the 5v rail of the servo connectors depending on the device and how it is wired.

```
{
  query: "failsafes"
}
```

The device will respond to a `failsafes` query with the current failsafe values.

```
{
  query: "settings"
}
```

The device will respond to a `settings` query with an object containing the current settings.

## Safety

M-Link Lite is designed as a simple device to make it easy to get started and because of this it has been designed to be simple and robust at the expense of some security features.

To avoid users locking themselves out of the device its Access Point has a fixed password, and when connected to a shared Access Point there is no authentication to prevent other users from taking control of the device. For this reason we do not recommend using this device for any robot with spinning weapons or any other weapons that would be dangerous if activated unexpectedly.
