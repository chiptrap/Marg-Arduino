# Project Overview: Station1RFID_V4_7

## Purpose

This project utilizes an ESP32 microcontroller to manage four MFRC522 RFID readers and a WS2812B LED strip. It reads RFID tags, communicates status and tag data via MQTT, and controls the LED strip based on RFID presence or commands received via MQTT. The system also supports Over-The-Air (OTA) updates for firmware deployment.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|FastLED Strip|WS2812B Addressable LEDs (32)|DATA=17|
|RFID Reader 0|MFRC522 RFID Reader|SS=13, RST=22, SPI (MOSI=23, MISO=19, SCK=18)|
|RFID Reader 1|MFRC522 RFID Reader|SS=14, RST=22, SPI (MOSI=23, MISO=19, SCK=18)|
|RFID Reader 2|MFRC522 RFID Reader|SS=15, RST=22, SPI (MOSI=23, MISO=19, SCK=18)|
|RFID Reader 3|MFRC522 RFID Reader|SS=16, RST=22, SPI (MOSI=23, MISO=19, SCK=18)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Station1RFID/Control/` : Primary control commands for the device.
- `/Renegade/Room1/Station1RFID/ActivePositions/` : Controls which RFID reader positions are active (e.g., "1,1,0,1").

### Published Topics/Channels

- `/Renegade/Room1/` : General status messages (Online, Quit, Watchdog).
- `/Renegade/Room1/Station1RFID/` : Publishes detected RFID tag data (space-separated bytes for each reader, 0 if none).
- `/Renegade/Room1/Station1RFID/Health/` : Publishes health metrics (Free Memory, RSSI, WiFi Channel, Connection Info, Reader Reinitialization events).

### Message Commands

|**Command**|**Action**|**Topic**|
|:--|:--|:--|
|`quit`|Stops the main operational loop (requires `modeActive` to be true, which is unused).|`/Renegade/Room1/Station1RFID/Control/`|
|`RfidRandom`|Enables a random pattern display on the LED strip.|`/Renegade/Room1/Station1RFID/Control/`|
|`ResetRfid`|Forces reinitialization of all connected RFID readers.|`/Renegade/Room1/Station1RFID/Control/`|
|`RfidFlash`|Disables random LED pattern, enables LED status based on RFID reads.|`/Renegade/Room1/Station1RFID/Control/`|
|`{ActivePositions}`|Comma-separated string of '1's and '0's (e.g., "1,1,0,1") to enable/disable specific readers.|`/Renegade/Room1/Station1RFID/ActivePositions/`|

## Component Configuration: Addressable LEDs (FastLED)

- **Type:** WS2812B
- **Data Pin(s):** 17
- **Number of Units:** 32 LEDs
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255
- **Notes:** Displays either a random pattern (`RfidRandom`=true) or status based on RFID reads (`RfidRandom`=false). Status logic defined in `updateLEDsBasedOnRFID`. Inactive positions show white. Active positions show Red (no card), Green/Red patterns based on card ID (1-4, 5, >5). LED mapping: Reader 0 -> LEDs 24-31, Reader 1 -> LEDs 16-23, Reader 2 -> LEDs 8-15, Reader 3 -> LEDs 0-7.

## Component Configuration: RFID Readers (MFRC522)

- **Driver/Library:** MFRC522 (via SPI)
- **Pin(s):** RST=22 (Common), SS=[13, 14, 15, 16], SPI (MOSI=23, MISO=19, SCK=18)
- **Parameter 1:** Number of Readers: 4
- **Parameter 2:** Default Authentication Key: FFFFFFFFFFFFh
- **Parameter 3:** Data Read Location: Block 4, Index 0 (by default)
- **Notes:** Reads a single byte from detected Mifare cards. Publishes space-separated list of detected card bytes (0 if no card) to `/Renegade/Room1/Station1RFID/`. Individual readers can be enabled/disabled via the `/Renegade/Room1/Station1RFID/ActivePositions/` topic. Automatically attempts reinitialization on read errors. Manual reset via `ResetRfid` command.

## Additional Notes / Important Behaviors
- **RFID Reinitialization:** If an RFID read fails, the specific reader attempts to reinitialize itself and reports this via the Health MQTT topic.