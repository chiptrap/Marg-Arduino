#include <ESP32Encoder.h> // https://github.com/madhephaestus/ESP32Encoder.git 
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
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
const long interval = 20000;           // interval at which to send mqtt watchdog (milliseconds)


//Base notifications
const char* hostName = "Station 2 Encoders and RFID"; // Set your controller unique name here
const char* quitMessage = "Station 2 Encoders and RFID force quitting...";
const char* onlineMessage = "Station 2 Encoders and RFID Online";
const char* watchdogMessage = "Station 2 Encoders and RFID Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station2EncodersRFID/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station2EncodersRFID/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
//-----------------------------------------------RFID-----------------------------------------------
#define SS_0_PIN  27
#define SS_1_PIN  13
#define SS_2_PIN  4
#define rstPin    32
#define NR_OF_READERS 3

byte ssPins[] = {SS_0_PIN, SS_1_PIN, SS_2_PIN};

MFRC522 mfrc522[NR_OF_READERS];  // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

byte currentRFIDinput[NR_OF_READERS] = {0, 0, 0};
byte lastRFIDinput[NR_OF_READERS] = {0, 0, 0};
byte correctInput[NR_OF_READERS] = {12, 0, 12};

//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 21
#define NUM_LEDS 35
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];

//-----------------------------------------------Encoders-----------------------------------------------
#define NUM_ENCODERS 3

const byte pins[NUM_ENCODERS][2] = {
  //{CLK(white wire), DT(colored wire)}
  {25, 26},
  {22, 33},
  {16, 17}
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
byte getMemoryContent(byte reader, byte block = 4, byte trailerBlock = 7, byte index = 0); //default location for the card identifier

void handleAll() {
  checkConnection();
  ArduinoOTA.handle();
  client.loop();
  readAllRFID();
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();

  if (message == "quit" && modeActive) {
    client.publish(mainPublishChannel, quitMessage);
    quit = true;
  } else if (message == "zero") {
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

  if (!modeActive) {

    if (message == "dumpEncoder") {
      client.publish(dataChannel, "Encoder dump mode active");
      modeActive = true;
      while (!quit) {
        handleAll();
        for (int i = 0; i < NUM_ENCODERS; i++) {
          encoderPosition(i);
        }
      }
      quit = false;
      modeActive = false;

    } else if (message == "encoderPositions") {
      modeActive = true;

      //Create payload that contains each encoder position, example: 55 27 0
      String outputString;
      for (int i = 0; i < NUM_ENCODERS; i++) {
        //Get count
        currentPosition[i] = (encoder[i].getCount() / 2) % 100;

        //Negative count correction
        if (currentPosition[i] < 0) {
          currentPosition[i] = currentPosition[i] + 100;
        }
        outputString = outputString + String(currentPosition[i]) + " ";
      }

      client.publish(dataChannel, outputString.c_str());
      quit = false;
      modeActive = false;
    } else if (message == "isTurning") {
      client.publish(dataChannel, "Encoder isTurning mode active");
      modeActive = true;
      while (!quit) {
        handleAll();
        for (int i = 0; i < NUM_ENCODERS; i++) {
          isTurning(i);
        }
      }
      quit = false;
      modeActive = false;
    } else if (message == "followLight") {
      modeActive = true;
      while (!quit) {
        handleAll();
        funLights();
      }
      quit = false;
      modeActive = false;
    } else if (message == "fullLight") {
      modeActive = true;
      while (!quit) {
        handleAll();
        funLights1();
      }
      quit = false;
      modeActive = false;
    }

  }

}

void setup () {
  Serial.begin (115200);
  //------------------------------------------------------Wifi------------------------------------------------------
  Serial.setTimeout(500);  // Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  //------------------------------------------------------RFID------------------------------------------------------
  while (!Serial)
    ;           // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();  // Init SPI bus

  // Prepare the key (used both as key A and as key B)
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], rstPin);  // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }

  //------------------------------------------------------FastLED------------------------------------------------------
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);

  //------------------------------------------------------Other------------------------------------------------------
  //Initalize each encoder
  for (int i = 0; i < NUM_ENCODERS; i++) {
    encoder[i].attachHalfQuad (pins[i][0], pins[i][1]);
    encoder[i].setCount (0);
  }

}

