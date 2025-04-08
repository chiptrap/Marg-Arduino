# Project Overview: Station 2 Encoders

## Purpose

This project controls three rotary encoders with integrated LED rings (WS2812B), three 3-position toggle switches, and a magnetic lock connected to an ESP32. It communicates via MQTT to report encoder positions, switch states, and maglock status. It can receive commands to control the maglock, enter different operational modes (like reporting encoder turning status or running light patterns), and execute a "ticker tape" style puzzle. The system also supports Over-The-Air (OTA) updates and reports health metrics.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|Rotary Encoder 1|Reads rotational input|CLK=13, DT=14|
|Rotary Encoder 2|Reads rotational input|CLK=15, DT=16|
|Rotary Encoder 3|Reads rotational input|CLK=17, DT=18|
|WS2812B LED Strip|Addressable RGB LEDs (3 rings, 35 LEDs each)|DATA=4|
|3-Position Switch 1|Reads Left/Center/Right state|L=19, R=21|
|3-Position Switch 2|Reads Left/Center/Right state|L=22, R=27|
|3-Position Switch 3|Reads Left/Center/Right state|L=25, R=26|
|Magnetic Lock Relay|Controls power to the maglock|32|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Station2Encoders/Control/` (Main control commands)
- `/Renegade/Room1/Station2Encoders/Control/Lights/` (Specific light animation commands)

### Published Topics/Channels

- `/Renegade/Room1/` (General status: Online, Boot, Watchdog, Quitting Mode)
- `/Renegade/Room1/Station2Encoders/` (Encoder positions (0-99), Switch states (e.g., "Left Center Right"), Maglock status, Mode status, Turning status/duration)
- `/Renegade/Room1/Station2Encoders/Health/` (Free Memory, RSSI, WiFi Channel, WiFi Info, IP Address)

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`quit`|Exits the currently active mode (if any).|System|
|`maglockRelease`|De-energizes the maglock relay (Pin 32 LOW).|Output|
|`maglockEngage`|Energizes the maglock relay (Pin 32 HIGH).|Output|
|`zero`|Resets all encoder counts to 0 and flashes LEDs green.|Encoder Control|
|`encoderPositions`|Publishes the current position (0-99) of all encoders as a comma-separated string.|Encoder Control|
|`{integer}` (on Lights topic)|Triggers a one-shot light animation (e.g., `0` flashes all rings white).|LED Control|
|`increment`|Advances the `tickerPuzzle` state (correct input).|Mode Control|
|`incorrect`|Resets the `tickerPuzzle` state (incorrect input).|Mode Control|
|`dumpEncoder`|Enters mode to continuously publish encoder positions.|Mode Setting|
|`isTurning`|Enters mode to publish when encoders start/stop turning and the duration.|Mode Setting|
|`followLight`|Enters mode where one LED per ring follows the encoder position.|Mode Setting|
|`fullLight`|Enters mode where all LEDs change color based on encoder positions.|Mode Setting|
|`tickerPuzzle`|Starts the ticker tape puzzle sequence (requires `increment`/`incorrect` commands).|Mode Setting|

## Component Configuration: Addressable LEDs (FastLED)

- **Type:** WS2812B
- **Data Pin(s):** 4
- **Number of Units:** 105 (3 rings of 35)
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255

## Component Configuration: Rotary Encoders

- **Driver/Library:** ESP32Encoder
- **Pin(s):** Enc1(13,14), Enc2(15,16), Enc3(17,18)
- **Parameter 1:** Type: Half Quadrature
- **Notes:** Position is read modulo 100. `isTurning` mode detects start/stop and duration of rotation.

## Component Configuration: 3-Position Switches

- **Type:** 3-Position Toggle Switches (using 2 GPIOs each)
- **Pin(s):** Sw1(19,21), Sw2(22,27), Sw3(25,26)
- **Activation Logic:** INPUT_PULLUP (LOW when active)
- **Notes:** Reads state as "Left", "Center", or "Right" based on which pin (or neither) is pulled LOW. Publishes combined state on change (e.g., "Left Center Right").

## Component Configuration: Magnetic Lock Relay

- **Type:** Relay Module (Assumed)
- **Activation Logic:** Active HIGH (Pin 32 HIGH engages lock)
- **Notes:** Controlled by `maglockEngage` and `maglockRelease` MQTT commands. Default state is Released (LOW).

## Additional Notes / Important Behaviors

- **Modes:** Several operational modes (`dumpEncoder`, `isTurning`, etc.) can be activated via MQTT. Only one mode can be active at a time. `quit` command exits the current mode.
- **Ticker Puzzle:** A specific mode (`tickerPuzzle`) uses `increment` and `incorrect` MQTT commands to progress or reset a light sequence on the LED rings.