#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

//-------------------------------------------------Shift Register-------------------------------------------------
// 165 Shift Register parameters
const int data165 = D2;  /* Q7 */
const int clock165 = D3; /* CP */
const int latch165 = D4;  /* PL */
const int numBits = 16;  /* Set to 8 * number of shift registers */
int buttonStates[numBits];
int buttonLastStates[numBits];
int arraySize = sizeof(buttonStates) / sizeof(buttonStates[0]);

//595 Shift Register Parameters
const int latch595 = D6;
const int clock595 = D5;
const int data595 = D7;
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
const char* consoleNumber = "4";
const char* hostName =         "Station 1 Console 4"; // Set your controller unique name here
const char* quitMessage =      "Station 1 Console 4 quitting...";
const char* onlineMessage =    "Station 1 Console 4 Online";
const char* watchdogMessage =  "Station 1 Console 4 Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station1Buttons/";
const char* dataChannelInputs = "/Renegade/Room1/Station1Buttons/Inputs/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station1Buttons/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;

PubSubClient client(wifiClient);
//-------------------------------------------------FastLED-------------------------------------------------
// FastLED parameters
#define DATA_PIN D1
#define NUM_LEDS 4
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
//-------------------------------------------------Other-------------------------------------------------
//Game logic
const byte codeLength = 6;

//correctSwitchPositions[0] = Green switch
//correctSwitchPositions[1] = Red switch
//Green switch: Left position = 6, Right position = 5
//Red switch:   Left position = 4, Right position = 3
//For both, middle is 0

//2x4 grid of buttons counts down left to right
//Use map below:
// 15   14  13  12
// 11   10   9   8

//Uncomment correct one depending on which console on station 1 you are uploading to:
//Console 1
//byte correctSwitchPositions[2] = {0, 4};
//byte correctArray[codeLength] = {9, 8, 12, 10, 11, 13};

////Console 2
//byte correctSwitchPositions[2] = {5, 4};
//byte correctArray[codeLength] = {8, 10, 13, 15, 11, 12};

//Console 3
//byte correctSwitchPositions[2] = {6, 0};
//byte correctArray[codeLength] = {11, 13, 9, 10, 14, 12};

//Console 4
byte correctSwitchPositions[2] = {5, 3};
byte correctArray[codeLength] = {13, 11, 10, 9, 8, 15};

byte inputArray[codeLength];

boolean modeActive = false;
boolean quit = false;
boolean driveSignatureSolved = false;
byte waveBPM = 30;
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
  if (message == "quit" && modeActive) {
    client.publish(mainPublishChannel, quitMessage);
    SIPOregisterOutput(0); //black out buttons
    quit = true;
  } else if (message == "Drive Signature Solved") {
    driveSignatureSolved = true;
  }

  if (!modeActive) {

    modeActive = true;
    if (message == "sequenceMode") {
      driveSignatureSolved = false; //Assume drive signature has not been solved when entering sequence mode
      client.publish(dataChannel, "Sequence mode starting; waiting for drive signature response");

      waitForFadeIn(waveBPM);
      while (!driveSignatureSolved && !quit) {
        handleAll();
        byte wave = beatsin8(waveBPM, 0, 255);
        fill_solid(leds, NUM_LEDS, CHSV(0, 255, wave));
        FastLED.show();

      }
      if (!quit) {
        client.publish(dataChannel, "Sequence mode buttons enabled; ready for input");
      }
      waitForFadeOut(waveBPM, 0);

      while (!quit) {
        yellowButton();
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
  Serial.begin(9600);
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

  SIPOregisterOutput(0); //black out buttons
}

void loop()
{
  handleAll();

  //default state
  PISOreadShiftRegister();
  int convertedValue = binaryArrayToInt(buttonStates, arraySize);
  SIPOregisterOutput(convertedValue);
}

void waitForFadeOut(byte bpm, byte hue) {
  while (beatsin8(waveBPM, 0, 255) != 1) {
    //wait for hue to fade out
    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, beatsin8(waveBPM, 0, 255)));
    FastLED.show();
    yield();
  }
}

void waitForFadeIn(byte bpm) {
  while (beatsin8(waveBPM, 0, 255) != 1) {
    //wait for hue to fade in
    handleAll();
  }
}

