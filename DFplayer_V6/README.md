# Project Overview: DFplayer_V6

## Purpose

This project utilizes an ESP8266 microcontroller to control a DFPlayer Mini MP3 module. It connects to a WiFi network and an MQTT broker, allowing remote control of audio playback (play, pause, stop, volume, folder selection, interrupt tracks) via MQTT messages.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|DFPlayer Mini MP3|Audio playback module|RX: D2, TX: D1 (via SoftwareSerial)|
|DFPlayer BUSY Pin|Indicates if audio is playing|D5 (Input)|
|ESP8266|Microcontroller|N/A|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/SFX2/Play/`
- `/Renegade/SFX2/Pause/`
- `/Renegade/SFX2/Resume/`
- `/Renegade/SFX2/Stop/`
- `/Renegade/SFX2/Volume/`
- `/Renegade/SFX2/Folder/`
- `/Renegade/SFX2/Interrupt/`

### Published Topics/Channels

- `/Renegade/SFX2/Status/`

### Message Commands

|**Command Topic**|**Payload**|**Action**|**Category**|
|:--|:--|:--|:--|
|`/Renegade/SFX2/Play/`|`{trackNumber}`|Plays the specified track number from the current folder.|Playback|
|`/Renegade/SFX2/Pause/`|*(ignored)*|Pauses the current playback.|Playback|
|`/Renegade/SFX2/Resume/`|*(ignored)*|Resumes the paused playback.|Playback|
|`/Renegade/SFX2/Stop/`|*(ignored)*|Stops the current playback.|Playback|
|`/Renegade/SFX2/Volume/`|`{volumeLevel (0-30)}`|Sets the playback volume.|Configuration|
|`/Renegade/SFX2/Folder/`|`{folderNumber}`|Sets the active folder for subsequent `Play` commands.|Configuration|
|`/Renegade/SFX2/Interrupt/`|`{trackNumber}`|Plays the specified track number from the 'advert' folder, interrupting current playback if necessary.|Playback|
|`/Renegade/SFX2/Interrupt/`|`Stop`|Stops the currently playing interrupt/advertisement track.|Playback|

## Component Configuration: DFPlayer Mini MP3

- **Type:** DFPlayer Mini MP3 Module
- **Library:** DFMiniMp3
- **Data Pin(s):** RX=D2, TX=D1 (SoftwareSerial)
- **Status Pin:** D5 (Connected to DFPlayer BUSY pin)
- **Storage:** SD Card
- **Notes:** Audio files should be organized in numbered folders (e.g., /01, /02) and named sequentially (e.g., 001.mp3, 002.mp3). Interrupt tracks are expected in an 'advert' folder (e.g., /advert/001.mp3). Default volume is set to 30 on startup.

## Additional Notes / Important Behaviors
- The `playing` status variable is updated based on the D5 pin state but is not currently used for external reporting.