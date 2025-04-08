# Project Overview: Engineering Corner Printer and Table Controller

## Purpose

This controller manages the stepper motor for a printer feed mechanism, controls addressable LEDs for visual feedback, operates several relays likely controlling table movement or other effects, and manages a door lock within the "Engineering Corner" environment. It communicates via MQTT for remote control and status updates.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|Stepper Motor Driver|Controls the printer feed motor|26 (Step), 27 (Direction)|
|Front Brake Beam|Input Sensor for printer feed|23|
|Back Brake Beam|Input Sensor for printer feed|25|
|Addressable LED Strip|WS2812B LEDs for visual status|18 (Data)|
|Relay 1|Controls Output Device 1 (Up)|12|
|Relay 2|Controls Output Device 1 (Down)|13|
|Relay 3|Controls Output Device 2 (Up)|14|
|Relay 4|Controls Output Device 2 (Down)|15|
|Relay 5|Controls Output Device 3 (Up)|16|
|Relay 6|Controls Output Device 3 (Down)|17|
|Door Lock Relay|Controls the door lock mechanism|19|
|Door Close Sensor (?) |Input Sensor (Purpose unclear from code)|21|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Engineering/CornerController/Control/`

### Published Topics/Channels

- `/Renegade/Engineering/` (General status/announcements)
- `/Renegade/Engineering/CornerController/` (Specific device status - currently unused in provided code excerpt)
- `/Renegade/Engineering/CornerController/Health/` (Health metrics like Free Memory, RSSI, WiFi Channel)

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`quit`|Stops the current motor action (`feedIn` or `feedOut`) if `modeActive` is true.|Motion|
|`feedIn`|Activates the stepper motor to feed inwards until the front beam sensor is triggered.|Motion|
|`feedOut`|Activates the stepper motor to feed outwards for a fixed distance (50000 steps).|Motion|
|`reset`|Publishes a reset request message.|System|
|`solve`|Publishes a solve message and sets `mqttSolve` flag.|System|
|`green`|Sets LED strip to green.|LED Control|
|`blue`|Sets LED strip to blue.|LED Control|
|`bluegreen`|Sets LED strip to green (Intended Blue/Green? Check code).|LED Control|
|`greenblue`|Sets LED strip to blue (Intended Green/Blue? Check code).|LED Control|
|`redblue`|Sets LED strip to blue (Intended Red/Blue? Check code).|LED Control|
|`redgreen`|Sets LED strip to red (Intended Red/Green? Check code).|LED Control|
|`red`|Sets LED strip to red.|LED Control|
|`off`|Turns LED strip off.|LED Control|
|`oneUp`|Activates Relay 1 (HIGH) and deactivates Relay 2 (LOW) for a timed duration.|Relay Control|
|`oneDown`|Activates Relay 2 (HIGH) and deactivates Relay 1 (LOW) for a timed duration.|Relay Control|
|`twoUp`|Activates Relay 3 (HIGH) and deactivates Relay 4 (LOW) for a timed duration.|Relay Control|
|`twoDown`|Activates Relay 4 (HIGH) and deactivates Relay 3 (LOW) for a timed duration.|Relay Control|
|`threeUp`|Activates Relay 5 (HIGH) and deactivates Relay 6 (LOW) for a timed duration.|Relay Control|
|`threeDown`|Activates Relay 6 (HIGH) and deactivates Relay 5 (LOW) for a timed duration.|Relay Control|
|`doorUnlock`|Activates the Door Lock Relay (HIGH).|Relay Control|
|`doorLock`|Deactivates the Door Lock Relay (LOW).|Relay Control|


## Component Configuration: Addressable LEDs

- **Type:** WS2812B
- **Data Pin(s):** 18
- **Number of Units:** 38 LEDs
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 150
- **Notes:** Color values set using CHSV (Hue, Saturation, Value). Brightness is low (30/255) in color functions.

## Component Configuration: Stepper Motor

- **Driver/Library:** AccelStepper (FULL2WIRE)
- **Pin(s):** Step = 26, Direction = 27
- **Parameter 1:** Max Speed: 5000 steps/sec
- **Parameter 2:** Acceleration: 5000 steps/sec^2
- **Notes:** Used for printer feed mechanism. `feedIn` uses front brake beam (Pin 23) for stopping. `feedOut` moves a fixed distance. `modeActive` prevents concurrent motor commands. `quit` command can interrupt movement.

## Component Configuration: Relays

- **Type:** Assumed 5V Relay Modules based on direct ESP32 pin control.
- **Activation Logic:** Active HIGH (digitalWrite(pin, HIGH) activates).
- **Notes:** Relays 1-6 operate in pairs (Up/Down) and automatically turn off after a duration defined by the `relay` variable (10000ms). The `doorLock` relay (Pin 19) does not have an automatic timer.

## Additional Notes / Important Behaviors

- The system uses a `modeActive` flag to prevent conflicting motor commands (`feedIn`, `feedOut`).
- The `quit` command only works to stop motor movement when `modeActive` is true.
- Relays 1 through 6 automatically deactivate after 10 seconds (`relay` variable).
- The `doorLock` relay stays in the commanded state (HIGH for unlock, LOW for lock) until a new command is received.
- Pin 21 (`doorClose`) is defined but not used in the provided code excerpt.