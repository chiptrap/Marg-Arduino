# CommsStationEncoderAndJoystick Documentation

**Name/Version:** CommsStationEncoderAndJoystick_V5

**Purpose:** Reads input from one rotary encoder and one 4-way joystick with a center button. Controls a WS2812B LED strip based on encoder/joystick state and MQTT commands. Communicates status and input data via MQTT. Includes OTA update capability.

**Physical Connections:**

*   **Encoder #1:** CLK=26, DT=27
*   **Joystick #1:** UP=19, DOWN=21, LEFT=22, RIGHT=32
*   **Button:** 25
*   **LED Strip:** DATA_PIN=17, NUM_LEDS=39 (WS2812B GRB)

**Unique Logic & Functions:**

*   **`callback(char* topic, byte* payload, unsigned int length)`:** Handles incoming MQTT messages. Commands include:
    *   `quit`: Exits the current active mode.
    *   `zero`: Resets the encoder count to 0.
    *   `encoderPositions`: Publishes the current encoder position.
    *   Numerical values on `/Renegade/Room1/CommsStationEncoder/Control/Lights/`: Triggers specific LED animations (case 0: attention flash).
    *   Numerical values on `/Renegade/Room1/CommsStationJoysticks/Control/`: Sets the `joystickOffset`.
    *   `dumpEncoder`: Enters a mode to continuously publish encoder position changes.
    *   `isTurning`: Enters a mode to publish when the encoder starts/stops turning and the duration.
    *   `directionMode`: Enters the primary operational mode, reading joystick/button and controlling LEDs.
*   **`readJoystick(int joystickID, int offset)`:** Reads the 4 digital inputs for the specified joystick, determines the direction (0-8), and applies an optional rotational offset.
*   **`reportDirection(byte joystickDirection, byte joystickID, boolean rawData)`:** Publishes the joystick direction (e.g., "up", "downLeft", "center") to `dataChannel`.
*   **`encoderPosition(byte encoderID)`:** Reads and publishes the encoder position (0-99) to `dataChannel` only when it changes.
*   **`isTurning(byte encoderID)`:** Detects if the encoder is actively being turned, publishing start/stop events and duration to `dataChannel`.
*   **`directionMode(int offset)`:** Main loop when `directionMode` is active. Reads joystick/button, reports changes via MQTT, and runs a default LED animation (`fill_solid` with `beatsin8`).
*   **`lightEncoderRing(byte encoderID, CHSV color)`:** Sets the color of the 35 LEDs associated with an encoder ring.
*   **`loop()`:** Default idle loop. Reads encoder, calculates LED gradient based on position, and updates LEDs using `fill_gradient`.

**MQTT Topics:**

*   **Publish:**
    *   `/Renegade/Room1/CommsStationEncoderJoystick/` (`dataChannel`): Encoder position, turning status, joystick direction, button presses.
    *   `/Renegade/Engineering/CommsJoystick/Health/` (`dataChannel2`): Free memory, RSSI, WiFi channel, connection info.
    *   `/Renegade/Room1/` (`mainPublishChannel`): Online/quit messages.
    *   `/Renegade/Room1/` (`watchdogChannel`): Watchdog messages.
*   **Subscribe:**
    *   `/Renegade/Room1/CommsStationEncoder/Control/`
    *   `/Renegade/Room1/CommsStationEncoder/Control/Lights/`
    *   `/Renegade/Room1/CommsStationJoysticks/Control/`
    *   `/Renegade/Room1/CommsStationJoysticks/Request/`

**Standard Header/Footer:** Uses standard “WiFi, MQTT, OTA” setup. Includes `connectToStrongestWiFi` logic to connect to the "Control booth" SSID with the best signal if multiple APs are present. Health metrics (Free Heap, RSSI, WiFi Channel, Connected BSSID) are published periodically.

**Maintenance Notes:**

*   The `joystickOffset` can be adjusted via MQTT to compensate for physical joystick orientation. Default offset is 2 (pins pointing left). See `readJoystick` comments for details.
*   The code enters specific modes (`dumpEncoder`, `isTurning`, `directionMode`) based on MQTT commands and stays in that mode's loop until a "quit" command is received.
*   The default `loop()` function runs a gradient animation on the first 35 LEDs based on encoder position when no specific mode is active.
*   Includes functions (`reportFreeHeap`, `publishRSSI`, `publishWiFiChannel`, `publishWiFiInfo`) to send health and connection status to `dataChannel2`.