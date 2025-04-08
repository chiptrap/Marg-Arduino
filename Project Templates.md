# Project Overview: {Project Name}

## Purpose

{Briefly describe the purpose and function of this custom controller/project. What problem does it solve or what role does it play in the overall system or escape room?}

## Connected Hardware & Pinout

{List all connected hardware components, their function/description, and the specific pin(s) they connect to on the controller board.}

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|{Component Name 1}|{Brief Description of Comp 1}|{Pin(s) used, e.g., 12, A0}|
|{Component Name 2}|{Brief Description of Comp 2}|{Pin(s) used, e.g., 26(S), 27(D)}|
|{Component Name 3}|{Brief Description of Comp 3}|{Pin(s) used}|
|{Input Sensor 1}|{Type of Sensor, e.g., IR Beam}|{Pin used}|
|{Input Sensor 2}|{Type of Sensor, e.g., Mag Sw}|{Pin used}|
|{Output Device 1}|{Type of Device, e.g., Lock}|{Pin used}|
|{Output Device 2 - Relay A}|{Control Purpose, e.g., Motor Up}|{Pin used}|
|{Output Device 2 - Relay B}|{Control Purpose, e.g., Motor Dn}|{Pin used}|
|...|...|...|

## Communication Protocol ({Protocol Name, e.g., MQTT, Serial})

### Subscribed Topics/Channels

- `{Primary control topic/channel the controller listens to}`
- `{Optional: Other subscribed topics}`

### Published Topics/Channels

- `{Primary status/feedback topic the controller publishes to}`
- `{Optional: Health check topic}`
- `{Optional: General system topic}`

### Message Commands

{List all commands the controller accepts, categorized by function if helpful, and describe the action each command performs.}

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`{commandName1}`|{Description of what command 1 does}|{e.g., Motion}|
|`{commandName2}`|{Description of what command 2 does}|{e.g., Output}|
|`{setModeX}`|{Description of what setting mode X does}|{e.g., LED Control}|
|`{setModeY}`|{Description of what setting mode Y does}|{e.g., LED Control}|
|`{triggerActionA}`|{Description of action A (e.g., Unlock Door)}|{e.g., Relay Control}|
|`{triggerActionB}`|{Description of action B (e.g., Lock Door)}|{e.g., Relay Control}|
|`{systemCommand1}`|{Description of system command (e.g., Reset)}|{e.g., System}|
|`{systemCommand2}`|{Description of system command (e.g., Status Req)}|{e.g., System}|
|...|...|...|


## Component Configuration: {Component Type 1, e.g., Addressable LEDs}

- **Type:** {e.g., WS2812B, APA102, Generic RGB}
- **Data Pin(s):** {Primary Pin, Optional Secondary Pin}
- **Number of Units:** {e.g., 38 LEDs, 1 Display}
- **Configuration Detail 1:** {e.g., Color Order: GRB}
- **Configuration Detail 2:** {e.g., Brightness: 150}
- **Configuration Detail 3:** {e.g., Voltage: 5V}
- **Notes:** {Any specific configuration notes, e.g., Color values set using HSV}

## Component Configuration: {Component Type 2, e.g., Stepper Motor}

- **Driver/Library:** {e.g., AccelStepper (FULL2WIRE), A4988, Standard Servo Lib}
- **Pin(s):** {e.g., Step = 26, Direction = 27, Enable = 14, Signal = 9}
- **Parameter 1:** {e.g., Max Speed: 5000 steps/sec}
- **Parameter 2:** {e.g., Acceleration: 5000 steps/sec^2}
- **Parameter 3:** {e.g., Microstepping: 1/16}
- **Notes:** {Describe the component's role and any specific operational logic, e.g., Reacts to sensor on Pin X for position awareness.}

## Component Configuration: {Component Type 3, e.g., Relays}

- **Type:** {e.g., 5V Mechanical Relay Module, Solid State Relay}
- **Activation Logic:** {e.g., Active LOW, Active HIGH}
- **Voltage Switched:** {e.g., 12V DC, 120V AC}
- **Notes:** {Any specific behaviors, e.g., Automatic reset/turn-off timer}

## Additional Notes / Important Behaviors

{List any other important operational details, default behaviors on startup, safety mechanisms, timing considerations, or miscellaneous notes relevant to the project.}

- {Note 1, e.g., System performs self-check on boot.}
- {Note 2, e.g., Debounce time for inputs is 50ms.}
- {Note 3, e.g., Specific power supply requirements.}