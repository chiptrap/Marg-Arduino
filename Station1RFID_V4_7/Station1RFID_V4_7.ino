#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

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

bool usingSpiBus = true; //Change to true if using spi bus
//Base notifications
const char* hostName = "Station 1 RFID"; // Set your controller unique name here
const char* quitMessage = "Station 1 RFID force quitting...";
const char* onlineMessage = "Station 1 RFID Online";
const char* watchdogMessage = "Station 1 RFID Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 2
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station1RFID/";
const char* dataChannel2 = "/Renegade/Room1/Station1RFID/Health/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station1RFID/Control/",
  "/Renegade/Room1/Station1RFID/ActivePositions/",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;

bool RfidRandom = true;
//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 17
#define NUM_LEDS 32
#define MAX_POWER_MILLIAMPS 1000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
//-----------------------------------------------RFID-----------------------------------------------
#define RST_PIN 22
#define SS_0_PIN 13
#define SS_1_PIN 14
#define SS_2_PIN 15
#define SS_3_PIN 16

#define NR_OF_READERS 4
bool resetReader = false;

byte ssPins[] = {SS_0_PIN, SS_1_PIN, SS_2_PIN, SS_3_PIN};

MFRC522 mfrc522[NR_OF_READERS];  // Create MFRC522 instance.
MFRC522::MIFARE_Key key;
// Global array to store which positions are active (1 means active, 0 means inactive)
bool activePositions[NR_OF_READERS] = {1, 1, 1, 1};  // Modify based on how many readers you have
byte currentRFIDinput[NR_OF_READERS] = {0, 0, 0, 2};
byte lastRFIDinput[NR_OF_READERS] = {0, 0, 0, 0};
byte correctInput[NR_OF_READERS] = {12, 0, 12, 0};

//-----------------------------------------------End Header-----------------------------------------------
byte getMemoryContent(byte reader, byte block = 4, byte trailerBlock = 7, byte index = 0); //default location for the card identifier



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
  } 
  else if (message == "zero") {

  }
  else if (message == "RfidRandom") {
     RfidRandom = true;
    }
    else if (message == "ResetRfid") {
     resetReader = true;
     readAllRFID();
    } 
    else if (String(topic) == "/Renegade/Room1/Station1RFID/ActivePositions/") {
        // Split the message by commas and update the activePositions array        
        int index = 0;
        for (int i = 0; i < message.length(); i++) {
            if (message[i] == ',') {
                continue;
            }
            if (index < NR_OF_READERS) {
                activePositions[index] = (message[i] == '1');  // '1' means active, '0' means inactive
                index++;
            }
        }       
    } 
    else if (message == "RfidFlash") {
     RfidRandom = false;
    }

  if (!modeActive) {
    modeActive = true;
    if (message == "dumpEncoder") {

    } 
    else if (message == "encoderPositions") {

    } 
    else if (message == "isTurning") {

    }
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
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN);  // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }

}

void loop () {
  readAllRFID();
  if (RfidRandom){
  EVERY_N_MILLISECONDS(500) {
    fill_solid(leds, NUM_LEDS, CHSV(128, 255, 90));
    for (int i = 0; i < 12; i++) {
      leds[random(NUM_LEDS)] = CHSV(random(255), 255, 90);
    }
    FastLED.show();
   }
  }
  handleAll();
}





//------------------------------------------------------Service functions------------------------------------------------------




void readAllRFID() {
  String output;
  if(resetReader){ 
    SPI.end();
    wait(200); 
    SPI.begin();// Init SPI bus
      
    for (byte reader = 0; reader < NR_OF_READERS; reader++) {
    // Halt PICC
      mfrc522[reader].PICC_HaltA();
     // Stop encryption on PCD
     mfrc522[reader].PCD_StopCrypto1();
     mfrc522[reader].PCD_Reset();
     mfrc522[reader].PCD_Init();
     // Publish an MQTT message indicating a timeout
    String timeoutMsg = "Reader " + String(reader) + " reinitialized manually.";
    client.publish(dataChannel2, timeoutMsg.c_str()); // Adjust the topic as needed
     wait(200);
     
    } 
    resetReader = false;
  }
  for (byte reader = 0; reader < NR_OF_READERS; reader++) {    
    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      currentRFIDinput[reader] = getMemoryContent(reader);
    } 
    else if(!mfrc522[reader].PICC_IsNewCardPresent()){
      currentRFIDinput[reader] = 0;
    }
    else{
      SPI.end();
    wait(200); 
    SPI.begin();// Init SPI bus
      // Halt PICC
      mfrc522[reader].PICC_HaltA();
     // Stop encryption on PCD
     mfrc522[reader].PCD_StopCrypto1();
     mfrc522[reader].PCD_Reset();
     mfrc522[reader].PCD_Init();
     // Publish an MQTT message indicating a timeout
    String timeoutMsg = "Reader " + String(reader) + " reinitialized.";
    client.publish(dataChannel2, timeoutMsg.c_str()); // Adjust the topic as needed
     wait(200);

      
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
  if (!RfidRandom){
  updateLEDsBasedOnRFID();//Handle leds based on card present
  }
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
//------------------------Added Functions for Reader LED's--------------------


// Function to update LEDs based on RFID input and MQTT-controlled positions
void updateLEDsBasedOnRFID() {
    for (int reader = 0; reader < NR_OF_READERS; reader++) {
        // Calculate the correct LED range for each reader
        int ledStart = (NR_OF_READERS - 1 - reader) * 8;
        int ledEnd = ledStart + 8;

        if (activePositions[reader]) {  // Only update if the position is active
            int card = currentRFIDinput[reader];  // Get the card number
            
            if (card >= 1 && card <= 4) {  // Card 1-4: Light 4 LEDs red, 4 LEDs green
                for (int led = ledStart; led < ledStart + 4; led++) {
                    leds[led] = CHSV(0, 255, 90);  // First 4 LEDs red (hue 0 for red)
                }
                for (int led = ledStart + 4; led < ledEnd; led++) {
                    leds[led] = CHSV(96, 255, 90);  // Last 4 LEDs green (hue 96 for green)
                }
            } else if (card == 5) {  // Card 5: Light all 8 LEDs green
                for (int led = ledStart; led < ledEnd; led++) {
                    leds[led] = CHSV(96, 255, 90);  // All 8 LEDs green (hue 96)
                }
            } else if (card > 5) {  // Card > 5: Light 4 LEDs green, 4 LEDs red
                for (int led = ledStart; led < ledStart + 4; led++) {
                    leds[led] = CHSV(96, 255, 90);  // First 4 LEDs green (hue 96)
                }
                for (int led = ledStart + 4; led < ledEnd; led++) {
                    leds[led] = CHSV(0, 255, 90);  // Last 4 LEDs red (hue 0)
                }
            } else {
                // No card detected (card == 0), turn all LEDs red
                for (int led = ledStart; led < ledEnd; led++) {
                    leds[led] = CHSV(0, 255, 90);  // Red (hue 0)
                }
            }
        } else {
            // If the position is inactive, set all the LEDs to red
            for (int led = ledStart; led < ledEnd; led++) {
                leds[led] = CHSV(0, 0, 90);  // Inactive positions set to solid white (hue 0)
            }
        }
    }
    
    FastLED.show();  // Apply the changes
}




//------------------------------------------------------Service functions------------------------------------------------------
void handleAll() {
  checkConnection();
  ArduinoOTA.handle();
  client.loop();
}

void reconnectMQTT() {
  if (usingSpiBus){SPI.end();
  }
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
  if (usingSpiBus){
    SPI.begin();// Init SPI bus
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
