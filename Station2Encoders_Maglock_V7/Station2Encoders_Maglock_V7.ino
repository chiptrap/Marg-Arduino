//added esp_task_wdt as of 11-12-24
#include <ESP32Encoder.h> // https://github.com/madhephaestus/ESP32Encoder.git 
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFi.h>
//#include <SPI.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

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

bool usingSpiBus = false; //Change to true if using spi bus
//Base notifications
const char* hostName = "Station 2 Encoders"; // Set your controller unique name here
const char* quitMessage = "Station 2 Encoders quitting...";
const char* onlineMessage = "Station 2 Encoders Online";
const char* bootMessage = "Station 2 Encoders Power Cycled";
const char* watchdogMessage = "Station 2 Encoders Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 2
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station2Encoders/";
const char* dataChannel2 = "/Renegade/Room1/Station2Encoders/Health/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station2Encoders/Control/",
  "/Renegade/Room1/Station2Encoders/Control/Lights/"
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);
bool espJustReset = true;
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 4
#define NUM_LEDS 105
#define MAX_POWER_MILLIAMPS 1000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
//-----------------------------------------------Black 3-position Switches-----------------------------------------------
#define NUM_SWITCHES 6 //6 inputs, 3 actual switches
const byte switchPins[NUM_SWITCHES] = {
  19, 21,
  22, 27,
  25, 26
};

boolean currentSwitchStates[NUM_SWITCHES];
boolean lastSwitchStates[NUM_SWITCHES];
String enumeration[3] = {"Center", "Center", "Center"};
//-----------------------------------------------Encoders-----------------------------------------------
#define NUM_ENCODERS 3

const byte pins[NUM_ENCODERS][2] = {
  //{CLK(white wire), DT(colored wire)}
  {13, 14},
  {15, 16},
  {17, 18}
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

//Variables for ticker tape puzzle (Station 2 Joysticks)
byte command = 0;

//-----------------------------------------------Briefcase Maglock control-----------------------------------

#define maglockPin 32 
bool maglock = false;

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
  else if (message == "maglockRelease"){
    maglock = false;
   digitalWrite (maglockPin,LOW); 
   client.publish(dataChannel, "Briefcase Maglock Released"); 
  } 
  else if (message == "maglockEngage"){
  maglock = true; 
  digitalWrite (maglockPin,HIGH); 
  client.publish(dataChannel, "Briefcase Maglock Engaged");
  }
  
  else if (message == "zero") {
    for (int i = 0; i < NUM_ENCODERS; i++) {
      encoder[i].setCount (0);
    }

    for (int i = 0; i < 3; i++) {
      fill_solid(leds, NUM_LEDS, CHSV(96, 255, 255));
      FastLED.show();
      wait(250);
      fill_solid(leds, NUM_LEDS, CHSV(96, 255, 0));
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
          handleAll();//added 10-1-24
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

  } 
  else if (message == "increment") {
    command = 1;
  } 
  else if (message == "incorrect") {
    command = 2;
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
      client.publish(mainPublishChannel, quitMessage);

    } else if (message == "isTurning") {
      client.publish(dataChannel, "Encoder isTurning mode active");
      while (!quit) {
        handleAll();
        for (int i = 0; i < NUM_ENCODERS; i++) {
          isTurning(i);
        }
      }
      client.publish(mainPublishChannel, quitMessage);
    } else if (message == "followLight") {
      while (!quit) {
        handleAll();
        ringLight();
      }
      client.publish(mainPublishChannel, quitMessage);
    } else if (message == "fullLight") {
      while (!quit) {
        handleAll();
        funLights1();
      }
      client.publish(mainPublishChannel, quitMessage);
    } else if (message == "tickerPuzzle") {
      tickerPuzzle(18);
      fill_solid(leds, NUM_LEDS, CHSV(96, 255, 0));
     FastLED.show();
    }
    
    quit = false;
    modeActive = false;
  }

}

