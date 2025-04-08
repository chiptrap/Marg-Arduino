# Project Overview: Button Wall Controller

## Purpose

This project controls a large interactive button wall composed of 80 buttons, each with 4 integrated WS2812B LEDs. It reads button states using cascaded 74HC165 shift registers (split into two input groups) and provides visual feedback via the LEDs using the FastLED library. The controller supports various interactive games (Connect 4, Twister, Lights Out, Match Pattern, Memory Game) managed via MQTT commands, Over-The-Air (OTA) updates, and reports system health metrics.

## Connected Hardware & Pinout

|**Component**|**Description**|**Pin(s)**|
|:--|:--|:--|
|WS2812B LED Strip|Addressable RGB LEDs (4 per button, 80 buttons total)|DATA=32|
|74HC165 Shift Registers (x10)|Parallel-in, Serial-out shift registers for reading button states|Group 1 (4 Reg): Data=25, Clock=26, Load=27|
|74HC165 Shift Registers (x10)|Parallel-in, Serial-out shift registers for reading button states|Group 2 (6 Reg): Data=23, Clock=26, Load=27|
|ESP32 Dev Module|Microcontroller|Implicit (Controls LEDs, Shift Regs, WiFi/MQTT)|

## Communication Protocol (MQTT)

### Subscribed Topics/Channels

- `/Renegade/Engineering/ButtonWall/` (Game mode selection, Force Quit)
- `/Renegade/Engineering/ButtonWall/Lighting/ButtonToLight/` (Directly light a specific button - likely for testing/debug)
- `/Renegade/Engineering/ButtonWall/Lighting/Hue/` (Set global hue for default animation)
- `/Renegade/Engineering/ButtonWall/Lighting/Fade/` (Set fade rate for default animation)
- `/Renegade/Engineering/ButtonWall/Connect4Difficulty/` (Set AI mistake probability for Connect 4)
- `/Renegade/Engineering/ButtonWall/TwisterPlayerCount/` (Set number of players for Twister)
- `/Renegade/Engineering/ButtonWall/TwisterRounds/` (Set number of rounds for Twister)

### Published Topics/Channels

- `/Renegade/Engineering/` (General status: Online, Watchdog, Game status, Button presses in default mode, Errors)
- `/Renegade/Engineering/ButtonWall/Health/` (System health: Free Memory, RSSI, WiFi Channel, WiFi Info)
- `/Renegade/Engineering/ButtonWall/TwisterData/` (Twister game state/button presses)

### Message Commands

|**Command**|**Action**|**Topic**|**Category (Optional)**|
|:--|:--|:--|:--|
|`Play Connect 4 (AI)`|Starts Connect 4 game against the AI.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Play Connect 4 (2P)`|Starts 2-player Connect 4 game.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Play Twister`|Starts Twister game.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Play Lights Out`|Starts Lights Out game.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Play Match Pattern`|Starts Match Pattern game.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Play Memory Game`|Starts Memory Game.|`/Renegade/Engineering/ButtonWall/`|Game Mode|
|`Exit Mode (Forcequit)`|Forces the current game mode to end.|`/Renegade/Engineering/ButtonWall/`|System|
|`Twister round bypass`|Skips the current round check in Twister.|`/Renegade/Engineering/ButtonWall/`|Game Control|
|`{integer value}`|Sets Connect 4 AI mistake probability (0-100).|`/Renegade/Engineering/ButtonWall/Connect4Difficulty/`|Game Config|
|`{integer value}`|Sets Connect 4 AI search depth.|`/Renegade/Engineering/ButtonWall/Connect4Depth/`|Game Config|
|`{integer value}`|Sets Connect 4 AI play depth before mistakes.|`/Renegade/Engineering/ButtonWall/Connect4PlayDepth/`|Game Config|
|`{integer value}`|Sets number of rounds for Twister.|`/Renegade/Engineering/ButtonWall/TwisterRounds/`|Game Config|
|`{integer value}`|Sets number of players for Twister.|`/Renegade/Engineering/ButtonWall/TwisterPlayerCount/`|Game Config|
|`{integer value}`|Sets fade rate (1-255) for default animation.|`/Renegade/Engineering/ButtonWall/Lighting/Fade/`|LED Control|
|`{integer value}`|Sets global hue (0-255) for default animation.|`/Renegade/Engineering/ButtonWall/Lighting/Hue/`|LED Control|
|`{integer value}`|Lights a specific button (0-79) - likely debug.|`/Renegade/Engineering/ButtonWall/Lighting/ButtonToLight/`|LED Control|

## Component Configuration: Addressable LEDs (FastLED)

- **Type:** WS2812B
- **Data Pin(s):** 32
- **Number of Units:** 320 LEDs (80 buttons * 4 LEDs/button)
- **Configuration Detail 1:** Color Order: GRB
- **Configuration Detail 2:** Brightness: 255 (Max)
- **Configuration Detail 3:** Max Power: 5V, 10000mA
- **Notes:** Controlled via FastLED library. Used for game feedback and animations.

## Component Configuration: Shift Registers (74HC165)

- **Driver/Library:** Custom GPIO reading logic
- **Pin(s):** Load=27, Clock=26, Data1(4 Reg)=25, Data2(6 Reg)=23
- **Number of Units:** 10 (read as 80 individual bits)
- **Notes:** Reads the state of 80 buttons. Input is split across two data pins (Data1 for first 16 bits, Data2 for remaining 64 bits).

## Additional Notes / Important Behaviors

- **Game Modes:** Supports Connect 4 (vs AI or 2-Player), Twister, Lights Out, Match Pattern, and Memory Game.
- **Default Mode:** When no game is active, pressing a button lights it red and publishes the button number via MQTT. An idle animation runs otherwise.
- **OTA Updates:** Supports ArduinoOTA for wireless firmware updates.
- **WiFi Connection:** Scans for available networks and connects to the strongest one matching the predefined SSID.
- **MQTT Reconnect:** Automatically attempts to reconnect to the MQTT broker if the connection is lost.
- **Health Monitoring:** Periodically publishes Free Heap memory, WiFi RSSI, and WiFi Channel via MQTT.
- **Force Quit:** An MQTT command (`Exit Mode (Forcequit)`) can terminate the currently active game mode.
- **Shift Register Reading:** Uses a custom function (`PISOreadShiftRegister`) adapted for the two separate data input pins.
- **Responsiveness:** Uses a custom `wait()` function incorporating `handleAll()` (OTA, MQTT loop, Connection Check) to remain responsive during delays.