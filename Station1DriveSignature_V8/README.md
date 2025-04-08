# Project Overview: Station 1 Drive Signature

## Purpose

This controller manages the "Drive Signature" puzzle at Station 1. It reads four RFID tags placed on specific readers and checks four corresponding Hall effect sensors to determine if the puzzle components are in the correct positions. It communicates status and puzzle completion via MQTT.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|MFRC522 RFID Reader 1|Reads RFID Tag|SS: 25, RST: 22, SPI: (SCK: 18, MOSI: 23, MISO: 19)|
|MFRC522 RFID Reader 2|Reads RFID Tag|SS: 26, RST: 22, SPI: (SCK: 18, MOSI: 23, MISO: 19)|
|MFRC522 RFID Reader 3|Reads RFID Tag|SS: 27, RST: 22, SPI: (SCK: 18, MOSI: 23, MISO: 19)|
|MFRC522 RFID Reader 4|Reads RFID Tag|SS: 4, RST: 22, SPI: (SCK: 18, MOSI: 23, MISO: 19)|
|Hall Effect Sensor 1|Detects magnetic field presence|32|
|Hall Effect Sensor 2|Detects magnetic field presence|33|
|Hall Effect Sensor 3|Detects magnetic field presence|34|
|Hall Effect Sensor 4|Detects magnetic field presence|35|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Room1/Station1DriveSignature/Control/`

### Published Topics/Channels

- `/Renegade/Room1/` (Main channel for general messages like solve state)
- `/Renegade/Room1/Station1DriveSignature/` (General status messages for this controller)
- `/Renegade/Room1/Station1DriveSignature/Health/` (Health metrics like free memory, RSSI)
- `/Renegade/Room1/Station1DriveSignature/RFID/` (Current state of all RFID readers)
- `/Renegade/Room1/Station1DriveSignature/HallFX/` (Current state of all Hall effect sensors)
- `/Renegade/Room1/Station1Buttons/Control/` (Sends commands to the Station 1 Buttons controller)

### Message Commands

|**Command**|**Action**|**Category (Optional)**|
|:--|:--|:--|
|`positionPuzzleMode`|Starts the main puzzle logic loop.|Mode Control|
|`quit`|Exits the current active mode (e.g., `positionPuzzleMode`).|Mode Control|
|`ResetRfid`|Forces a re-initialization of all RFID readers.|System|
|`setUpperBound`|Initiates calibration sequence to determine the upper reading bounds for Hall effect sensors (no magnet present).|Calibration|
|`setLowerBound`|Initiates calibration sequence to determine the lower reading bounds (magnet present) and calculates thresholds.|Calibration|
|`myNewMode`|Placeholder for future modes.|Mode Control|


## Component Configuration: MFRC522 RFID Readers

- **Type:** MFRC522
- **Data Pin(s):** SS Pins: 25, 26, 27, 4. Shared SPI: SCK=18, MOSI=23, MISO=19. Shared RST=22.
- **Number of Units:** 4
- **Configuration Detail 1:** Reads byte 0 from block 4 of MIFARE Ultralight tags to identify the tag.
- **Notes:** The correct sequence of tags required is {1 (Red), 4 (Yellow), 3 (Blue), 2 (Green)} corresponding to readers 0-3. Publishes the combined state (e.g., "1432") to the RFID topic.

## Component Configuration: Hall Effect Sensors

- **Type:** Analog Hall Effect Sensor
- **Pin(s):** 32, 33, 34, 35
- **Number of Units:** 4
- **Configuration Detail 1:** Uses analog readings compared against calculated `thresholds`.
- **Configuration Detail 2:** Readings are averaged over 20 samples.
- **Notes:** A reading *below* the threshold indicates the magnet is present (state 1). A reading *above* indicates no magnet (state 0). Calibration commands (`setUpperBound`, `setLowerBound`) are used to determine thresholds. Publishes the combined state (e.g., "1111") to the HallFX topic.

## Additional Notes / Important Behaviors

- The primary mode is `positionPuzzleMode`.
- The puzzle is solved only when the correct RFID tag sequence (`correctInput`) is present AND all Hall effect sensors detect a magnet (`hallInput` state is 1 for all).
- The Hall effect sensors determine the drive's direction (magnet present/absent), requiring calibration. The RFID readers determine the color/identity of the placed object.
- Upon solving, it publishes `Drive Signature Solved` to the main room channel and tells the `Station1Buttons` controller to enter `sequenceMode`.
- The `wait()` function includes calls to `handleAll()` to maintain MQTT and OTA connectivity during delays.
- Relies on the `Station1Buttons` controller for subsequent puzzle steps after solving.