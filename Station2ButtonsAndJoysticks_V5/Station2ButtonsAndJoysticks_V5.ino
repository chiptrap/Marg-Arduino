#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

//-------------------------------------------------Shift Register-------------------------------------------------
// 165 Shift Register parameters
const int data165 = D6;  /* Q7 */
const int clock165 = D8; /* CP */
const int latch165 = D7;  /* PL */
const int numBits = 16;  /* Set to 8 * number of shift registers */
int buttonStates[numBits];
int arraySize = sizeof(buttonStates) / sizeof(buttonStates[0]);

//For pins on rev 1.0 board
#define I8 0
#define I9 1
#define I10 2
#define I11 3

#define I12 4
#define I13 5
#define I14 6
#define I15 7

#define I0 8
#define I1 9
#define I2 10
#define I3 11

#define I4 12
#define I5 13
#define I6 14
#define I7 15


//595 Shift Register Parameters
const int latch595 = D3;
const int clock595 = D2;
const int data595 = D4;
//-------------------------------------------------Wifi-------------------------------------------------
unsigned long previousMillis = 0;
const long interval = 10000;           // interval at which to send mqtt watchdog (milliseconds)

const char* ssid = "Control booth";
const char* password = "MontyLives";
const char* mqtt_server = "192.168.86.102";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"

//Base notifications
const char* hostName = "Station 2 Joysticks"; // Set your controller unique name here
const char* quitMessage = "Station 2 Joysticks quitting...";
const char* onlineMessage = "Station 2 Joysticks Online";
const char* watchdogMessage = "Station 2 Joysticks Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 2
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station2Joysticks/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station2Joysticks/Control/",
  "/Renegade/Room1/Station2Joysticks/Request/"
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;

PubSubClient client(wifiClient);
boolean modeActive = false;
boolean quit = false;
//-------------------------------------------------FastLED-------------------------------------------------
// FastLED parameters
#define DATA_PIN D1
#define NUM_LEDS 13
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
//-------------------------------------------------Other-------------------------------------------------
#define NUM_JOYSTICKS 3
int currentJoystickInputs[NUM_JOYSTICKS][2] = {
  {0, 0}, //joystick 0; x, y
  {0, 0}, //joystick 1; x, y
  {0, 0}  //joystick 2; x, y
};

int lastJoystickInputs[NUM_JOYSTICKS][2] = {
  {0, 0}, //joystick 0; x, y
  {0, 0}, //joystick 1; x, y
  {0, 0}  //joystick 2; x, y
};

int currentJoystickPositions[NUM_JOYSTICKS] = {0, 0, 0};
int lastJoystickPositions[NUM_JOYSTICKS] = {0, 0, 0};

//Joystick Wire Default:
//Yellow    5V (Switch Common)
//Purple    Up
//White     Down
//Green     Left
//Red/White Right
//As long as 5V is on the outside-most pin, the directions actually don't matter that muchsee offset section under int readJoystick
int joystick[NUM_JOYSTICKS][4] = {
  //{UP,  DOWN, LEFT, RIGHT}
  {I15, I14,  I13,  I12},  //joystick 0 (left)
  {I11, I10,  I9,   I8},   //joystick 1 (middle)
  {I7,  I6,   I5,   I4}    //joystick 2 (right)
};

byte button[3] = {I0, I1, I2};
byte buttonState[3] = {0, 0, 0};
byte lastButtonState[3] = {0, 0, 0};

const byte codeLength = 18;


#define GreenUP     1
#define GreenDOWN   5
#define GreenRIGHT  3
#define GreenLEFT   7

#define BlackUP     9
#define BlackDOWN   13
#define BlackRIGHT  11
#define BlackLEFT   15

#define RedUP       17
#define RedDOWN     21
#define RedRIGHT    19
#define RedLEFT     23


// Joystick 0 inputs:
//             UP
//             1
//             |
//   LEFT 7--- 0 ---3 RIGHT
//             |
//             5
//            DOWN

//Joystick 1 inputs:
//             UP
//              9
//              |
//   LEFT 15--- 0 ---11 RIGHT
//              |
//              13
//            DOWN

