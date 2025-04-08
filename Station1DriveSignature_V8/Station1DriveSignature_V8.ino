
/**
   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing how to read data from more than one PICC to serial.
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid
   Example sketch/program showing how to read data from more than one PICC (that is: a RFID Tag or Card) using a
   MFRC522 based RFID Reader on the Arduino SPI interface.
   Warning: This may not work! Multiple devices at one SPI are difficult and cause many trouble!! Engineering skill
            and knowledge are required!
   @license Released into the public domain.
   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS 1    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required *
   SPI SS 2    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required *
   SPI MOSI    MOSI         11 / ICSP-4   51        41        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        42        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        43        ICSP-3           15
   More pin layouts for other boards can be found here: https://github.com/miguelbalboa/rfid#pin-layout

   SCK 18
   MOSI 23
   MISO 19

   RST 22 (you can change this)

*/
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//-----------------------------------------------------------WiFi,MQTT,OTA-----------------------------------------------------------
unsigned long previousMillis = 0;
const long interval = 10000;           // interval at which to send mqtt watchdog (milliseconds)
int lastValue;

const char* ssid = "Control booth";
const char* password = "MontyLives";
const char* mqtt_server = "192.168.86.102";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"

bool usingSpiBus = true; //Change to true if using spi bus
//Base notifications
const char* hostName = "Station 1 Drive Signature"; // Set your controller unique name here
const char* quitMessage = "Station 1 Drive Signature quitting current mode...";
const char* onlineMessage = "Station 1 Drive Signature Online";
const char* watchdogMessage = "Station 1 Drive Signature Watchdog";
const char* positionPuzzleSolvedMessage = "Drive Signature Solved";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Room1/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Station1DriveSignature/";
const char* dataChannel2 = "/Renegade/Room1/Station1DriveSignature/Health/";
const char* dataChannelRFID = "/Renegade/Room1/Station1DriveSignature/RFID/";
const char* dataChannelHallFX = "/Renegade/Room1/Station1DriveSignature/HallFX/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Station1DriveSignature/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);

//-----------------------------------------------RFID-----------------------------------------------

#define SS_1_PIN  25
#define SS_2_PIN  26
#define SS_3_PIN  27
#define SS_4_PIN  4
#define rstPin    22
#define NR_OF_READERS 4
bool resetReader = false;

byte ssPins[] = { SS_1_PIN, SS_2_PIN, SS_3_PIN, SS_4_PIN};

MFRC522 mfrc522[NR_OF_READERS];  // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