void setup () {
  Serial.begin(115200);
  //------------------------------------------------------Wifi------------------------------------------------------

  // Register event handlers  
  esp_task_wdt_init(10, true); // Set timeout to 10 seconds
  esp_task_wdt_add(NULL);      // Add current task to watchdog
  
   //WiFi.onEvent(onWiFiDisconnect, SYSTEM_EVENT_STA_DISCONNECTED);
  Serial.setTimeout(500);  // Set time out for 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  WiFi.mode(WIFI_STA);
  delay(10);
  connectToWiFi(); 
  //connectToStrongestWiFi();
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

  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(switchPins[i], INPUT_PULLUP);
  }

  pinMode(maglockPin, OUTPUT);
  digitalWrite (maglockPin,LOW);

}

void loop () {
  handleAll();
  esp_task_wdt_reset();        // Reset watchdog timer to prevent reset
}

void tickerPuzzle(byte codeLength) {
  command = 0;
  byte var = 0;
  byte cursorPosition = 0;
  uint32_t timestamp;
  CHSV color;
  FastLED.clear();

  while (var < codeLength) {
    handleAll();
    if (quit) {
      return;
    }
    if (command ==  1) {
      var++;
      color = CHSV(128, 255, 255);
      command = 0;
    } else if (command == 2) {
      var++;
      color = CHSV(0, 255, 255);
    }

    cursorPosition = map(var, 0, codeLength, 0, 34);

    for (int i = 0; i < cursorPosition; i++) {
      leds[i] = color;
      leds[i + 35] = color;
      leds[i + 70] = color;
    }
    FastLED.show();

    //If wrong sequence, erase progress and reset
    if (command == 2) {
      command = 0;
      wait(250);
      timestamp = millis();
      while (millis() - timestamp < 1000) {
        leds[map(beat8(60, timestamp), 0, 255, 34, 0)] = CHSV(0, 0, 0);
        leds[map(beat8(60, timestamp), 0, 255, 69, 35)] = CHSV(0, 0, 0);
        leds[map(beat8(60, timestamp), 0, 255, 104, 70)] = CHSV(0, 0, 0);
        FastLED.show();
        handleAll();
      }
      client.publish(mainPublishChannel, "End of Red Flash Loop Station 2 Encoders");
      FastLED.clear();
      FastLED.show();
      return;
    }

  }
  byte peakCounter = 0;
  timestamp = millis();
  while (peakCounter < 4) {
    handleAll();
    yield();
    byte wave2 = beatsin8(90, 0, 255, timestamp);
    fill_solid(leds, NUM_LEDS, CHSV(128, 255, wave2));
    FastLED.show();
    if (wave2 == 255) {
      peakCounter++;
    }
  }

}

void readSwitches() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    currentSwitchStates[i] = !digitalRead(switchPins[i]);

    //If switch state change
    if (currentSwitchStates[i] != lastSwitchStates[i]) {

      for (int j = 0; j < 3; j++) {
        if (currentSwitchStates[j * 2]) {
          enumeration[j] = "Left";
        } else if (currentSwitchStates[(j * 2) + 1]) {
          enumeration[j] = "Right";
        } else {
          enumeration[j] = "Center";
        }
      }

      //Send switch states
      client.publish(dataChannel, String(enumeration[0] + " " + enumeration[1] + " " + enumeration[2]).c_str());
      lastSwitchStates[i] = currentSwitchStates[i];
    }
  }
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

  byte hue = 128;
  byte pos[3];
  pos[0] = map(currentPosition[0], 0, 99, 0, 34);
  pos[1] = map(currentPosition[1], 0, 99, 35, 69);
  pos[2] = map(currentPosition[2], 0, 99, 70, 104);

  leds[pos[0]] = CHSV(hue, 255, 255);
  leds[pos[1]] = CHSV(hue, 255, 255);
  leds[pos[2]] = CHSV(hue, 255, 255);


  fadeToBlackBy(leds, NUM_LEDS, 3);
  FastLED.show();
}

