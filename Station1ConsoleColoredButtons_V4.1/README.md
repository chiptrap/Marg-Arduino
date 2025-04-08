# Project Overview: Station 1 Console 1-4 Colored Buttons (V4.1)

## Purpose

This project reads input from a console featuring 8 buttons and 2 three-position switches, managed via shift registers. It waits for a specific MQTT command (`sequenceMode`) to activate. Once active, it requires another MQTT message (`Drive Signature Solved`) before accepting a 6-button sequence input and checking the switch positions. The controller validates the input sequence and switch positions against a predefined correct combination (specific to the configured Console). Feedback is provided via 4 addressable LEDs and by illuminating the pressed buttons. Success or failure, along with the input sequence, is reported via MQTT.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|74HC165 Shift Register|Parallel-in, Serial-out Shift Register for reading button and switch states.|Data: D2, Clock: D3, Latch: D4|
|74HC595 Shift Register|Serial-in, Parallel-out Shift Register for controlling button LEDs.|Data: D7, Clock: D5, Latch: D6|
|WS2812B LED Strip|Addressable LEDs for status feedback.|Data: D1|
|Buttons (x8)|Input buttons connected to 165 shift register inputs 8-15.|Via 165 SR (Pins 8-15)|
|Green Switch|3-position switch connected to 165 shift register inputs 5 (Right) and 6 (Left). Middle is 0.|Via 165 SR (Pins 5, 6)|
|Red Switch|3-position switch connected to 165 shift register inputs 3 (Right) and 4 (Left). Middle is 0.|Via 165 SR (Pins 3, 4)|
|Button LEDs (x8)|LEDs corresponding to each input button, controlled by the 595 shift register.|Via 595 SR|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Station1Buttons/Control/`

### Published Topics/Channels

- `/Renegade/Room1/` (General status like Online/Quit messages)
- `/Renegade/Room1/Station1Buttons/` (Game state messages: "Sequence mode starting...", "Incorrect entry", "Console X correct entry...")
- `/Renegade/Room1/Station1Buttons/Inputs/` (Publishes the entered sequence and switch positions, e.g., "4,13,11,10,9,8,15,5,3")

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`sequenceMode`|Initiates the puzzle sequence. Waits for "Drive Signature Solved" message. Pulses Red LEDs while waiting.|Mode Control|
|`Drive Signature Solved`|Received after `sequenceMode`. Enables button input. Pulses Yellow LEDs.|Mode Control|
|`quit`|Stops the active mode, turns off LEDs, and publishes a quit message.|System|

## Component Configuration: Shift Registers (Input/Output)

- **Input Type:** 74HC165 (PISO)
- **Input Pins:** Data: D2, Clock: D3, Latch: D4
- **Input Bits Read:** 16 (Inputs 8-15 for buttons, 3-6 for switches)
- **Output Type:** 74HC595 (SIPO)
- **Output Pins:** Data: D7, Clock: D5, Latch: D6
- **Output Control:** Controls LEDs integrated into the input buttons (corresponding to inputs 8-15).
- **Notes:** Reads all 16 inputs from the 165 register. Uses the 595 register to light up corresponding button LEDs as they are pressed.

## Component Configuration: Addressable LEDs (FastLED)

- **Type:** WS2812B
- **Data Pin(s):** D1
- **Number of Units:** 4 LEDs
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255
- **Configuration Detail 3:** Voltage: 5V (Max Power: 500mA)
- **Notes:** Uses HSV color space. Provides visual feedback: Pulsing Red (waiting for Drive Signature), Pulsing Yellow (waiting for input), Solid Red (incorrect input), Pulsing Purple (correct input).

## Additional Notes / Important Behaviors

- **Multi-Console Configuration:** This single codebase (`Station1ConsoleColoredButtons_V4.1.ino`) is used for all 4 consoles at Station 1. Before uploading to a specific console's Arduino, ensure the correct `consoleNumber`, `hostName`, `correctSwitchPositions`, and `correctArray` lines are uncommented (and others are commented out) in the code header section (lines ~35 and ~80-100).
- The puzzle requires a sequence of 6 button presses.
- Button input on register pin 7 is ignored.
- The system checks both the button sequence and the position of the two switches for correctness based on the uncommented configuration.
- If the input is incorrect, LEDs flash Red, then turn off briefly before returning to the Yellow input state.
- If the input is correct, LEDs pulse Purple, and the system waits for a `quit` command via MQTT to reset.
- Switch inputs are debounced with a 10ms delay.
- The `loop()` function continuously reads the input shift register and updates the output shift register, effectively making the button LEDs mirror the button presses when no specific mode is active (though this state is usually superseded by `sequenceMode`).