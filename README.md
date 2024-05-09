# WiFi mod for Belkin SoHo 4-port DVI KVM

ESP8266 KVM Switch Automator.
Phil Pemberton <philpem@philpem.me.uk>

This is the firmware for a WiFi mod for the Belkin SOHO KVM switch (model F1DD104L). It's based around a WeMos D1 Mini clone board (ESP8266 processor) and allows a HTTP client to read the KVM's status or switch the ports.

The software was developed with the Arduino IDE and the ESP8266 board support package.

I use this with Bitfocus Companion to control a matrix switcher and KVM switch.

**Caution: Control is done via HTTP with no authentication, so this should only be used on a trusted network.**

## Instructions:

  - Open the Arduino IDE. Check preferences and make sure the following URLs are included in the board support URL list:
    * https://dl.espressif.com/dl/package_esp32_index.json
    * http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the Board Manager (in the IDE) and install the ESP8266 board support package. (I used version 2.7.4)
  - Edit config.h and set your WiFi settings.
  - Flash the firmware onto the ESP8266 (remember to set your board type to Wemos D1 R2 and Mini)
  - Connect as below. (use plug-in "Dupont" crimp connectors so the ESP can be removed if needed)
  - Point your browser to http://kvm-switch.local, you should see a response.
## HTTP requests available
  - http://kvm-switch,local/
     - returns a default homepage
  - http://kvm-switch.local/get
     - returns the current port number
  - http://kvm-switch.local/switch?n=1 switches to port 1 --
     - change the port number parameter as needed
     - the response will be the same as "get", i.e. the current port number.
     - Response is not returned until the switch has activated (~100ms).

## Electrical wiring

This should be done with removable connectors if possible. Dupont style square-pin headers and crimp sockets are a good choice.

  - +5V: front panel pin 1 or 2
  - GND: front panel pin 11, 12, 18, 23 or 24
  - D1: Switch 1 "push" -- front panel pin 5
  - D2: Switch 2 "push" -- front panel pin 8
  - D3: Switch 3 "push" -- front panel pin 17
  - D4: Switch 4 "push" -- front panel pin 22
  - D7: PC LED binary low -- LS139 pin 2 / front panel pin 15
  - D8: PC LED binary low -- LS139 pin 3 / front panel pin 16

## Belkin SOHO front-panel pinout

This is a 26-pin 0.1-inch header. Pin 1 is marked on the silkscreen with a filled in white square under that pin.

The full 26-pin pinout is as follows:

```
     1: V+ (5V)
     2: V+ (5V)
     3: Port 1, Right (switch PC)
     4: Port 1, Left  (switch audio)
     5: Port 1, Push  (switch both)
     6: Port 2, Right
     7: Port 2, Left
     8: Port 2, Push
     9: Port 3, Right
    10: Port 3, Left
    11: GND
    12: GND
    13: LS139 pin 14 (Audio LED binary select)
    14: LS139 pin 13 (Audio LED binary select)
    15: LS139 pin 2  (KVM LED binary select)
    16: LS139 pin 3  (KVM LED binary select)
    17: Port 3, Push
    18: GND
    19: ? (possibly no connect)
    20: Port 4, Right
    21: Port 4, Left
    22: Port 4, Push
    23: GND
    24: GND
    25: USB data (to front USB port)
    26: USB data (to front USB port)
```

## Switch pinning

  The switches are 10x10mm through-hole directional switches, pinned as follows:

```
    PUSH  UP  COMMON
    ||    ||  ||
  +--------------+
  |              |
  |              |
  |              |
  |              |
  |              |
  +--------------+
    ||    ||  ||
   LEFT  DOWN RIGHT
```

Common connects to ground via a 1k resistor.

When a direction is pushed, the direction pin connects to Common.

## LED selection

LED selection is done with a 74LS139 mux. The incoming binary inputs select the active LED. LEDs 2 and 3 seem to be swapped, probably to ease PCB layout. This is compensated for in the ESP firmware.

