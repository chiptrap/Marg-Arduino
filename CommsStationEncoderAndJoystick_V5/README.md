# Project Overview: Comms Station Encoder and Joystick

## Purpose

This project reads input from a rotary encoder and a 4-way joystick with a push button. It publishes the state changes and position data via MQTT and controls a strip of WS2812B LEDs based on encoder/joystick state and received MQTT commands. It operates in different modes controlled by MQTT messages.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|Rotary Encoder 1|Reads rotational input|26 (CLK), 27 (DT)|
|Joystick 1|Reads directional input (4-way)|19 (UP), 21 (DOWN), 22 (LEFT), 32 (RIGHT)|
|Button 1|Push button integrated with Joystick|25|
|Addressable LEDs|WS2812B LED Strip|17 (Data)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/CommsStationEncoder/Control/`
- `/Renegade/Room1/CommsStationEncoder/Control/Lights/`
- `/Renegade/Room1/CommsStationJoysticks/Control/`
- `/Renegade/Room1/CommsStationJoysticks/Request/`

### Published Topics/Channels

- `/Renegade/Room1/` (General status like Online/Quitting)
- `/Renegade/Room1/CommsStationEncoderJoystick/` (Primary data channel for encoder position, joystick direction, button state, mode status)
- `/Renegade/Engineering/CommsJoystick/Health/` (Health metrics like Free Memory, RSSI, WiFi Channel)

### Message Commands

|**Command**|**Topic/Channel**|**Action**|**Category**|
|:--|:--|:--|:--|
|`quit`|Any subscribed topic (when `modeActive`)|Exits the current active mode.|System|
|`zero`|Any subscribed topic|Resets the encoder count to 0 and flashes LEDs green.|Encoder Control|
|`encoderPositions`|Any subscribed topic|Publishes the current encoder position(s) to `dataChannel`.|Encoder Control|
|`{integer}` (e.g., `0`, `1`)|`/Renegade/Room1/CommsStationEncoder/Control/Lights/`|Triggers specific one-shot LED animations (Case 0: All rings pulse white).|LED Control|
|`{integer}` (e.g., `0`, `2`, `4`, `6`)|`/Renegade/Room1/CommsStationJoysticks/Control/`|Sets the `joystickOffset` value for rotational adjustment.|Joystick Control|
|`dumpEncoder`|Any subscribed topic (when `!modeActive`)|Enters a mode that continuously publishes encoder position changes to `dataChannel`.|Mode Control|
|`isTurning`|Any subscribed topic (when `!modeActive`)|Enters a mode that publishes messages when the encoder starts or stops turning, including duration, to `dataChannel`.|Mode Control|
|`directionMode`|Any subscribed topic (when `!modeActive`)|Enters a mode that reads joystick direction and button presses, publishing changes to `dataChannel`, and runs a default LED animation.|Mode Control|

## Component Configuration: Addressable LEDs

- **Type:** WS2812B
- **Data Pin(s):** 17
- **Number of Units:** 39 LEDs
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255 (Max)
- **Configuration Detail 3:** Voltage: 5V (Max Power Limit: 1000mA)
- **Notes:** Uses the FastLED library. Default loop behavior shows a gradient based on encoder position.

## Component Configuration: Rotary Encoder

- **Library:** ESP32Encoder
- **Pin(s):** 26 (CLK), 27 (DT)
- **Number of Units:** 1
- **Configuration Detail 1:** Attached using `attachHalfQuad`.
- **Configuration Detail 2:** Position calculated as `(encoder.getCount() / 2) % 100`.
- **Notes:** Tracks turning state and duration with a 500ms timeout.

## Component Configuration: Joystick & Button

- **Pin(s) Joystick:** 19 (UP), 21 (DOWN), 22 (LEFT), 32 (RIGHT)
- **Pin(s) Button:** 25
- **Number of Units:** 1 Joystick, 1 Button
- **Activation Logic:** `INPUT_PULLUP` (Reads LOW when active)
- **Notes:** `readJoystick` function calculates 8 directions plus center (0). Includes an offset parameter (default 2) settable via MQTT to adjust orientation. Button state changes (pressed/released) are published.

## Additional Notes / Important Behaviors

- The system enters specific operational modes (`dumpEncoder`, `isTurning`, `directionMode`) based on MQTT commands. Only one mode can be active at a time.
- The `quit` command exits the current mode.
- Joystick direction reporting can be adjusted using the `joystickOffset` variable, controlled via MQTT.
- The default `loop()` function runs a gradient LED animation based on the encoder position when no specific mode is active.