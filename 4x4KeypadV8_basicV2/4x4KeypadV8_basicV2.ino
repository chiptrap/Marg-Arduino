#include <PubSubClient.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <MFRC522.h>
//-----------------------------------------------------------WiFi,MQTT,OTA-----------------------------------------------------------
unsigned long previousMillis = 0;
const long interval = 10000;           // interval at which to send mqtt watchdog (milliseconds)
const long interval2 = 2000;
const char* hostName = "4x4Keypad"; // Set your controller unique name here

const char* ssid = "Control booth";
const char* password = "MontyLives";
const char* mqtt_server = "192.168.86.102";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"

WiFiClient wifiClient;

PubSubClient client(wifiClient);
//-----------------------------------------------------------RFID-----------------------------------------------------------
#define RST_PIN         27          // Configurable, see typical pin layout above
#define SS_PIN          26         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::MIFARE_Key key;
int currentRFIDinput = 0;
int lastRFIDinput = 0;
boolean cardMode = false;

//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 32
#define NUM_LEDS 32
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];

//-----------------------------------------------------------Keypad-----------------------------------------------------------
// Use this example with the Adafruit Keypad products.
// You'll need to know the Product ID for your keypad.
// Here's a summary:
//   * PID3844 4x4 Matrix Keypad
//   * PID3845 3x4 Matrix Keypad
//   * PID1824 3x4 Phone-style Matrix Keypad
//   * PID1332 Membrane 1x4 Keypad
//   * PID419  Membrane 3x4 Matrix Keypad

#include "Adafruit_Keypad.h"

// define your specific keypad here via PID
#define KEYPAD_PID3844
// define your pins here
// can ignore ones that don't apply
#define C1    25
#define C2    22
#define C3    21
#define C4    17
#define R1    16
#define R2    15
#define R3    14
#define R4    13
// leave this import after the above configuration
#include "keypad_config.h"

//initialize an instance of class NewKeypad
Adafruit_Keypad keypad4x4 = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);
String input = "";
String finalInput = "";
char rawInput;
byte cursorPosition = 0;
byte codeLength = 4;

//-----------------------------------------------------------End Header-----------------------------------------------------------
byte getMemoryContent(byte block = 4, byte trailerBlock = 7, byte index = 0); //default location for the card identifier
void writeData(byte newData, byte sector = 1, byte blockAddr = 4, byte index = 0, byte trailerBlock = 7);

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
  publishWiFiInfo();
  publishIPAddress();
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
      client.publish("/Renegade/Room1/", "4x4 Keypad Online");//change this to required message
      // ... and resubscribe
      client.subscribe("/Renegade/Room1/4x4Keypad/");//change this to correct topic(s)
      client.subscribe("/Renegade/Room1/4x4Keypad/RFIDprogram/");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      wait(5000);
    }
  }

}

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
  if (message == "reset") {
    cursorPosition = 0;
    cardMode = false;
    Serial.println("Cursor position resetting");
    client.publish("/Renegade/Room1/", "4x4 keypad cursor resetting...");
  } else if (!strcmp(topic, "/Renegade/Room1/4x4Keypad/RFIDprogram/")) {
    mfrc522.PCD_Reset();
    delay(100);
    mfrc522.PCD_Init(); // Init MFRC522
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      writeData(val);
    } else {
      client.publish("/Renegade/Room1/4x4Keypad/Data/", "No card present to write to. Make sure to insert card before calling rewrite function.");
    }
  } else if (message == "get card info") {
    mfrc522.PCD_Reset();
    delay(100);
    mfrc522.PCD_Init(); // Init MFRC522
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      byte cardContent = getMemoryContent();
      client.publish("/Renegade/Room1/4x4Keypad/Data/", String(cardContent).c_str());
    } else {
      client.publish("/Renegade/Room1/4x4Keypad/Data/", "No card present to read. Make sure to insert card before calling read function.");
    }
  } else if (message == "enter card mode") {
    cardMode = true;
  } else if (message == "light") {

    fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
    for (int i = 0; i < 150; i++) {
      fadeToBlackBy(leds, NUM_LEDS, 5);
      FastLED.show();
    }
  }
}

void checkConnection() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval2)) {
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
    client.publish("/Renegade/Room1/", "4x4 Keypad Watchdog");//Change this to correct watchdog info
    previousMillis = currentMillis;
    reportFreeHeap();
    publishRSSI();

  }

}

//-----------------------------------------------------------Begin Base Code-----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(500);

  //RFID Reader Setup
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  // Prepare the key (used both as key A and as key B)
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  // FastLED Setup
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);

  fill_solid(leds, NUM_LEDS, CHSV(128, 255, 50));
  FastLED.show();

  keypad4x4.begin();
}

