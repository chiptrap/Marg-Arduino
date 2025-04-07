Com 
#include <ESP32Encoder.h> // https://github.com/madhephaestus/ESP32Encoder.git 
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFi.h>
//#include <SPI.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//-----------------------------------------------------------WiFi,MQTT,OTA-----------------------------------------------------------
const char* ssid = "Control booth";
const char* password = "MontyLives";
const char* mqtt_server = "192.168.86.102";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"
unsigned long previousMillis = 0;
const long interval = 10000;           // interval at which to send mqtt watchdog (milliseconds)


//Base notifications
const char* hostName = "Comms Station Encoder and Joystick"; // Set your controller unique name here
const char* quitMessage = "Comms Station Encoder and Joystick quitting...";
const char* onlineMessage = "Comms Station Encoder and Joystick Online";
const char* watchdogMessage = "Comms Station Encoder and Joystick Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 4
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/CommsStationEncoderJoystick/";
const char* dataChannel2 = "/Renegade/Engineering/CommsJoystick/Health/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/CommsStationEncoder/Control/",
  "/Renegade/Room1/CommsStationEncoder/Control/Lights/",
  "/Renegade/Room1/CommsStationJoysticks/Control/",
  "/Renegade/Room1/CommsStationJoysticks/Request/"
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
//-----------------------------------------------Buttons and Joystick-----------------------------------------------

byte joystickOffset = 0;
const byte button = 25;
byte buttonState = 0;
byte lastButtonState = 0;

#define NUM_JOYSTICKS 1
int currentJoystickInputs[NUM_JOYSTICKS][2] = {
  {0, 0}, //joystick 0; x, y
};

int lastJoystickInputs[NUM_JOYSTICKS][2] = {
  {0, 0}, //joystick 0; x, y
};

int currentJoystickPositions[NUM_JOYSTICKS] = {0};
int lastJoystickPositions[NUM_JOYSTICKS] = {0};

//Joystick Wire Default:
//Black     Ground (Switch Common)
//Purple    Up
//White     Down
//Green     Left
//Red       Right
//As long as Ground is on the outside-most pin, the directions actually don't matter that much see offset section under int readJoystick
int joystick[NUM_JOYSTICKS][4] = {
  //{UP,  DOWN, LEFT, RIGHT}
  {19, 21,  22,  32},  //joystick 0 (left)
};

//-------------------------------------------------Function constructors (for functions containing default parameters)-------------------------------------------------
int readJoystick(int joystickID, int offset = 2); //Need to control this from mqtt. Needs to default to 2 then ?
void reportDirection(byte joystickDirection, byte joystickID, boolean rawData = false);

//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 17
#define NUM_LEDS 39
#define MAX_POWER_MILLIAMPS 1000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
//-----------------------------------------------Encoders-----------------------------------------------
#define NUM_ENCODERS 1

const byte pins[NUM_ENCODERS][2] = {
  //{CLK(white wire), DT(colored wire)}
  {26, 27},
};

ESP32Encoder encoder[NUM_ENCODERS];

//Current/Last state to keep track of state changes (prevent flooding)
bool turningState[NUM_ENCODERS];
bool lastTurningState[NUM_ENCODERS];
long currentPosition[NUM_ENCODERS];
long lastPosition[NUM_ENCODERS];

//Variables to help determine when turning has started/stopped
unsigned long lastChangeTimestamp[NUM_ENCODERS];
unsigned long movementTimeout = 500;

//Variables to determine how long it was turned for
unsigned long turnDuration[NUM_ENCODERS];
unsigned long turnStartTime[NUM_ENCODERS];