void yellowButton() {//In this mode, it is watching for the input sequence for drive auth
  byte input = 0;
  byte cursorPosition = 0;
  byte peakCounter = 0;
  byte wave;
  String finalInput = consoleNumber + String(",");
  int buttonLEDs[numBits] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int convertedValue = binaryArrayToInt(buttonLEDs, arraySize);
  SIPOregisterOutput(convertedValue); //Write array state to outputs

  //-------------------------------------------------Get Input-------------------------------------------------

  while (cursorPosition < codeLength) {
    handleAll();
    wave = beatsin8(waveBPM, 0, 255);
    fill_solid(leds, NUM_LEDS, CHSV(64, 255, wave));
    FastLED.show();

    //Get input
    input = getButton();

    //Reject button 7 (fastled button)
    if (input == 7) {
      input = 16;
    }

    //Light button LEDs along the way
    buttonLEDs[input] = 1;
    int convertedValue = binaryArrayToInt(buttonLEDs, arraySize);
    SIPOregisterOutput(convertedValue); //Write array state to outputs

    //Only accept valid button presses
    if (input != numBits) {
      //Serial.println(input);
      inputArray[cursorPosition] = input;
      finalInput = finalInput + String(input) + ",";
      cursorPosition++;
    }

    //Check for quit
    if (quit) {
      SIPOregisterOutput(0); //turn off LEDs
      waitForFadeOut(waveBPM, 64);
      return;
    }
  }

  //-------------------------------------------------Send data-------------------------------------------------
  //  Serial.println(finalInput);
  //  client.publish(dataChannelInputs, finalInput.c_str());
  //  if (digitalReadRegister(correctSwitchPositions[0]) && digitalReadRegister(correctSwitchPositions[1])) {
  //    client.publish(dataChannel, "Switches correct");
  //  }


  //-------------------------------------------------Check input-------------------------------------------------
  bool greenSwitchCorrect = false;
  bool redSwitchCorrect = false;

  //If correct position is middle, make sure pin 6,7,5,3 are all 0
  if ( ((correctSwitchPositions[0] == 0) && (!digitalReadRegister(6)) && (!digitalReadRegister(5))) || (digitalReadRegister(correctSwitchPositions[0])) ) {
    greenSwitchCorrect = true;
  }
  if ( ((correctSwitchPositions[1] == 0) && (!digitalReadRegister(4)) && (!digitalReadRegister(3))) || (digitalReadRegister(correctSwitchPositions[1])) ) {
    redSwitchCorrect = true;
  }

  if (digitalReadRegister(6)) {
    finalInput = finalInput + "6";
  } else if (digitalReadRegister(5)) {
    finalInput = finalInput + "5";
  } else {
    finalInput = finalInput + "0";
  }

  if (digitalReadRegister(4)) {
    finalInput = finalInput + ",4";
  } else if (digitalReadRegister(3)) {
    finalInput = finalInput + ",3";
  } else {
    finalInput = finalInput + ",0";
  }

  for (int i = 0; i < codeLength; i++) {
    if ((inputArray[i] != correctArray[i]) || (!greenSwitchCorrect) || (!redSwitchCorrect)) {
      //delay animation
      Serial.println("Incorrect entry");
      client.publish(dataChannel, "Incorrect entry");
      client.publish(dataChannelInputs, finalInput.c_str());
      fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
      FastLED.show();
      wait(2000);
      fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
      FastLED.show();
      wait(500);
      //yellow light fade-in on reset
      while (wave != 1) {
        wave = beatsin8(waveBPM, 0, 255);
        handleAll();
      }
      return;
    }
  }

  //-------------------------------------------------Correct Input-------------------------------------------------
  Serial.println("Correct entry");
  client.publish(dataChannel, String("Console " + String(consoleNumber) + " correct entry, waiting for quit to reset").c_str());
  client.publish(dataChannelInputs, finalInput.c_str());
  uint32_t currentMillis = millis();

  while (peakCounter < 5) {
    yield();
    byte wave2 = beatsin8(90, 0, 255, currentMillis);
    fill_solid(leds, NUM_LEDS, CHSV(128, 255, wave2));
    FastLED.show();
    if (wave2 == 255) {
      peakCounter++;
    }
  }
  //  fill_solid(leds, NUM_LEDS, CHSV(128, 255, 255));
  //  FastLED.show();

  //wait for quit signal
  while (true) {
    handleAll();
    if (quit) {
      SIPOregisterOutput(0); //black out buttons
      fill_solid(leds, NUM_LEDS, CHSV(0, 0, 0));
      FastLED.show();
      return;
    }
  }

}

int getButton() {
  PISOreadShiftRegister();
  for (int i = 7; i < numBits; i++) {
    if (buttonStates[i] != buttonLastStates[i]) {
      buttonLastStates[i] = buttonStates[i];
      if (buttonStates[i]) {
        return i;
      }
    }
  }
  return numBits;
}

void PISOreadShiftRegister() {
  // Step 1: Sample
  digitalWrite(latch165, LOW);
  delayMicroseconds(0.1);
  digitalWrite(latch165, HIGH);

  // Step 2: Shift
  // Serial.print("Bits: ");
  for (int i = 0; i < numBits; i++)
  {
    int bit = digitalRead(data165);
    buttonStates[i] = bit;
    // Serial.print(buttonStates[i]);
    digitalWrite(clock165, HIGH); // Shift out the next bit
    delayMicroseconds(0.1);
    digitalWrite(clock165, LOW);
  }

  // Serial.println();
}

boolean digitalReadRegister(int pinNumber) {
  //pinNumber goes from 0 to (numBits-1)
  boolean output = false;
  PISOreadShiftRegister();
  // output = buttonStates[numBits - 1 - pinNumber];
  output = buttonStates[pinNumber];
  delay(10); //debounce
  return output;
}

int binaryArrayToInt(int binaryArray[], int size) {
  int result = 0;

  for (int i = 0; i < size; i++) {
    result = (result << 1) | binaryArray[i];
  }
  //Serial.println(result);
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
//---------------Flash Button and watch for input-----------------
void flashButtonRed(){
  handleAll();
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
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
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
      Serial.print("failed, rc=");
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