//Joystick 2 inputs:
//              UP
//              17
//              |
//   LEFT 23--- 0 ---19 RIGHT
//              |
//              21
//              DOWN
//NOT TO BE CONFUSED WITH DIRECTION INDEXES OUTLINED IN READJOYSTICK FUNCTION
//ONLY USED TO TURN JOYSTICK INTO INPUT METHOD:
//example:
//{1,     9,     17,    15,      11,       23}
// J0 UP, J1 UP, J2 UP, J1 LEFT, J1 RIGHT, J2 LEFT

byte correctArray[codeLength] = {BlackLEFT, RedDOWN, GreenRIGHT, GreenLEFT, RedDOWN, BlackDOWN, BlackRIGHT, RedDOWN, BlackUP, GreenLEFT, RedLEFT, RedDOWN, GreenLEFT, RedRIGHT, GreenDOWN, BlackUP, RedRIGHT, GreenLEFT};
byte inputArray[codeLength];

//-------------------------------------------------Function constructors (for functions containing default parameters)-------------------------------------------------
int readJoystick(int joystickID, int offset = 0);
void reportDirection(byte joystickDirection, byte joystickID, boolean rawData = false);
//-------------------------------------------------End Header-------------------------------------------------

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  int val = message.toInt();
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  if (!strcmp(topic, subscribeChannel[1])) {
    reportDirection(currentJoystickPositions[val], val);
  } else if (message == "quit" && modeActive) {
    client.publish(mainPublishChannel, quitMessage);
    quit = true;
  }

  if (!modeActive) {
    modeActive = true;

    if (message == "sequenceMode") {
      client.publish(mainPublishChannel, "Start ticker tape puzzle");
      while (!quit) {
        sequenceMode();
      }
      client.publish("/Renegade/Room1/Station2Encoders/Control/", "quit");
    } else if (message == "directionMode") {
      while (!quit) {
        handleAll();
        directionMode();
      }
    } else if (message == "myNewMode") {
      //add more modes here
    }

    quit = false;
    modeActive = false;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(500);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  // FastLED Setup
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);

  // 165 Shift Register Setup
  pinMode(data165, INPUT);
  pinMode(clock165, OUTPUT);
  pinMode(latch165, OUTPUT);

  //595 Shift Register Setup
  pinMode(latch595, OUTPUT);
  pinMode(clock595, OUTPUT);
  pinMode(data595, OUTPUT);
}

void loop()
{
  handleAll();
//  fill_solid(leds, NUM_LEDS, CHSV(0, 255, beatsin8(30)));
//  FastLED.show();
}

void sequenceMode() {
  //Start animation mode
  //client.publish("/Renegade/Room1/Station2Encoders/Control/", "tickerPuzzle");
  byte input = 0;
  byte joystickID;
  byte cursorPosition = 0;
  String output = "";

  while (cursorPosition < codeLength) {
    if (quit) {
      return;
    }
    handleAll();
    fadeToBlackBy(leds, NUM_LEDS, 128);
    FastLED.show();

    //read joysticks
    for (int i = 0; i < NUM_JOYSTICKS; i++) {
      currentJoystickPositions[i] = readJoystick(i);
      if (currentJoystickPositions[i] != lastJoystickPositions[i]) {
        //grab the first input
        wait(20);
        joystickID = i;
        input = currentJoystickPositions[i];
        lastJoystickPositions[i] = currentJoystickPositions[i];
        break;
      }
    }

    //if it's a directional input, accept input
    if (input != 0) {
      inputArray[cursorPosition] = input + (joystickID * 8);
      output = output + String(inputArray[cursorPosition]) + " ";

      //Terminate loop and start over if it's wrong
      if (inputArray[cursorPosition] != correctArray[cursorPosition]) {
        client.publish(dataChannel, "Incorrect sequence entered");
        client.publish("/Renegade/Room1/Station2Encoders/Control/", "incorrect");
        //insert led code here
        // Flash between red and black 4 times
        for (int i = 0; i < 4; i++) {
        // Set all LEDs to red
        //fill_solid(leds, NUM_LEDS, CRGB::Red);
        // Fill all LEDs except the first one
        fill_solid(leds + 1, NUM_LEDS - 1, CRGB::Red);

        FastLED.show();     // Update the LEDs
        wait(125);         // Wait for 125 milliseconds

        // Set all LEDs to black
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();     // Update the LEDs
        wait(125);         // Wait for 125 milliseconds
        }
        wait(500);
        return;
      }
      client.publish("/Renegade/Room1/Station2Encoders/Control/", "increment");
      cursorPosition++;

    }

    //while inputs are not centered, refuse to proceed
    while ( (currentJoystickPositions[0] != 0) ||
            (currentJoystickPositions[1] != 0) ||
            (currentJoystickPositions[2] != 0) ) {
      handleAll();
      if (quit) {
        return;
      }

      //read joysticks again to see if they are centered
      for (int i = 0; i < NUM_JOYSTICKS; i++) {
        currentJoystickPositions[i] = readJoystick(i);
        if (currentJoystickPositions[i] != lastJoystickPositions[i]) {
          wait(20);
          lastJoystickPositions[i] = currentJoystickPositions[i];
        }
      }
    }

    input = 0;


  } //while cursorPosition<codeLength


  //If it makes it here, the code was correct.
  client.publish(dataChannel, "Correct sequence entered");
  quit = true;
}

