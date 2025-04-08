# Project Overview: 4x4Keypad (Center Tower)

## Purpose

This project implements a 4x4 matrix keypad and an MFRC522 RFID reader connected to an ESP32 microcontroller. It provides visual feedback using a strip of WS2812B LEDs (FastLED).

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|MFRC522 RFID Reader|Reads MIFARE Classic RFID cards|SS=26, RST=27|
|WS2812B LED Strip|Addressable RGB LEDs for visual feedback|DATA=32|
|4x4 Matrix Keypad|Adafruit PID3844 Keypad for user input|R1=16, R2=15, R3=14, R4=13, C1=25, C2=22, C3=21, C4=17|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/4x4Keypad/` (General commands)
- `/Renegade/Room1/4x4Keypad/RFIDprogram/` (RFID programming commands)

### Published Topics/Channels

- `/Renegade/Room1/` (General status messages, e.g., "4x4 Keypad Online", Watchdog, Resetting)
- `/Renegade/Room1/4x4Keypad/Input/` (Individual key presses)
- `/Renegade/Room1/KeypadReader/` (RFID card read data or removal notification)
- `/Renegade/Room1/4x4Keypad/Data/` (RFID read/write results or errors)
- `/Renegade/Room1/4x4Keypad/Health/` (System health info: Free Memory, RSSI, WiFi Info, IP Address)

### Message Commands

|**Command**|**Action**|**Topic**|**Category (Optional)**|
|:--|:--|:--|:--|
|`reset`|Resets keypad input state and card mode.|`/Renegade/Room1/4x4Keypad/`|System|
|`{integer value}`|Writes the integer value to a presented RFID card (default block 4, index 0).|`/Renegade/Room1/4x4Keypad/RFIDprogram/`|RFID Control|
|`get card info`|Reads the content from a presented RFID card (default block 4, index 0) and publishes it.|`/Renegade/Room1/4x4Keypad/`|RFID Control|
|`enter card mode`|Sets the `cardMode` flag to true (currently unused in loop logic).|`/Renegade/Room1/4x4Keypad/`|Mode Setting|
|`light`|Triggers a brief light effect on the LED strip.|`/Renegade/Room1/4x4Keypad/`|LED Control|

## Component Configuration: MFRC522 RFID Reader

- **Library:** MFRC522
- **Pin(s):** SS = 26, RST = 27 (Uses default SPI pins for MOSI, MISO, SCK)
- **Card Type:** MIFARE Classic (Mini, 1K, 4K)
- **Default Read/Write:** Sector 1, Block 4, Index 0
- **Authentication Key:** Default (0xFFFFFFFFFFFF)
- **Notes:** Reads card content (byte) upon presentation and publishes changes. Can write a byte value received via MQTT.

## Component Configuration: Addressable LEDs (FastLED)

- **Type:** WS2812B
- **Data Pin(s):** 32
- **Number of Units:** 32 LEDs
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255 (Max)
- **Notes:** Used for status indication (boot, MQTT commands). Color values set using CHSV.

## Component Configuration: 4x4 Matrix Keypad

- **Driver/Library:** Adafruit_Keypad
- **Type:** PID3844
- **Pin(s):** Rows = {16, 15, 14, 13}, Columns = {25, 22, 21, 17}
- **Notes:** Publishes each key press individually via MQTT.

## Additional Notes / Important Behaviors

- **Debounce:** A 50ms `wait()` (which includes `handleAll()`) is called after each key press detection.
- **Startup:** Initializes Serial, SPI, RFID, WiFi, MQTT, FastLED, and Keypad. Publishes online status and network info. Sets LEDs to a default color.