//-----------------------------------------------End Header-----------------------------------------------



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

  if (message == "quit" && modeActive) {
    quit = true;
  }
  else if (message == "zero") {
    for (int i = 0; i < NUM_ENCODERS; i++) {
      encoder[i].setCount (0);
    }

    for (int i = 0; i < 3; i++) {
      fill_solid(leds, 105, CHSV(96, 255, 255));
      FastLED.show();
      wait(250);
      fill_solid(leds, 105, CHSV(96, 255, 0));
      FastLED.show();
      wait(250);
    }
  }
  else if (message == "encoderPositions") {
    //Create payload that contains each encoder position, example: 55 27 0
    String outputString;
    for (int i = 0; i < NUM_ENCODERS; i++) {
      //Get count
      currentPosition[i] = (encoder[i].getCount() / 2) % 100;

      //Negative count correction
      if (currentPosition[i] < 0) {
        currentPosition[i] = currentPosition[i] + 100;
      }
      if (i != NUM_ENCODERS - 1) {
        outputString = outputString + String(currentPosition[i]) + ",";
      } else {
        outputString = outputString + String(currentPosition[i]);
      }
    }
    client.publish(dataChannel, outputString.c_str());
  }
  else if (!strcmp(topic, subscribeChannel[1])) {
    byte peakCounter;
    byte wave;
    uint32_t timebase;
    //Various commands to flash certain elements ON DEMAND on Station 2. Continuous animations are considered MODES.
    //These animations are "one-shot;" they play once and  return to whatever you were doing before.
    //Warning: highly custom. Be sure to apply necessary changes if you reuse this encoder code.
    switch (val) {
      case 0:
        //Call attention: all rings
        timebase = millis();
        while (peakCounter < 6) {
          wave = beatsin8(100, 0, 100, timebase);
          for (int i = 0; i < NUM_ENCODERS; i++) {
            lightEncoderRing(i, CHSV(0, 0, wave));
          }
          FastLED.show();
          if (wave == 100) {
            peakCounter++;
          }
        }
        break;
      case 1:
        handleAll();
        break;
      default:
        handleAll();
        break;
    }

  } else if (!strcmp(topic, subscribeChannel[2]) && message != "directionMode") {
    joystickOffset = val;
    client.publish(dataChannel, String("Joystick offset updated to: " + String(joystickOffset)).c_str());
  }

  if (!modeActive) {
    modeActive = true;

    if (message == "dumpEncoder") {
      client.publish(dataChannel, "Encoder dump mode active");
      while (!quit) {
        handleAll();
        for (int i = 0; i < NUM_ENCODERS; i++) {
          encoderPosition(i);
        }
      }

    }
    else if (message == "isTurning") {
      client.publish(dataChannel, "Encoder isTurning mode active");
      while (!quit) {
        handleAll();
        for (int i = 0; i < NUM_ENCODERS; i++) {
          isTurning(i);
        }
      }
    }
    else if (message == "directionMode") {
      while (!quit) {
        handleAll();
        directionMode(joystickOffset);
      }
    }
    client.publish(mainPublishChannel, quitMessage);
    quit = false;
    modeActive = false;
  }

}

