# Project Overview: Franks_CenterTower_OTA_V2

## Purpose

This project controls three strips of WS2812B addressable LEDs connected to an ESP32 microcontroller. It connects to an MQTT broker to receive commands for turning the LEDs on/off, setting brightness, and potentially triggering specific patterns (though currently limited). The primary function is to provide visual effects for a center tower structure.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|WS2812B LED Strip 0|Addressable LED Strip|25|
|WS2812B LED Strip 1|Addressable LED Strip|26|
|WS2812B LED Strip 2|Addressable LED Strip|27|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Engineering/CenterColumn/`
- `/Renegade/Engineering/CenterColumn/brightness/`
- `/Renegade/Engineering/` (General topic, usage may vary)

### Published Topics/Channels

- `/Renegade/Engineering/Center Column/` (Status messages like "Online", "On Requested", "Off Requested", "Brightness Set")
- `/Renegade/Engineering/WireTracePuzzle/Rssi/` (Reports WiFi Signal Strength - *Note: Topic name seems inconsistent with project*)
- `/Renegade/Engineering/WireTracePuzzle/Channel/` (Reports WiFi Channel - *Note: Topic name seems inconsistent with project*)


### Message Commands

|**Command**|**Action**|**Topic**|
|:--|:--|:--|
|`on`|Enables the default LED effect (fading beat).|`/Renegade/Engineering/CenterColumn/`|
|`off`|Turns all LEDs off (sets to black).|`/Renegade/Engineering/CenterColumn/`|
|`green`|Publishes "Center Column Green Requested" (currently no visual change).|`/Renegade/Engineering/CenterColumn/`|
|`<integer>`|Sets the master brightness level (0-255).|`/Renegade/Engineering/CenterColumn/brightness/`|


## Component Configuration: Addressable LEDs

- **Type:** WS2812B
- **Data Pin(s):** 25, 26, 27
- **Number of Units:** 2816 LEDs per strip (3 strips total)
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: Variable via MQTT (default 255)
- **Configuration Detail 3:** Max Power Defined: 30000 mA (Note: Active power limiting not implemented in main loop)
- **Notes:** Uses FastLED library. `TypicalLEDStrip` color correction applied.

## Additional Notes / Important Behaviors

- On startup, LEDs are initialized to black (off).
- The default 'on' state runs a `beatsin16` effect independently on each strip, fading to black.
- The `rainbow` boolean variable controls whether the effect runs (`true`) or LEDs stay off (`false`).