byte currentInput[NR_OF_READERS] = {0, 0, 0, 0};
byte lastInput[NR_OF_READERS] = {0, 0, 0, 0};
byte correctInput[NR_OF_READERS] = {1, 4, 3, 2};
//      1 RED
//      2 GREEN
//      3 BLUE
//      4 YELLOW
//-----------------------------------------------Hall FX-----------------------------------------------
const byte NUM_HALL = 4;
const byte hallPins[NUM_HALL] = {32, 33, 34, 35};
int hallInput[NUM_HALL] = {0, 0, 0, 0};
int lastHallInput[NUM_HALL] = {0, 0, 0, 0};
int upperBounds[NUM_HALL];
int lowerBounds[NUM_HALL];
int thresholds[NUM_HALL] = {2625, 2574, 2584, 2681}; //2850 bigger number is more sensitive This was average of 5 auto cal on 10_4_24
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
//-----------------------------------------------------------End Header-----------------------------------------------------------

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
    quit = true;
  } 
  else if (message == "ResetRfid") {
     resetReader = true;
     //readAllRFID();
    } 
  else if (message == "setUpperBound") {
    for (int i = 0; i < NUM_HALL; i++) {

      //Take 50 readings and average
      for (int j = 0; j < 100; j++) {
        upperBounds[i] = upperBounds[i] + analogRead(hallPins[i]);
      }
      upperBounds[i] = upperBounds[i] / 100;
      Serial.print(upperBounds[i]);
      Serial.print(" ");
    }
    client.publish(dataChannel, "Upper Bounds Set");
    Serial.println();

  } else if (message == "setLowerBound") {
    for (int i = 0; i < NUM_HALL; i++) {

      //Take 50 readings and average
      for (int j = 0; j < 100; j++) {
        lowerBounds[i] = lowerBounds[i] + analogRead(hallPins[i]);
      }
      lowerBounds[i] = lowerBounds[i] / 100;
      Serial.print(lowerBounds[i]);
      Serial.print(" ");
    }
    client.publish(dataChannel, "Lower Bounds Set");
    Serial.println();

    //Calculate threshold
    String output;
    for (int i = 0; i < NUM_HALL; i++) {
      int distance = ((upperBounds[i] - lowerBounds[i]) / 2)+25;//Added offset of +25
      thresholds[i] = lowerBounds[i] + distance;
      output = output + String(thresholds[i]) + " ";
      Serial.print(thresholds[i]);
      Serial.print(" ");
    }
    output = "New Thresholds: " + output;
    client.publish(dataChannel, output.c_str());
    Serial.println();
  }


  if (!modeActive) {
    modeActive = true;

    if (message == "positionPuzzleMode") {

      //tell dataChannel puzzle is starting
      client.publish(dataChannel, "Station 1 drive signature starting position puzzle");

      //tell console1 buttons to start sequence mode
      client.publish("/Renegade/Room1/Station1Buttons/Control/", "sequenceMode");

      while (!quit) {
        positionPuzzleMode();
      }
      client.publish(dataChannel, quitMessage);

    } else if (message == "myNewMode") {
      //add more modes here
      client.publish(dataChannel, quitMessage);
    }
    quit = false;
    modeActive = false;
  }

}

int getMemoryContentUltralight(byte reader, byte blockAddr = 4, byte trailerBlock = 7, byte index = 0); //default location for the card identifier
void readHallFX(boolean forcePrint = false);

void setup() {
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

  //-----------------------------------------------Hall FX-----------------------------------------------
  for (int i = 0; i < NUM_HALL; i++) {
    pinMode(hallPins[i], INPUT);
  }
}


void loop() {
  handleAll();
  //  for (int i = 0; i < 4; i++) {
  //    Serial.print(analogRead(hallPins[i]));
  //    Serial.print(" ");
  //  }
  //  Serial.println();
}


void positionPuzzleMode() {
  handleAll();
  readHallFX();
  String payload;

  for (int reader = 0; reader < NR_OF_READERS; reader++) {
    getMemoryContentUltralight(reader);

    if (currentInput[reader] != lastInput[reader]) {
      if (currentInput[reader] == 0) {
        //client.publish("/Renegade/Room1/", String("Card " + String(lastInput[reader]) + " removed").c_str());
        Serial.println(String("Tag " + String(lastInput[reader]) + " removed"));
      } else {
        //client.publish("/Renegade/Room1/", String(currentInput[reader]).c_str());
        Serial.println(currentInput[reader]);
      }
      Serial.print("New RFID States: ");
      for (int i = 0; i < NR_OF_READERS; i++) {
        payload = String(currentInput[NR_OF_READERS - 1 - i]) + payload;
        //        Serial.print(currentInput[i]);
        //        Serial.print(" ");
      }
      Serial.println();
      client.publish(dataChannelRFID, payload.c_str());
      lastInput[reader] = currentInput[reader];
    }

  }//for

  for (int i = 0; i < NR_OF_READERS; i++) {
    //Check if any RFIDs are in the wrong position OR if ANY hallInputs are 0, both cases we want to return.
    //This will reset the loop before we can send our "Puzzle Solved" messages.
    if ((currentInput[i] != correctInput[i]) || (hallInput[i] != 1)) {
      return;
    }     
  }
  wait(150);
  readHallFX();//read hallfx again to verify  position is correct
  for (int i = 0; i < NR_OF_READERS; i++) {
    //Check if any hallInputs are 0
    //This will reset the loop before we can send our "Puzzle Solved" messages.
    if (hallInput[i] != 1) {
      return;
    }     
  }

  Serial.println("Puzzle solved");

  //Tell the 8266 that controls the button inputs to switch to yellow, and also to begin allowing inputs.
  client.publish("/Renegade/Room1/Station1Buttons/Control/", positionPuzzleSolvedMessage);

  //Tell everyone in Room1 if you want
  client.publish(mainPublishChannel, positionPuzzleSolvedMessage);

  quit = true;
}

