# Arduino Projects Workspace

This workspace contains Arduino projects for ESP32 microcontrollers, primarily focused on interaction with sensors, displays, LEDs, and MQTT communication.

## Projects

### 1. Communication Station RFID (`bricen_CommsStationRFID_V4_9`)

**Description:**
This project manages multiple MFRC522 RFID readers connected to an ESP32. It controls WS2812B LED strips (via FastLED) to indicate reader status and card presence. The system communicates heavily via MQTT for control commands and status reporting, including an "All Active" mode where all reader LEDs turn blue. It also includes logic for controlling outputs like door locks and actuators based on MQTT messages.

**Hardware Requirements:**
*   ESP32 Development Board
*   Multiple MFRC522 RFID Readers (configured for `NR_OF_READERS = 2` in the code)
*   WS2812B LED Strip (`NUM_LEDS = 76`, `DATA_PIN = 21`)
*   Appropriate power supply for ESP32 and LEDs
*   Outputs connected to pins: `entryDoorMaglock` (13), `leftSlideDoor` (14), `rightSlideDoor` (15), `presentBriefcase` (16), `retractBriefcase` (17)
*   Input button on pin: `missionControlButton` (33)

**Libraries Used:**
*   `FastLED`
*   `PubSubClient` (MQTT)
*   `WiFi`
*   `SPI`
*   `MFRC522`
*   `ESPmDNS`
*   `WiFiUdp`
*   `ArduinoOTA`

**Key Features:**
*   Reads card IDs from multiple RFID readers.
*   Controls LED segments associated with each reader based on card presence and type, or MQTT commands.
*   Supports an "RFID All Active" mode via MQTT (`rfidAllActive`/`rfidAllDeactive`).
*   Supports selecting active RFID positions via MQTT (`/Renegade/Room1/CommsRFID/ActivePositions/`).
*   MQTT control for various functions (quit, hints, RFID modes, door/briefcase control).
*   MQTT status reporting (online, watchdog, card data, health metrics like free memory, RSSI, WiFi channel, connected BSSID).
*   OTA (Over-The-Air) update capability.
*   Connects to the strongest WiFi AP matching the configured SSID.
*   LED animations for hints and alerts.

**MQTT Topics (Primary):**
*   **Publish Base:** `/Renegade/Room1/` (configurable `mainPublishChannel`)
*   **Data Publish:** `/Renegade/Room1/CommsRFID/` (card reads)
*   **Health Publish:** `/Renegade/Room1/CommsRFID/Health/` (status, memory, WiFi info)
*   **Watchdog Publish:** `/Renegade/Room1/` (watchdog messages)
*   **Control Subscribe:** `/Renegade/Room1/CommsRFID/Control/` (commands like `quit`, `rfidAllActive`, door control, etc.)
*   **Active Positions Subscribe:** `/Renegade/Room1/CommsRFID/ActivePositions/` (e.g., "1,0" to activate reader 0, deactivate reader 1)

**Configuration:**
*   WiFi SSID/Password (`ssid`, `password`)
*   MQTT Broker IP (`mqtt_server`)
*   MQTT Topics (various `#define` and `const char*`)
*   Pin definitions (`DATA_PIN`, `SS_x_PIN`, `rstPin`, output pins)
*   Number of LEDs (`NUM_LEDS`) and RFID Readers (`NR_OF_READERS`)
*   Host name (`hostName`) for OTA and identification.

---

### 2. EPO Keypad Box (`bricen_epo7`)

**Description:**
This project implements an Emergency Power Off (EPO) style input box using an ESP32. It features a 4x4 keypad for code entry, an SSD1306 OLED display (via U8g2) for user feedback, and monitors multiple Hall effect sensors. Status and control are managed via MQTT.

**Hardware Requirements:**
*   ESP32 Development Board
*   4x4 Matrix Keypad (connected to `rowPins`, `colPins`)
*   SSD1306 I2C OLED Display
*   Hall Effect Sensors (connected to `hallPins`)
*   WS2812B LED (optional, `NUM_LEDS = 1`, `DATA_PIN = 5`)
*   Appropriate power supply

**Libraries Used:**
*   `Keypad`
*   `U8g2lib`
*   `PubSubClient` (MQTT)
*   `WiFi`
*   `SPI`
*   `ESPmDNS`
*   `WiFiUdp`
*   `ArduinoOTA`
*   `FastLED`

**Key Features:**
*   Accepts 4-digit code input via keypad.
*   Displays input prompts (shapes) and entered digits on the OLED screen.
*   Validates entered code against `correctCode`.
*   Displays "AUTHORIZED" or "DENIED" messages on the screen.
*   Monitors Hall effect sensors and publishes state changes ("triggered"/"released") to unique MQTT topics.
*   Can be enabled/disabled/reset via MQTT commands (`enableEPO`, `disableEPO`, `resetEPO`).
*   Publishes confirmation ("EPO puzzle solved") upon correct code entry.
*   MQTT status reporting (online, watchdog, health metrics).
*   OTA (Over-The-Air) update capability.
*   Connects to the strongest WiFi AP matching the configured SSID.
*   Uses a single FastLED for basic status indication.

**MQTT Topics (Primary):**
*   **Publish Base:** `/Renegade/Room1/` (configurable `mainPublishChannel`)
*   **Data Publish:** `/Renegade/Engineering/Test/` (configurable `dataChannel`) - Used for puzzle solved message.
*   **Health Publish:** `/Renegade/Engineering/Test/Health/` (status, memory, WiFi info)
*   **Watchdog Publish:** `/Renegade/Engineering/Test/` (watchdog messages)
*   **Control Subscribe:** `/Renegade/Room1/EPO/` (commands like `quit`, `enableEPO`, `disableEPO`, `resetEPO`)
*   **Hall Sensor Publish:** `/Renegade/Room1/EPO/<hostname>/hall_sensor/<pin_number>` (e.g., `/Renegade/Room1/EPO/Esp32_EPObox/hall_sensor/12`)

**Configuration:**
*   WiFi SSID/Password (`ssid`, `password`)
*   MQTT Broker IP (`mqtt_server`)
*   MQTT Topics (various `#define` and `const char*`)
*   Keypad Row/Column Pins (`rowPins`, `colPins`)
*   Hall Sensor Pins (`hallPins`)
*   Correct Code (`correctCode`)
*   Host name (`hostName`) for OTA and identification.
*   FastLED Pin (`DATA_PIN`)

---

## General Notes

*   **WiFi & MQTT:** Both projects require configuration of WiFi credentials and the MQTT broker address. They include logic to connect to the strongest available access point matching the configured SSID and attempt to reconnect if the connection is lost.
*   **OTA Updates:** Both projects are configured for ArduinoOTA, allowing wireless firmware updates over the network using the Arduino IDE or platformio. The `hostName` variable is important for identifying the device during OTA.
*   **Dependencies:** Ensure all listed libraries are installed in your Arduino IDE or platformio environment.