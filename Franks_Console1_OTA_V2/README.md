# Project Overview: Franks_Console1_OTA_V2

## Purpose

This project reads the state of 16 digital inputs using a 74HC165 shift register. It compares the current input states to a predefined "solved" state array. The controller publishes individual input changes, the overall input status, and the solved/unsolved status of the puzzle via MQTT. It also mirrors the input states to 16 digital outputs using a 74HC595 shift register.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|74HC165|Input Shift Register|Data (Q7): D2, Clock (CP): D3, Latch (PL): D4|
|74HC595|Output Shift Register|Data (DS): D7, Clock (SH_CP): D5, Latch (ST_CP): D6|
|Inputs 1-16|Connected to 74HC165 Parallel Inputs|N/A (Shift Register)|
|Outputs 1-16|Connected to 74HC595 Parallel Outputs|N/A (Shift Register)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Engineering/Console1/`

### Published Topics/Channels

- `/Renegade/Engineering/` (Initial connection message)
- `/Renegade/Engineering/Console1/` (Input changes, status updates, solved state)

### Message Commands

|**Command**|**Action**|**Topic Received On**|
|:--|:--|:--|
|`Console 1 Status`|Publishes "Console 1 online" to `/Renegade/Engineering/`|`/Renegade/Engineering/Console1/`|

## Component Configuration: 74HC165 Input Shift Register

- **Type:** 74HC165 Parallel-In Serial-Out
- **Data Pin(s):** D2
- **Clock Pin(s):** D3
- **Latch Pin(s):** D4
- **Number of Units:** 2 (Implied, for 16 inputs)
- **Notes:** Reads 16 digital input states.

## Component Configuration: 74HC595 Output Shift Register

- **Type:** 74HC595 Serial-In Parallel-Out
- **Data Pin(s):** D7
- **Clock Pin(s):** D5
- **Latch Pin(s):** D6
- **Number of Units:** 2 (Implied, for 16 outputs)
- **Notes:** Outputs mirror the state read from the 74HC165 inputs.

## Additional Notes / Important Behaviors

- Reads 16 input states continuously in the main loop.
- Publishes MQTT message `Input {i+1} changed to {bit}` to `/Renegade/Engineering/Console1/` when an input changes state.
- Publishes MQTT message `Current input status: {comma-separated list of 0s and 1s}` to `/Renegade/Engineering/Console1/` immediately after an input change is detected.
- Compares the current 16 input states (`buttonStates`) against the `expected_array` (`{0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1}`).
- Publishes "Console 1 Puzzle Solved" to `/Renegade/Engineering/Console1/` when the input state matches `expected_array` and the `solved` flag is false. Sets `solved` flag to true.
- Publishes "Console 1 Puzzle Not Solved" to `/Renegade/Engineering/Console1/` when the input state does not match `expected_array` and the `solved` flag is true. Sets `solved` flag to false.
- The state read from the inputs is immediately written to the 74HC595 outputs in the main loop.