void directionMode() {
  byte wave = beat8(20);
  for (int i = 0; i < NUM_JOYSTICKS; i++) {
    wave = beat8(20);

    //read joysticks
    currentJoystickPositions[i] = readJoystick(i);
    if (currentJoystickPositions[i] != lastJoystickPositions[i]) {
      wait(20);
      reportDirection(currentJoystickPositions[i], i);
      lastJoystickPositions[i] = currentJoystickPositions[i];
    }

    //read buttons
    buttonState[i] = digitalReadRegister(button[i]);
    String buttonMessage = "Button ";
    if (buttonState[i] != lastButtonState[i]) {
      buttonMessage = buttonMessage + String(i);
      if (buttonState[i] == 1) {
        buttonMessage = buttonMessage + " pressed";
        client.publish(dataChannel, buttonMessage.c_str());
      } else {
        buttonMessage = buttonMessage + " released";
      }
      Serial.println(buttonMessage);
      lastButtonState[i] = buttonState[i];
    }


    //lightButton(map(wave, 0, 255, 0, 2), 0, 255, 255);
    //lightButton(i, 0, 255, 255);
    //fadeToBlackBy(leds, NUM_LEDS, 64);
    fill_solid(leds +1, NUM_LEDS -1, CHSV(0, 255, beatsin8(30)));
    FastLED.show();
    if (quit) {
      return;
    }

  }
}

void lightButton(byte buttonToLight, byte h, byte s, byte v) {
  for (int i = 0; i < 4; i++) {
    leds[i + (4 * buttonToLight)+1] = CHSV(h, s, v);
  }
}

void reportDirection(byte joystickDirection, byte joystickID, boolean rawData) {
  String message = "Joystick " + String(joystickID) + " position: ";

  if (rawData) {
    message = String(joystickID);
    Serial.println(message);
    client.publish(dataChannel, message.c_str());
    return;
  }

  switch (joystickDirection) {
    case 1:
      message = message + "up";
      break;
    case 2:
      message = message + "upRight";
      break;
    case 3:
      message = message + "right";
      break;
    case 4:
      message = message + "downRight";
      break;
    case 5:
      message = message + "down";
      break;
    case 6:
      message = message + "downLeft";
      break;
    case 7:
      message = message + "left";
      break;
    case 8:
      message = message + "upLeft";
      break;
    default:
      message = message + "center";
      break;
  }
  Serial.println(message);
  client.publish(dataChannel, message.c_str());
}