void loop () {
  handleAll();
}



void funLights() {
  for (int i = 0; i < NUM_ENCODERS; i++) {
    //Get count
    currentPosition[i] = (encoder[i].getCount() / 2) % 100;

    //Negative count correction
    if (currentPosition[i] < 0) {
      currentPosition[i] = currentPosition[i] + 100;
    }
  }

  byte pos = map(currentPosition[0], 0, 99, 0, NUM_LEDS);
  byte hue = map(currentPosition[1], 0, 99, 0, 255);
  byte val = map(currentPosition[2], 0, 99, 0, 255);
  fadeToBlackBy(leds, NUM_LEDS, 1);
  leds[pos] = CHSV(hue, 255, val);
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
    outputString = String(encoderID) + " : " + String(currentPosition[encoderID]);
    Serial.println(outputString);
    client.publish(dataChannel, outputString.c_str());
    lastPosition[encoderID] = currentPosition[encoderID];
    //return currentPosition[encoderID];
  }
}

void isTurning(byte encoderID) {
  String outputString;
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

//------------------------------------------------------Service functions------------------------------------------------------
void setup_wifi() {
  delay(10);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

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

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
  SPI.end();
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
  SPI.begin();  // Init SPI bus
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
    client.publish(watchdogChannel, watchdogMessage);
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

void readAllRFID() {
  String output;
  for (byte reader = 0; reader < NR_OF_READERS; reader++) {
    funLights();
    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      currentRFIDinput[reader] = getMemoryContent(reader);
    } else {
      currentRFIDinput[reader] = 0;
    }


    if (currentRFIDinput[reader] != lastRFIDinput[reader]) {
      for (byte i = 0; i < NR_OF_READERS; i++) {
        output = output + String(currentRFIDinput[i]) + " ";
        Serial.print(currentRFIDinput[i]);
        Serial.print(" ");
      }
      Serial.println();
//      if (currentRFIDinput[reader] == 0) {
//        //disable this if you don't need a separate message for "card removed"
//        client.publish(dataChannel, String("Card " + String(lastRFIDinput[reader]) + " removed").c_str());
//        //Serial.println(String("Card " + String(lastRFIDinput[reader]) + " removed"));
//      }
      client.publish(dataChannel, output.c_str());
      lastRFIDinput[reader] = currentRFIDinput[reader];
    }

  }//for
}

byte getMemoryContent(byte reader, byte block, byte trailerBlock, byte index) {
  //mfrc522[reader].PCD_Init();

  //trailerBlock is the highest block in the sector, for example, block 0 is in sector 0, so its trailerBlock is block 3. Block 3's trailerBlock is also block 3.
  //block goes from 0 - 63
  //index goes from 0 - 15
  byte readBuffer[18];
  byte readBufferSize = sizeof(readBuffer);
  MFRC522::StatusCode status;

  // Authenticate using key A
  //Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode) mfrc522[reader].PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522[reader].uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522[reader].GetStatusCodeName(status));
    return 0;
  }

  //read block
  mfrc522[reader].MIFARE_Read(block, readBuffer, &readBufferSize);

  //print stuff
  //  dump_byte_array(readBuffer, 16, block);
  //  Serial.print(String("Value in block " + String(block) + ", index " + index + ": "));
  //  Serial.println(readBuffer[index]);

  // Halt PICC
  mfrc522[reader].PICC_HaltA();
  // Stop encryption on PCD
  mfrc522[reader].PCD_StopCrypto1();
  mfrc522[reader].PCD_Reset();
  mfrc522[reader].PCD_Init();


  return readBuffer[index];
}


//Helper routine to dump a byte array as hex values to Serial.

void dump_byte_array(byte * buffer, byte bufferSize, byte block) {
  Serial.println(String("Block " + String(block) + " content:"));
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
  return;
}
