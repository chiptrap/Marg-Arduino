# Project Overview: Episode 1 Briefcase Keypad

## Purpose

This project implements a 3x4 matrix keypad and an SSD1306 OLED display connected to an ESP32 microcontroller. It allows users to enter a 4-digit code. Upon correct entry and receiving an MQTT 'unlock' command, it activates a relay to unlock a briefcase latch. It also supports remote code changes and status updates via MQTT, and Over-The-Air (OTA) updates.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|Adafruit 3x4 Keypad (PID3845)|User input for 4-digit code|C1=19, C2=33, C3=32, R1=23, R2=25, R3=26, R4=27|
|SSD1306 OLED Display|128x64 I2C display for user feedback and status|SCL=22, SDA=21|
|Relay Module|Controls the briefcase latch mechanism|4|
|Latch Switch|Input sensor (likely for detecting if latch is closed/open)|16 (INPUT_PULLUP)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Briefcase/Control/` (Receives commands like 'unlock', 'lock', 'force unlock', 'ESP RESTART')
- `/Renegade/Room1/Briefcase/CodeChange/` (Receives new 4-digit code)

### Published Topics/Channels

- `/Renegade/Room1/` (General status: Online, Watchdog)
- `/Renegade/Room1/Briefcase/` (Key presses, full code entry, status messages like 'Waiting for unlock signal', 'Briefcase Unlocked', 'Passkey changed')

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`unlock`|Sets the internal `unlocked` flag to true, allowing the unlock sequence to proceed after correct code entry.|Relay Control|
|`lock`|Sets the internal `unlocked` flag to false. If the briefcase was unlocked, it returns to the code entry screen.|Relay Control|
|`force unlock`|Immediately pulses the relay HIGH then LOW, sets `unlocked` flag to true, and bypasses code entry for the current cycle.|Relay Control|
|`ESP RESTART`|Restarts the ESP32 microcontroller.|System|
|`{new 4-digit code}` (on CodeChange topic)|Updates the `briefcasePassword` variable with the new code.|System|

## Component Configuration: Adafruit 3x4 Keypad

- **Type:** PID3845 (3x4 Matrix Keypad)
- **Driver/Library:** Adafruit_Keypad
- **Pin(s):** C1=19, C2=33, C3=32, R1=23, R2=25, R3=26, R4=27
- **Notes:** Reads key presses for code entry. '*' long press enters maintenance mode. '#' is ignored for code input.

## Component Configuration: SSD1306 OLED Display

- **Type:** SSD1306 128x64
- **Driver/Library:** U8g2lib
- **Interface:** Software I2C
- **Pin(s):** SCL = 22, SDA = 21
- **Notes:** Displays boot sequence, code entry prompts, status messages (Success, Failure, Unlocking, Override), maintenance info, and animations.

## Component Configuration: Relay

- **Type:** Generic Relay Module (Assumed)
- **Activation Logic:** Active HIGH (Brief pulse)
- **Pin:** 4
- **Notes:** Controls the physical unlocking mechanism of the briefcase. Pulsed briefly upon successful unlock sequence.

## Additional Notes / Important Behaviors

- **Default Code:** The default unlock code is "8631".
- **Unlock Sequence:** Requires both the correct code entry AND receiving an "unlock" MQTT command before activating the relay.
- **OTA Updates:** The system supports ArduinoOTA for wireless firmware updates. The display shows update progress.
- **Maintenance Mode:** Holding the '*' key for >3 seconds enters a maintenance screen displaying Battery Status (Not Implemented), IP Address, and Device Name. Press '*' again to exit.
- **Watchdog:** Publishes an MQTT watchdog message every 10 seconds.
- **WiFi/MQTT Reconnect:** Attempts to reconnect to WiFi and MQTT if the connection is lost.
- **Boot Sequence:** Displays a boot animation and copyright information on startup.
- **Input Handling:** Uses a custom `wait()` function that incorporates `handleAll()` (OTA, MQTT loop, Connection Check) to remain responsive during delays.