int readJoystick(int joystickID, int offset) {
  int x;
  int y;
  int outputDirection = 0; //default center

  //IMPORTANT OFFSET INFO:
  //Default offset is 0, where your joystick pins point DOWNWARDS.
  //To rotate the joystick here, look at which direction your pins are pointing relative to you.
  //If your pins:
  //point left (clockwise rotation 90),   set offset to 2
  //point up   (clockwise rotation 180),  set offset to 4
  //point right (clockwise rotation 270),  set offset to 6

  //
  //             UP
  //             1
  //          8  |  2
  //   LEFT 7--- 0 ---3 RIGHT
  //          6  |  4
  //             5
  //            DOWN
  //
  // outputDirection diagram, type on .ino file, Owen Fang, © 2024
  // All rights reserved

  if (digitalReadRegister(joystick[joystickID][0])) {
    //Up
    y = 1;
  } else if (digitalReadRegister(joystick[joystickID][1])) {
    //Down
    y = -1;
  } else {
    //Center
    y = 0;
  }

  if (digitalReadRegister(joystick[joystickID][2])) {
    //Left
    x = -1;
  } else if (digitalReadRegister(joystick[joystickID][3])) {
    //Right
    x = 1;
  } else {
    //Center
    x = 0;
  }

  currentJoystickInputs[joystickID][0] = x;
  currentJoystickInputs[joystickID][1] = y;


  if ((currentJoystickInputs[joystickID][0] == lastJoystickInputs[joystickID][0])
      && (currentJoystickInputs[joystickID][1] == lastJoystickInputs[joystickID][1])) {
    //if x and y haven't changed, don't bother parsing a new outputDirection value, because it will be the same.
    return outputDirection;
  }

  switch (x) {
    case 1:
      switch (y) {
        case 1:
          //upRight
          outputDirection = 2;
          break;
        case -1:
          //downRight
          outputDirection = 4;
          break;
        default:
          //right
          outputDirection = 3;
          break;
      }
      break;
    case -1:
      switch (y) {
        case 1:
          //upLeft
          outputDirection = 8;
          break;
        case -1:
          //downLeft
          outputDirection = 6;
          break;
        default:
          //left
          outputDirection = 7;
          break;
      }
      break;
    default:
      switch (y) {
        case 1:
          //up
          outputDirection = 1;
          break;
        case -1:
          //down
          outputDirection = 5;
          break;
        default:
          //center
          outputDirection = 0;
          break;
      }
      break;
  }

  if (!offset) {
    //if no offset, don't waste time performing orientation offset
    return outputDirection;
  }

  //perform orientation offsets
  if (outputDirection != 0) {
    //prevent offset on center
    outputDirection = outputDirection + offset;
  }

  if (outputDirection > 8) {
    outputDirection = outputDirection - 8;
  }

  return outputDirection;
}

void PISOreadShiftRegister()
{
  // Step 1: Sample
  digitalWrite(latch165, LOW);
  delayMicroseconds(0.1);
  digitalWrite(latch165, HIGH);

  // Step 2: Shift
  //Serial.print("Bits: ");
  for (int i = 0; i < numBits; i++)
  {
    int bit = digitalRead(data165);
    buttonStates[i] = bit;
    //Serial.print(buttonStates[i]);
    digitalWrite(clock165, HIGH); // Shift out the next bit
    delayMicroseconds(0.1);
    digitalWrite(clock165, LOW);
  }

  //Serial.println();
}

int binaryArrayToInt(int binaryArray[], int size) {
  int result = 0;

  for (int i = 0; i < size; i++) {
    result = (result << 1) | binaryArray[i];
  }
  Serial.println(result);
  return result;
}

void SIPOregisterOutput(int output) {
  // ST_CP LOW to keep LEDs from changing while reading serial data
  digitalWrite(latch595, LOW);
  // Shift out the bits
  shiftOut(data595, clock595, MSBFIRST, output);
  // ST_CP HIGH change LEDs
  digitalWrite(latch595, HIGH);
}

boolean digitalReadRegister(int pinNumber) {
  //pinNumber goes from 0 to (numBits-1)
  boolean output = false;
  PISOreadShiftRegister();
  // output = buttonStates[numBits - 1 - pinNumber];
  output = buttonStates[pinNumber];
  delay(5); //debounce
  return output;
}

void setup_wifi() {

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: % u % % \r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[ % u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    byte mac[5];
    WiFi.macAddress(mac);  // get mac address
    String clientId = String(mac[0]) + String(mac[4]);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish(mainPublishChannel, onlineMessage);//change this to required message
      // ... and resubscribe
      for (int i = 0; i < NUM_SUBSCRIPTIONS; i++) {
        client.subscribe(subscribeChannel[i]);
      }
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      wait(5000);
    }
  }
}

void checkConnection() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    //WiFi.reconnect();
    previousMillis = currentMillis;
  }
  if ((!client.connected() ) && (WiFi.status() == WL_CONNECTED) ) {
    Serial.println("Lost mqtt connection");
    reconnect();
  }
  if ((currentMillis - previousMillis >= interval)) {
    client.publish(watchdogChannel, watchdogMessage);//Change this to correct watchdog info
    previousMillis = currentMillis;
  }
}

void handleAll() {
  ArduinoOTA.handle();
  checkConnection();
  client.loop();
  FastLED.delay(1);
}

void wait(uint16_t msWait) {
  uint32_t start = millis();
  while ((millis() - start) < msWait)
  {
    handleAll();
  }
}