void readHallFX(boolean forcePrint) {
  boolean stateChanged = false;
  const byte averageCount = 20;
  String output;

  for (int i = 0; i < NUM_HALL; i++) {
    int temp = 0;
    for (int j = 0; j < averageCount; j++) {
      wait(1);//Was 2
      temp = temp + analogRead(hallPins[i]);
    }
    temp = temp / averageCount;
    hallInput[i] = temp;
    //    Serial.print(hallInput[i]);
    //    Serial.print(" ");
    if (hallInput[i] > thresholds[i]) {
      hallInput[i] = 0;
    } else {
      hallInput[i] = 1;
    }
  }
  //  Serial.println();

  for (int i = 0; i < NUM_HALL; i++) {
    if (hallInput[i] != lastHallInput[i]) {
      stateChanged = true;
      lastHallInput[i] = hallInput[i];
      break;
    } else {
      stateChanged = false;
    }
  }

  if (stateChanged || forcePrint) {
    Serial.print("Hall FX Sensor States: ");
    for (int i = 0; i < NUM_HALL; i++) {
      output = String(hallInput[i]) + output;
      Serial.print(hallInput[i]);
      Serial.print(" ");
    }
    client.publish(dataChannelHallFX, output.c_str());
    Serial.println();
  }
}

int getMemoryContentUltralight(byte reader, byte blockAddr, byte trailerBlock, byte index) {
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
  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);
  // Look for new cards

  delay(10);  //allow bus to settle

  mfrc522[reader].PCD_Init();

  if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
    status = (MFRC522::StatusCode)mfrc522[reader].MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522[reader].GetStatusCodeName(status));
    }
    currentInput[reader] = buffer[index];
    //    Serial.print("Reader 1");
    //    Serial.print(": ");
    //    Serial.println(tagID);


    mfrc522[reader].PICC_HaltA();
    mfrc522[reader].PCD_StopCrypto1();

  } else {
    currentInput[reader] = 0;
  }
  return currentInput[reader];
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

void connectToStrongestWiFi() {
  int n = WiFi.scanNetworks();  // Scan available networks

  if (n == 0) {
    Serial.println("No networks found");
    return;
  }

  int strongestSignal = -1000;  // Initialize to a very low signal strength
  String bestSSID = "";
  String bestBSSID = "";

  for (int i = 0; i < n; i++) {
    String foundSSID = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    String bssid = WiFi.BSSIDstr(i);

    // Check for the target SSID and get the network with the strongest signal
    if (foundSSID == ssid) {
      if (rssi > strongestSignal) {
        strongestSignal = rssi;
        bestSSID = foundSSID;
        bestBSSID = bssid;
      }
    }
  }

  if (bestSSID != "") {
    // Check if already connected to the strongest network
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == bestSSID && WiFi.BSSIDstr() == bestBSSID) {
      Serial.println("Already connected to the strongest network, no need to reconnect.");
      return;  // No need to reconnect, already connected to the best network
    }

    // If not connected to the strongest network, disconnect first
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Disconnecting from current WiFi...");
      WiFi.disconnect();  // Disconnect from current network
      delay(1000);        // Short delay to ensure proper disconnection
    }

    // Now connect to the strongest network
    Serial.printf("Connecting to strongest network: %s with BSSID: %s\n", bestSSID.c_str(), bestBSSID.c_str());
    WiFi.begin(bestSSID.c_str(), password);  // Connect to the strongest WiFi network

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 10) {
      delay(1000);
      Serial.print(".");
      tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
      publishWiFiInfo();
      publishIPAddress();
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect to WiFi.");
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
