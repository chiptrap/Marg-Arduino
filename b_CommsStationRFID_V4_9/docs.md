# CommsStationRFID Documentation

**Name/Version:** CommsStationRFID_V4_9

**Purpose:** Reads up to two MFRC522 RFID readers, controls outputs for slide doors, a maglock, and a briefcase motor based on RFID input and MQTT commands. Manages LED strip animations for RFID feedback, hint indication, and joystick alerts. Communicates status and RFID data via MQTT. Includes OTA update capability.

**Physical Connections:**

*   **RFID Reader #1:** SS_0_PIN=25, RST=32
*   **RFID Reader #2:** SS_1_PIN=26, RST=32
*   **RFID Reader #3:** SS_2_PIN=27 (Defined but `NR_OF_READERS` is 2)
*   **Outputs:**
    *   Entry Door Maglock: *entryDoorMaglock* 13
    *   Left Slide Door: *leftSlideDoor* 14
    *   Right Slide Door: *rightSlideDoor* 15
    *   Briefcase Door *presentBriefcase* 16
    *   Retract Briefcase Motor: 17
*   **Input:**
    *   Mission Control Button (Hint): *minssionControlButton* 33 (INPUT_PULLUP)
*   **LED Strip:** DATA_PIN=21, NUM_LEDS=76 (WS2812B GRB)

**Unique Logic & Functions:**

*   **`callback(char* topic, byte* payload, unsigned int length)`:** Handles incoming MQTT messages. Commands include:
    *   `quit`: Exits the current active mode (though no specific modes are implemented beyond the main loop).
    *   `Hint sent`: Resets the `hint` flag.
    *   `RfidFlash`: Sets `RfidRandom` to false, enabling fixed LED patterns based on RFID state.
    *   `RfidRandom`: Sets `RfidRandom` to true, enabling random LED patterns on reader segments.
    *   `ResetRfid`: Triggers manual re-initialization of RFID readers.
    *   `/Renegade/Room1/CommsRFID/ActivePositions/`: Updates `activePositions` array based on comma-separated '1's and '0's.
    *   `rfidAllActive`: Sets `rfidAllActive` flag to true, forcing all reader LEDs blue.
    *   `rfidAllDeactive`: Sets `rfidAllActive` flag to false.
    *   `openRightSlide`/`closeRightSlide`: Controls right door output and `alertJoy` state.
    *   `openLeftSlide`/`closeLeftSlide`: Controls left door output.
    *   `presentBriefcase`/`retractBriefcase`: Controls briefcase motor outputs (with 6-second activation).
    *   `engageMaglock`/`releaseMaglock`: Controls maglock output.
    *   `joyOnline`: Clears the `alertJoy` flag.
*   **`readAllRFID()`:** Loops through configured readers (`NR_OF_READERS`). Reads card UIDs if present, stores them in `currentRFIDinput[]`. Handles reader timeouts by re-initializing SPI and the specific reader. Publishes the state of all readers (`currentRFIDinput`) to `dataChannel` on change. Updates LEDs via `updateLEDsBasedOnRFID()` if `RfidRandom` is false.
*   **`updateLEDsBasedOnRFID()`:** Sets LED patterns for reader segments (0-7, 8-15) based on `currentRFIDinput[]` and `activePositions[]`. Skips LEDs 16-19.
    *   Active & Card 1-4: 4 Red, 4 Green
    *   Active & Card 5: 8 Green
    *   Active & Card > 5: 4 Green, 4 Red
    *   Active & No Card: 8 Red
    *   Inactive: 8 Off (or White based on comment vs code - code sets value to 90, saturation to 0 -> White)
*   **`getMemoryContent(...)`:** Reads a specific byte from a specified block on an RFID card (defaults to block 4, index 0). Handles authentication.
*   **`serviceHint()`:** State machine controlling blinking animation for the hint button LEDs (28-31). Triggered by `hint` flag.
*   **`serviceAlertJoy()`:** State machine controlling blinking animation for the joystick alert LEDs (16-19). Triggered by `alertJoy` flag.
*   **`updateRfidAllActive()`:** Sets LEDs 0-15 to solid blue when `rfidAllActive` is true.
*   **`loop()`:** Main loop. Calls `readAllRFID()`, `handleAll()`. Manages state flags (`hint`, `alertJoy`, `RfidRandom`, `rfidAllActive`) and calls corresponding service/update functions. Handles mission control button press to trigger hint state. Runs random LED patterns if `RfidRandom` is true.

**MQTT Topics:**

*   **Publish:**
    *   `/Renegade/Room1/CommsRFID/` (`dataChannel`): RFID reader states (e.g., "12 0").
    *   `/Renegade/Room1/CommsRFID/Health/` (`dataChannel2`): Free memory, RSSI, WiFi channel, connection info, reader timeout/reinit messages, RFID active status.
    *   `/Renegade/Room1/` (`mainPublishChannel`): Online/quit messages, "Hint button pressed".
    *   `/Renegade/Room1/` (`watchdogChannel`): Watchdog messages.
*   **Subscribe:**
    *   `/Renegade/Room1/CommsRFID/Control/`
    *   `/Renegade/Room1/CommsRFID/ActivePositions/`

**Standard Header/Footer:** Uses standard “WiFi, MQTT, OTA” setup. Includes `connectToStrongestWiFi` logic. Health metrics are published periodically. `usingSpiBus` flag controls SPI re-initialization during MQTT reconnect.

**Maintenance Notes:**

*   Pressing the mission control button publishes "Hint button pressed" to `mainPublishChannel` and triggers the `hint` LED state machine.
*   RFID reader timeouts are automatically handled by re-initializing SPI and the specific reader, with a notification published to `dataChannel2`. Manual reset via MQTT `ResetRfid` is also possible.
*   LED behavior depends on `RfidRandom` and `rfidAllActive` flags, controlled via MQTT.
*   `NR_OF_READERS` is set to 2, but pins and arrays are defined for 3 readers.
*   LED segments: Reader 1 (0-7), Reader 2 (8-15), Joystick Alert (16-19), Reader 3 (unused? 20-27), Hint Button (28-31). Total LEDs defined is 76.
*   The `updateLEDsBasedOnRFID` function skips LEDs 16-19.