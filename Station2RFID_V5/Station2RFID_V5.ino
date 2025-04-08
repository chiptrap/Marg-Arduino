////////////Not Working




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
const char* hostName = "Station 2 RFID"; // Set your controller unique name here
const char* quitMessage = "Station 2 RFID force quitting...";
const char* onlineMessage = "Station 2 RFID Online";
const char* watchdogMessage = "Station 2 RFID Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station2RFID/";
const char* dataChannel2 = "/Renegade/Room1/Station2RFID/Health/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station2RFID/Control/",
  //"myTopic0",
  //"myTopic1",+
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
#define NUM_LEDS 24
#define MAX_POWER_MILLIAMPS 1000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 25
CRGB leds[NUM_LEDS];
//-----------------------------------------------RFID-----------------------------------------------
#define SS_0_PIN  25
#define SS_1_PIN  26
#define SS_2_PIN  27
#define rstPin    32
#define NR_OF_READERS 3

byte ssPins[] = {SS_0_PIN, SS_1_PIN, SS_2_PIN};

MFRC522 mfrc522[NR_OF_READERS];  // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

byte currentRFIDinput[NR_OF_READERS] = {0, 0, 0};
byte lastRFIDinput[NR_OF_READERS] = {0, 0, 0};
byte correctInput[NR_OF_READERS] = {12, 0, 12};

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
  } else if (message == "zero") {

  }
  else if (message == "RfidRandom") {
     RfidRandom = true;
    } 
    else if (message == "RfidFlash") {
     RfidRandom = false;
    }

  if (!modeActive) {
    modeActive = true;
    if (message == "dumpEncoder") {

    } else if (message == "encoderPositions") {

    } else if (message == "isTurning") {

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
    mfrc522[reader].PCD_Init(ssPins[reader], rstPin);  // Init each MFRC522 card
    delay(500);
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
    fill_solid(leds, NUM_LEDS, CHSV(128, 255, 255));
    for (int i = 0; i < 12; i++) {
      leds[random(NUM_LEDS)] = CHSV(random(256), 255, 255);
    }
    FastLED.show();
   }
  }
  handleAll();

}






bool isRFIDReaderActive(byte reader) {
    unsigned long startTime = millis(); // Record the start time
    unsigned long timeout = 500; // Set a timeout of 500 ms
    bool cardPresent; // Variable to hold the card presence status

    // Attempt to check if the reader is able to detect a card
    while (millis() - startTime < timeout) {
        cardPresent = mfrc522[reader].PICC_IsNewCardPresent();
        if (cardPresent) {
            // A card is detected
            Serial.print("Reader "); 
            Serial.print(reader);
            Serial.println(" is active (card detected).");
            return true; // Reader is active
        }
        else if (!cardPresent) {
        // No card present, but the reader is still functioning
        Serial.print("Reader "); 
        Serial.print(reader);
        Serial.println(" is active (no card present).");
        return true; // Reader is still active
      }
    }

    // After timeout return false
    

    // If we exit the loop without returning true, it indicates a timeout
    Serial.print("Reader "); 
    Serial.print(reader);
    Serial.println(" timed out.");
    // Publish an MQTT message indicating a timeout
    String timeoutMsg = "Reader " + String(reader) + " no response!";
    client.publish(dataChannel2, timeoutMsg.c_str()); // Adjust the topic as needed
    return false; // Indicate inactive reader
}






void readAllRFID() {
    String output;
    const unsigned long reinitTimeLimit = 5000;  // Time limit in milliseconds before reinitializing
    unsigned long lastSuccessfulRead[NR_OF_READERS] = {0, 0, 0}; // Last successful read timestamps

    for (byte reader = 0; reader < NR_OF_READERS; reader++) {
        // Check if the reader is active
        if (!isRFIDReaderActive(reader)) {
            // If not active, try to reinitialize
            Serial.print("RFID reader "); 
            Serial.print(reader);
            Serial.println(" is not active. Attempting reinitialization...");
            mfrc522[reader].PCD_Init(ssPins[reader], rstPin); // Reinitialize
            wait(500); // Allow some time for reinitialization
            lastSuccessfulRead[reader] = millis(); // Reset last read time
            continue; // Skip to the next reader
        }

        // Proceed to read cards if the reader is active
        if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
            currentRFIDinput[reader] = getMemoryContent(reader);
            lastSuccessfulRead[reader] = millis(); // Update last successful read time
        } else {
            currentRFIDinput[reader] = 0; // No card present
        }

        // Check if we need to reinitialize the reader based on time
        if (millis() - lastSuccessfulRead[reader] >= reinitTimeLimit) {
            Serial.print("Reinitializing RFID reader "); 
            Serial.println(reader);
            mfrc522[reader].PCD_Init(ssPins[reader], rstPin); // Reinitialize
            wait(500); // Allow some time for reinitialization
            lastSuccessfulRead[reader] = millis(); // Reset last read time after reinit
        }

        // Prepare output only if the reader has successfully reported
        if (currentRFIDinput[reader] != lastRFIDinput[reader]) {
            for (byte i = 0; i < NR_OF_READERS; i++) {
                output = output + String(currentRFIDinput[i]) + " ";
                Serial.print(currentRFIDinput[i]); 
                Serial.print(" ");
            }
            Serial.println();
            client.publish(dataChannel, output.c_str());
            lastRFIDinput[reader] = currentRFIDinput[reader];
        }
    } // for
    if (!RfidRandom) {
        updateLEDsBasedOnRFID(); // Handle LEDs based on card presence
    }
}




byte getMemoryContent(byte reader, byte block, byte trailerBlock, byte index) {
  //trailerBlock is the highest block in the sector, for example, block 0 is in sector 0, so its trailerBlock is block 3. Block 3's trailerBlock is also block 3.
  //block goes from 0 - 63
  //index goes from 0 - 15
  byte readBuffer[18];
  byte readBufferSize = sizeof(readBuffer);
  MFRC522::StatusCode status;


  // Authenticate using key A
  //Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode)mfrc522[reader].PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522[reader].uid));
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
//----------------------------------Additional Functions----------------------
void updateLEDsBasedOnRFID() {
    for (int reader = 0; reader < NR_OF_READERS; reader++) {
        if (currentRFIDinput[reader] != 0) {  // Non-zero means a card is present
            for (int led = reader * 8; led < (reader + 1) * 8; led++) {
                leds[led] = CRGB::Green;  // Turn LED green
            }
        } else {
            for (int led = reader * 8; led < (reader + 1) * 8; led++) {
                leds[led] = CRGB::Red;  // Turn LED red
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