void setup () {
  Serial.begin(115200);
  //------------------------------------------------------Wifi------------------------------------------------------
  Serial.setTimeout(500);  // Set time out for 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  WiFi.mode(WIFI_STA);
  delay(10); 
  connectToStrongestWiFi();
  //-----Start MQTT----------------
  //reconnectMQTT(); This is handled inside connectToStrongestWiFi()
  //---------------OTA Setup------------------------
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(hostName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

      Serial.println("\nConnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

  //------------------------------------------------------FastLED------------------------------------------------------
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  fill_solid(leds, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();

  //------------------------------------------------------Other------------------------------------------------------
  //Initalize each encoder
  for (int i = 0; i < NUM_ENCODERS; i++) {
    encoder[i].attachHalfQuad (pins[i][0], pins[i][1]);
    encoder[i].setCount (0);
  }

  for (int i = 0; i < 4; i++) {
    pinMode(joystick[0][i], INPUT_PULLUP);
  }
  pinMode(button, INPUT_PULLUP);

}

void loop () {

  for (int i = 0; i < NUM_ENCODERS; i++) {
    //Get count
    currentPosition[i] = (encoder[i].getCount() / 2) % 100;

    //Negative count correction
    if (currentPosition[i] < 0) {
      currentPosition[i] = currentPosition[i] + 100;
    }
  }

  //byte wave = map(currentPosition[0], 0, 99, 0, 255);
  byte var = map(currentPosition[0], 0, 99, 0, 128);

  byte wave = beat8(30);
  byte startHue = wave;
  byte endHue = wave + var;
  handleAll();
  fill_gradient(leds, 0, CHSV(startHue, 255, 255), 34, CHSV(endHue, 255, 255));
  FastLED.show();
}

void ringLight() {
  for (int i = 0; i < NUM_ENCODERS; i++) {
    //Get count
    currentPosition[i] = (encoder[i].getCount() / 2) % 100;

    //Negative count correction
    if (currentPosition[i] < 0) {
      currentPosition[i] = currentPosition[i] + 100;
    }
  }
}



void encoderPosition(byte encoderID) {
  String outputString;
  //prints encoder position on state change.
  //Get count
  currentPosition[encoderID] = (encoder[encoderID].getCount() / 2) % 100;

  //Negative count correction
  if (currentPosition[encoderID] < 0) {
    currentPosition[encoderID] = currentPosition[encoderID] + 100;
  }

  //On POSITION state change
  if (currentPosition[encoderID] != lastPosition[encoderID]) {
    //    outputString = String(encoderID) + " : " + String(currentPosition[encoderID]);
    //    Serial.println(outputString);
    for (int i = 0; i < NUM_ENCODERS; i++) {
      if (i != NUM_ENCODERS - 1) {
        outputString = outputString + String(currentPosition[i]) + ",";
      } else {
        outputString = outputString + String(currentPosition[i]);
      }
    }
    client.publish(dataChannel, outputString.c_str());
    lastPosition[encoderID] = currentPosition[encoderID];
    //return currentPosition[encoderID];
  }
}

void isTurning(byte encoderID) {
  String outputString;
  byte val;
  //encoderID = which encoder you want to know about (0,1,2 etc.)

  //Get count
  currentPosition[encoderID] = (encoder[encoderID].getCount() / 2) % 100;

  //Negative count correction
  if (currentPosition[encoderID] < 0) {
    currentPosition[encoderID] = currentPosition[encoderID] + 100;
  }

  //On POSITION state change
  if (currentPosition[encoderID] != lastPosition[encoderID]) {
    lastChangeTimestamp[encoderID] = millis();
    turningState[encoderID] = true;

    //Serial.println(currentPosition);
    lastPosition[encoderID] = currentPosition[encoderID];
  }

  //On isTurning state change (were we turning and now not turning, OR not turning and now turning)
  if (turningState[encoderID] != lastTurningState[encoderID]) {
    if (turningState[encoderID]) {
      //We were not turning, and now turning
      outputString = "Encoder " + String(encoderID) + " turning";
      client.publish(dataChannel, outputString.c_str());
      Serial.println(outputString);
      turnStartTime[encoderID] = millis();
    } else {
      //We were turning, and now not turning
      turnDuration[encoderID] = millis() - turnStartTime[encoderID] - movementTimeout; //need to also account for the movementTimeout
      outputString = "Encoder stopped turning. Turning duration: " + String(turnDuration[encoderID]) + "ms";
      client.publish(dataChannel, outputString.c_str());
      Serial.println(outputString);
    }
    lastTurningState[encoderID] = turningState[encoderID];
  }

  //Check if we have stopped turning for (movementTimeout) milliseconds
  if (millis() - lastChangeTimestamp[encoderID] > movementTimeout) {
    //do something if encoder STOPS turning
    turningState[encoderID] = false;
  }

}

void directionMode(int offset) {
  byte wave = beat8(20);
  for (int i = 0; i < NUM_JOYSTICKS; i++) {
    wave = beat8(20);

    //read joysticks
    currentJoystickPositions[i] = readJoystick(i, offset);
    if (currentJoystickPositions[i] != lastJoystickPositions[i]) {
      wait(20);
      reportDirection(currentJoystickPositions[i], i);
      lastJoystickPositions[i] = currentJoystickPositions[i];
    }

    //read buttons
    buttonState = !digitalRead(button);
    String buttonMessage = "Button";
    if (buttonState != lastButtonState) {
      if (buttonState == 1) {
        buttonMessage = buttonMessage + " pressed";
      } else {
        buttonMessage = buttonMessage + " released";
      }
      Serial.println(buttonMessage);
      client.publish(dataChannel, buttonMessage.c_str());
      lastButtonState = buttonState;
    }


    //lightButton(map(wave, 0, 255, 0, 2), 0, 255, 255);
    //lightButton(i, 0, 255, 255);
    //fadeToBlackBy(leds, NUM_LEDS, 64);
    fill_solid(leds, NUM_LEDS, CHSV(0, 255, beatsin8(30)));
    FastLED.show();
    if (quit) {
      fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
    FastLED.show();
      return;
    }

  }
}


void lightButton(byte buttonToLight, byte h, byte s, byte v) {
  for (int i = 0; i < 4; i++) {
    leds[i + (4 * buttonToLight)] = CHSV(h, s, v);
  }
}

void reportDirection(byte joystickDirection, byte joystickID, boolean rawData) {
  String message = "";

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

  if (!digitalRead(joystick[joystickID][0])) {
    //Up
    y = 1;
  } else if (!digitalRead(joystick[joystickID][1])) {
    //Down
    y = -1;
  } else {
    //Center
    y = 0;
  }

  if (!digitalRead(joystick[joystickID][2])) {
    //Left
    x = -1;
  } else if (!digitalRead(joystick[joystickID][3])) {
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
void lightEncoderRing(byte encoderID, CHSV color) {
  for (int i = 0; i < 35; i++) {
    leds[i + (encoderID * 35)] = color;
  }
}
//------------------------------------------------------Service functions------------------------------------------------------
void handleAll() {
  checkConnection();
  ArduinoOTA.handle();
  client.loop();
}

void reconnectMQTT() {
  //if (usingSpiBus){SPI.end();
  //}
  // Loop until we're reconnected
  while (!client.connected() && (WiFi.status() == WL_CONNECTED)) {
    Serial.print("Attempting MQTT connection...");
    byte mac[5];
    WiFi.macAddress(mac);  // get mac address
    String clientId = String(mac[0]) + String(mac[4]);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish(mainPublishChannel, onlineMessage);
      // ... and resubscribe
      for (int i = 0; i < NUM_SUBSCRIPTIONS; i++) {
        client.subscribe(subscribeChannel[i]);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      wait(5000);
    }
  }
  //if (usingSpiBus){    SPI.begin();// Init SPI bus
  //}
    
}

void checkConnection() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    delay(100);
    connectToStrongestWiFi();   
    previousMillis = currentMillis;
  }
  if ((!client.connected() ) && (WiFi.status() == WL_CONNECTED) ) {
    Serial.println("Lost mqtt connection");
    reconnectMQTT();
  }
  if ((currentMillis - previousMillis >= interval)) {
    client.publish(watchdogChannel, watchdogMessage);
    reportFreeHeap();
    publishRSSI();
    publishWiFiChannel();
    previousMillis = currentMillis;

  }

}


void wait(uint16_t msWait) {
  uint32_t start = millis();
  while ((millis() - start) < msWait)
  {
    handleAll();
  }
}

// Function to connect to the strongest WiFi network
void connectToStrongestWiFi() {
  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("No networks found");
    return;
  }

  int strongestSignal = -1000;  // Initialize to a very low signal strength
  String bestSSID = "";
  String bestBSSID = "";

  Serial.println("Scan complete.");
  for (int i = 0; i < n; i++) {
    String foundSSID = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    String bssid = WiFi.BSSIDstr(i);

    if (foundSSID == ssid) {
      Serial.printf("Found SSID: %s, Signal strength: %d dBm, BSSID: %s\n", foundSSID.c_str(), rssi, bssid.c_str());

      if (rssi > strongestSignal) {
        strongestSignal = rssi;
        bestSSID = foundSSID;
        bestBSSID = bssid;
      }
    }
  }

  if (bestSSID != "") {
    Serial.printf("Connecting to strongest network: %s with BSSID: %s\n", bestSSID.c_str(), bestBSSID.c_str());

    WiFi.begin(bestSSID.c_str(), password);

    // Wait for connection
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 10) {
      delay(1000);
      Serial.print(".");
      tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
      publishWiFiInfo();
      //ArduinoOTA.begin();
      
    } else {
      Serial.println("\nFailed to connect.");
    }
  } else {
    Serial.println("No matching SSID found.");
  }
}
// Function to report the free heap memory over MQTT
void reportFreeHeap() {
  uint32_t freeHeap = ESP.getFreeHeap();  // Get free heap memory
  // Publish the heap memory data to the MQTT topic
  client.publish(dataChannel2,String("FreeMemory:" +  String(freeHeap)).c_str());
  //Serial.print("Published free heap: ");
  //Serial.println(heapStr);  // Print to Serial for debugging
}
void publishRSSI() {
  int rssi = WiFi.RSSI();  
  client.publish(dataChannel2, String("RSSI:" + String(rssi)).c_str());
}
void publishWiFiChannel() {
  int channel = WiFi.channel();  
  client.publish(dataChannel2, String("Channel:" + String(channel)).c_str());  
}
// Function to publish WiFi info (SSID and BSSID) via MQTT
void publishWiFiInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    String connectedSSID = WiFi.SSID();         // Get SSID
    String connectedBSSID = WiFi.BSSIDstr();    // Get BSSID

    // Create MQTT message
    String message = "Connected to SSID: " + connectedSSID + ", BSSID: " + connectedBSSID;

    // Publish message to MQTT
    client.publish(dataChannel2, message.c_str());

    //Serial.println("Published WiFi info to MQTT:");
    //Serial.println(message);
  } else {
    //Serial.println("Not connected to any WiFi network.");
  }
}