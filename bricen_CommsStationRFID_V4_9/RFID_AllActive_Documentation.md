# RFID All Active Mode - Implementation Documentation

## Overview
The RFID All Active Mode is a feature that allows all RFID reader LEDs to be set to a solid blue color, indicating that all readers are active regardless of card presence or absence.

## Changes Made to Code
- Added `rfidAllActive` boolean flag to track the state
- Added MQTT callback handlers for "rfidAllActive" and "rfidAllDeactive" commands
- Created `updateRfidAllActive()` function to turn RFID reader LEDs blue
- Modified the `loop()` function to check for rfidAllActive state
- Updated `updateLEDsBasedOnRFID()` to skip LED updates when in all-active mode

## Node-RED Setup for Testing

### Create the following inject nodes:

1. **Enable RFID All Active Mode:**
   - Type: Inject node
   - Payload: `rfidAllActive`
   - Topic: (leave blank)
   - Connect to: MQTT Out node

2. **Disable RFID All Active Mode:**
   - Type: Inject node
   - Payload: `rfidAllDeactive`
   - Topic: (leave blank)
   - Connect to: MQTT Out node

3. **MQTT Out Node:**
   - Server: Your MQTT broker (192.168.86.102)
   - Topic: `/Renegade/Room1/CommsRFID/Control/`
   - QoS: 0
   - Retain: false

4. **Optional Debug Node:**
   - Add a debug node subscribed to `/Renegade/Room1/CommsRFID/Health/` to see confirmation messages

## Testing Procedure

1. Deploy your Node-RED flow
2. Click the "Enable RFID All Active Mode" inject node
3. Verify all RFID reader LEDs turn blue (hue 160)
4. Check that a confirmation message appears in the Health channel
5. Test that the blue state persists even when RFID cards are present/removed
6. Click the "Disable RFID All Active Mode" inject node
7. Verify LEDs return to normal behavior based on card states

## Technical Details

When "rfidAllActive" is received via MQTT:
- The system sets the `rfidAllActive` flag to true
- All RFID reader LEDs are set to blue (hue 160)
- Normal LED behavior based on card presence is suspended
- A confirmation message is sent to the Health channel

When "rfidAllDeactive" is received:
- The system sets the `rfidAllActive` flag to false
- LED behavior returns to normal operation based on card presence
- A confirmation message is sent to the Health channel

## Troubleshooting

If the RFID All Active Mode is not working as expected:
1. Check MQTT connectivity
2. Verify the correct topics are being used
3. Ensure the Arduino code has been properly updated with the new functionality
4. Monitor the Health channel for diagnostic messages
