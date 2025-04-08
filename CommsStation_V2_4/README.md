# Project Overview: Comms Station V2.4

## Purpose

This project implements a communication station interface using an ESP8266. It reads input from four 8-bit rotary switches (via 74HC165 shift registers) and a potentiometer (analog input) to determine a frequency. It displays information and status on an SSD1309 OLED display and controls a strip of 16 WS2812B LEDs for visual feedback. The station interacts with a central system via MQTT to receive commands, report its status, frequency settings, and button interactions.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|SSD1309 OLED Display|I2C Display for UI|D1 (SCL), D2 (SDA)|
|74HC165 Shift Registers (x4)|Parallel-in, Serial-out for reading rotary switches|D3 (Load/PL), D5 (Clock/CP), D6 (Data/Q7)|
|Potentiometer|Analog input for frequency fine-tuning|A0|
|Push Button|Input for user interaction (Contact/Confirm)|D4 (Internal Pullup)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Comms4/` ([`mqttComms`]) - General commands for this specific station.
- `/Renegade/Room1/Comms4Set/` ([`mqttCommsSet`]) - Commands to set the target frequency or department.
- `/Renegade/Room1/CommsAll/` ([`mqttCommsAll`]) - Commands intended for all comms stations.
- `/Renegade/Room1/Comms/LEDs/` ([`subscribeChannel[0]`]) - Commands specifically for LED animations/modes.

### Published Topics/Channels

- `/Renegade/Room1/` - General status messages (Online, Puzzle Start, Button states, Department selections, Lock/Unlock status).
- `/Renegade/Room1/Comms4/` ([`mqttComms`]) - Current frequency reading, lock confirmation.
- `/Renegade/Room1/Comms4/Reset/` ([`mqttCommsReset`]) - Reports current frequency upon request (`resetOk?`).

### Message Commands

|**Command**|**Action**|**Topic**|
|:--|:--|:--|
|`commsPuzzle`|Starts the main frequency setting puzzle mode.|[`mqttComms`]|
|`quit`|Stops the current active mode (e.g., `commsPuzzle`, LED animation).|Any subscribed topic|
|`unlock`|Unlocks frequency and department selection.|[`mqttCommsSet`]|
|`lock`|Locks the selected department.|[`mqttCommsSet`]|
|`lockFreq`|Locks the frequency input digits.|[`mqttCommsSet`]|
|`resetOk?`|Requests the station to report its current frequency.|[`mqttCommsSet`]|
|`report`|Requests the station to report its status.|[`mqttCommsSet`]|
|`Engineering`|Sets the target department display to Engineering.|[`mqttCommsSet`]|
|`Docking`|Sets the target department display to Cargo Bay.|[`mqttCommsSet`]|
|`Infirmary`|Sets the target department display to Infirmary.|[`mqttCommsSet`]|
|`Security`|Sets the target department display to Security.|[`mqttCommsSet`]|
|`Bridge`|Sets the target department display to Bridge.|[`mqttCommsSet`]|
|`Diagnostics`|Sets the target department display to Service.|[`mqttCommsSet`]|
|`Research`|Sets the target department display to Research.|[`mqttCommsSet`]|
|`Telemetry`|Sets the target department display to Telemetry.|[`mqttCommsSet`]|
|`Unknown`|Sets the target department display to Unknown.|[`mqttCommsSet`]|
|`allowButton`|Resets the button pressed state, allowing it to be detected again.|[`mqttCommsSet`]|
|`buttonsOn`|Turns on LEDs with predefined Hues.|[`mqttCommsSet`]|
|`buttonsBlue`|Turns all LEDs blue.|[`mqttCommsSet`]|
|`buttonsOff`|Turns all LEDs off.|[`mqttCommsSet`]|
|`0`|Starts "Low Power" LED animation and displays error on OLED.|[`subscribeChannel[0]`]|
|`1`|Starts "Ambient" LED animation and displays "comms unavailable" on OLED.|[`subscribeChannel[0]`]|

## Component Configuration: OLED Display

- **Library:** U8g2lib
- **Type:** SSD1309 128x64
- **Interface:** I2C (Software)
- **Pin(s):** D1 (Clock), D2 (Data)
- **Notes:** Used to display frequency, department, status messages, and prompts.

## Component Configuration: Shift Registers (74HC165)

- **Quantity:** 4 ([`numShiftRegisters`])
- **Pin(s):** D3 ([`loadPin`]), D5 ([`clockPin`]), D6 ([`dataPin`])
- **Notes:** Reads the state of four 8-position rotary switches to determine the first four digits of the frequency.

## Component Configuration: Addressable LEDs

- **Library:** FastLED
- **Type:** WS2812B ([`LED_TYPE`])
- **Data Pin(s):** D7 ([`DATA_PIN`])
- **Number of Units:** 16 ([`NUM_LEDS`])
- **Configuration Detail 1:** Color Order: GRB ([`COLOR_ORDER`])
- **Configuration Detail 2:** Brightness: 255 ([`BRIGHTNESS`])
- **Notes:** Provides visual feedback for different modes and states (e.g., idle, puzzle active, locked, low power). Uses predefined hues ([`hueArray`]) for some states.

## Component Configuration: Button

- **Pin(s):** D4 ([`buttonPin`])
- **Configuration:** Input with internal pull-up resistor (`INPUT_PULLUP`).
- **Notes:** Detects presses and holds. Used for confirming department selection or initiating contact when the department is locked.

## Additional Notes / Important Behaviors

- The frequency is determined by reading four 8-position switches (giving digits 0-9 based on active position 1-8, assuming 0 if none active) and a potentiometer mapped to 0-9 for the decimal place.
- The system has several states controlled via MQTT: idle, frequency setting ([`commsPuzzle`]), frequency locked ([`digitsLocked`]), department selection ([`unk`]), department locked ([`deptLocked`]).
- The OLED display updates based on the current state, showing prompts, frequency, department names, or status messages.
- Button press and hold events are detected and published via MQTT.
- The [`correctStation`] variable defines which department name is considered correct for the final "Contact" screen.
- Default frequency ([`predefinedNumber`]) can be overridden via MQTT.