# Project Overview: Center Wall LEDs

## Purpose

This project controls multiple strips of WS2812B addressable LEDs connected to an ESP32 microcontroller. It connects to a WiFi network, communicates via MQTT to receive commands for different lighting modes (e.g., "ambient", "doorActivity") and publish status/health information, and supports Over-The-Air (OTA) updates.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|ESP32 Controller|Microcontroller|N/A|
|LED Strip 1 (`leds`)|WS2812B Addressable LEDs (108 LEDs)|33 (Data)|
|LED Strip 2 (`leds1`)|WS2812B Addressable LEDs (108 LEDs)|32 (Data)|
|LED Strip 3 (`ledsV6`)|WS2812B Addressable LEDs (293 LEDs)|27 (Data)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/CenterWallLEDs/Control/`

### Published Topics/Channels

- `/Renegade/Engineering/` (Main status/notifications like online, quitting, watchdog)
- `/Renegade/CenterWallLEDs/data/` (General data - currently unused in provided code)
- `/Renegade/CenterWallLEDs/data/Health/` (Health metrics: Free Memory, RSSI, WiFi Channel, Connection Info)
- `/Renegade/Engineering/` (Watchdog messages)

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`ambient`|Starts an ambient lighting effect on `leds` and `leds1`.|LED Control|
|`doorActivity`|Starts a blinking effect on `ledsV6`.|LED Control|
|`quit`|Stops the currently active mode (if `modeActive` is true).|System|

## Component Configuration: Addressable LEDs

- **Type:** WS2812B
- **Data Pin(s):** 33 (`leds`), 32 (`leds1`), 27 (`ledsV6`)
- **Number of Units:** 108 (`leds`), 108 (`leds1`), 293 (`ledsV6`)
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255 (Master brightness)
- **Notes:** Uses the FastLED library. `TypicalLEDStrip` color correction is applied.

## Additional Notes / Important Behaviors

- The `loop()` function runs a default pattern cycling colors when no specific mode is active via MQTT.
- MQTT commands are only processed if `modeActive` is false. The `ambient` and `doorActivity` commands set `modeActive` to true and run in a blocking loop until a `quit` command is received.