void loop() {
  handleAll();
  keypad4x4.tick();
  //fadeToBlackBy(leds, NUM_LEDS, 32);

  //send every keystroke
  while (keypad4x4.available()) {
    keypadEvent e = keypad4x4.read();
    wait(50);
    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      //fill_solid(leds, NUM_LEDS, CHSV(128, 255, 25));
      //FastLED.show();
      //Serial.print((char)e.bit.KEY);
      //Serial.println(" pressed");
      rawInput = (char)e.bit.KEY;
      input = String(rawInput);
      client.publish("/Renegade/Room1/4x4Keypad/Input/", input.c_str());
    }
  }


  //send every card reading
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    currentRFIDinput = getMemoryContent();
  } else {
    currentRFIDinput = 0;
  }

  if ((currentRFIDinput != lastRFIDinput) && (currentRFIDinput < 21)) {
    //      Serial.print("currentRFIDinput: ");
    //      Serial.print(currentRFIDinput);
    if (currentRFIDinput == 0) {
      client.publish("/Renegade/Room1/KeypadReader/", String("Card " + String(lastRFIDinput) + " removed").c_str());
      //Serial.println(String("Card " + String(lastRFIDinput) + " removed"));
    } else {
      client.publish("/Renegade/Room1/KeypadReader/", String(currentRFIDinput).c_str());
      //Serial.println(currentRFIDinput);
    }
    lastRFIDinput = currentRFIDinput;
  }


}

//-----------------------------------------------------------Functions-----------------------------------------------------------
void handleAll() {
  ArduinoOTA.handle();      // Handle over-the-air updates
  checkConnection();        // Monitor and maintain network connections
  client.loop();            // Process MQTT client events
  yield();                  // Allow the ESP8266 to handle background tasks
}

void wait(uint16_t msWait) {
  uint32_t start = millis();
  while ((millis() - start) < msWait)
  {
    handleAll();
  }
}

void dump_byte_array(byte * buffer, byte bufferSize, byte block) {
  Serial.println(String("Block " + String(block) + " content:"));
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
  return;
}

byte getMemoryContent(byte block, byte trailerBlock, byte index) {
  //trailerBlock is the highest block in the sector, for example, block 0 is in sector 0, so its trailerBlock is block 3. Block 3's trailerBlock is also block 3.
  //block goes from 0 - 63
  //index goes from 0 - 15
  byte readBuffer[18];
  byte readBufferSize = sizeof(readBuffer);
  MFRC522::StatusCode status;

  // Authenticate using key A
  //Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return 0;
  }

  //read block
  mfrc522.MIFARE_Read(block, readBuffer, &readBufferSize);
  //  dump_byte_array(readBuffer, 16, block);
  //
  //  Serial.print(String("Value in block " + String(block) + ", index " + index + ": "));
  //  Serial.println(readBuffer[index]);

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

  mfrc522.PCD_Reset();
  wait(5);
  mfrc522.PCD_Init(); // Init MFRC522

  return readBuffer[index];
}

//-----------------------------------------------writeData-----------------------------------------------

void writeData(byte newData, byte sector, byte blockAddr, byte index, byte trailerBlock) {
  // Show some details of the PICC (that is: the tag/card)
  //  Serial.print(F("Card UID:"));
  //  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size, blockAddr);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
          &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
          &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("This sample only works with MIFARE Classic cards."));
    return;
  }

  // In this sample we use the second sector,
  // that is: sector #1, covering block #4 up to and including block #7

  byte dataBlock[]    = {};
  dataBlock[index] = newData;

  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key B
  Serial.println(F("Authenticating using key B..."));
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Write data to the block
  Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
  Serial.println(F(" ..."));
  dump_byte_array(dataBlock, 16, blockAddr); Serial.println();
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.println();

  // Read data from the block (again, should now be what we have written)
  //  Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
  //  Serial.println(F(" ..."));
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
  dump_byte_array(buffer, 16, blockAddr); Serial.println();

  // Dump the sector data
  Serial.println(F("Current data in sector:"));
  mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);

  String msg = String("Wrote value '" + String(newData) + "' to sector " + String(sector) + ", block " + String(blockAddr) + ", index " + String(index) + " successfully.");
  client.publish("/Renegade/Room1/4x4Keypad/Data/", msg.c_str());

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

}

// Function to report the free heap memory over MQTT
void reportFreeHeap() {
  uint32_t freeHeap = ESP.getFreeHeap();  // Get free heap memory
  // Publish the heap memory data to the MQTT topic
  client.publish("/Renegade/Room1/4x4Keypad/Health/",String("FreeMemory:" +  String(freeHeap)).c_str());
  //Serial.print("Published free heap: ");
  //Serial.println(heapStr);  // Print to Serial for debugging
}
void publishRSSI() {
  int rssi = WiFi.RSSI();  
  client.publish("/Renegade/Room1/4x4Keypad/Health/", String("RSSI:" + String(rssi)).c_str());
}
void publishWiFiChannel() {
  int channel = WiFi.channel();  
  client.publish("/Renegade/Room1/4x4Keypad/Health/", String("Channel:" + String(channel)).c_str());  
}

// Function to publish WiFi info (SSID and BSSID) via MQTT
void publishWiFiInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    String connectedSSID = WiFi.SSID();         // Get SSID
    String connectedBSSID = WiFi.BSSIDstr();    // Get BSSID

    // Create MQTT message
    String message = "Connected to SSID: " + connectedSSID + ", BSSID: " + connectedBSSID;

    // Publish message to MQTT
    client.publish("/Renegade/Room1/4x4Keypad/Health/", message.c_str());

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
  if (client.publish("/Renegade/Room1/4x4Keypad/Health/", ipAddress.c_str())) {
    //Serial.println("IP Address published: " + ipAddress);
  } else {
    Serial.println("Failed to publish IP Address");
  }
}