void funLights1() {
  for (int i = 0; i < NUM_ENCODERS; i++) {
    //Get count
    currentPosition[i] = (encoder[i].getCount() / 2) % 100;

    //Negative count correction
    if (currentPosition[i] < 0) {
      currentPosition[i] = currentPosition[i] + 100;
    }
  }
  byte hue = map(currentPosition[0], 0, 99, 0, 255);
  byte sat = map(currentPosition[1], 0, 99, 0, 255);
  byte val = map(currentPosition[2], 0, 99, 0, 255);
  fadeToBlackBy(leds, NUM_LEDS, 1);
  fill_solid(leds, NUM_LEDS, CHSV(hue, sat, val));
  FastLED.show();
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

/**
 * @brief Monitors and track s the state of a specificencoder, reporting when it starts or stops turning.
 * 
 * This function tracks the position of the specified encoder, detects changes in its position,
 * and manages the turning state (whether the encoder is actively being turned or not).
 * It publishes messages via MQTT when the encoder starts turning or stops turning,
 * and calculates the duration of each turning session.
 * 
 * @param encoderID The index of the encoder to monitor (0, 1, 2, etc.)
 * 
 * @details The function:
 *   1. Gets the current position of the encoder and normalizes it to a value between 0-99
 *   2. Updates timestamps when the position changes
 *   3. Detects state transitions between turning and not turning
 *   4. Publishes notifications via MQTT client when turning starts or stops
 *   5. Calculates and reports the duration of turning sessions
 *   6. Considers the encoder stopped if no movement is detected for movementTimeout milliseconds
 * 
 * @note Requires global variables:
 *   - currentPosition[], lastPosition[] - Arrays to track encoder positions
 *   - turningState[], lastTurningState[] - Arrays to track turning states
 *   - lastChangeTimestamp[], turnStartTime[], turnDuration[] - Arrays for timing
 *   - encoder[] - Array of encoder objects
 *   - client - MQTT client for publishing updates
 *   - dataChannel - MQTT topic for data publishing
 *   - movementTimeout - Milliseconds threshold to consider movement stopped
 */
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
      outputString = "Encoder " + String(encoderID) + " stopped turning. Turning duration: " + String(turnDuration[encoderID]) + "ms";
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
      if (espJustReset){//if this is the first connection after a reset or first power on
        espJustReset = false;
        client.publish(mainPublishChannel, bootMessage);
      }
      else{
      client.publish(mainPublishChannel, onlineMessage);
      }
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
  //if (usingSpiBus){ SPI.begin();// Init SPI bus
  //}
    
}

void checkConnection() {
  """The checkConnection function in C++ monitors the WiFi 
  and MQTT connections, attempting to reconnect if the WiFi 
  is disconnected and re-establishing the MQTT connection if
  it is lost, while also periodically publishing system status 
  messages."""
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    delay(100);
    connectToWiFi();
    //connectToStrongestWiFi();   
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




// Simplified function to connect to a single WiFi network
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  // Wait for connection
  int tries = 0;
  const int maxTries = 5;
  while (WiFi.status() != WL_CONNECTED && tries < maxTries) {
    delay(1500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi.");
    reconnectMQTT();          // Establish MQTT connection if needed
    publishWiFiInfo();        // Publish connection information
    // ArduinoOTA.begin();    // Uncomment if using OTA updates
  } else {
    Serial.println("\nFailed to connect. Restarting...");
    ESP.restart();
  }
}

// Event handler for when WiFi is disconnected
void onWiFiDisconnect(system_event_id_t event) {
  Serial.println("WiFi disconnected. Attempting to reconnect...");
  connectToWiFi();
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
    while (WiFi.status() != WL_CONNECTED && tries < 5) {
      delay(1500);
      Serial.print(".");
      tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
      publishWiFiInfo();
      publishIPAddress();
      //ArduinoOTA.begin();
      
    } else {
      Serial.println("\nFailed to connect.");
      ESP.restart();
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
void publishIPAddress() {
  // Get the local IP address
  String ipAddress = WiFi.localIP().toString();
  
  // Publish the IP address to the specified topic
  if (client.publish(dataChannel2, ipAddress.c_str())) {
    //Serial.println("IP Address published: " + ipAddress);
  } else {
    Serial.println("Failed to publish IP Address");
  }
}
