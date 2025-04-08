#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>

//-----------------------------------------------------------IO Extender board-----------------------------------------------------------
#define PCF8575_ADDR 0x20  // Replace with the actual address if different (Our boards require some soldering to change address)

// Define symbolic constants for each pin on the PCF8575
#define P00 0
#define P01 1
#define P02 2
#define P03 3
#define P04 4
#define P05 5
#define P06 6
#define P07 7
#define P10 8
#define P11 9
#define P12 10
#define P13 11
#define P14 12
#define P15 13
#define P16 14
#define P17 15
uint16_t pcf8575State = 0xFFFF;  // Initialize all pins to HIGH (all off)

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

bool usingSpiBus = false; //Change to true if using spi bus
//Base notifications
const char* hostName = "Renegade Solenoid Controller"; // Set your controller unique name here
const char* quitMessage = "Renegade Solenoid Controller quitting...";
const char* onlineMessage = "Renegade Solenoid Controller Online";
const char* watchdogMessage = "Renegade Solenoid Controller Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Engineering/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Engineering/Solenoid/";
const char* dataChannel2 = "/Renegade/Engineering/Solenoid/Health/";
const char* watchdogChannel = "/Renegade/Engineering/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Engineering/Solenoid/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);

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
    client.publish(mainPublishChannel, quitMessage);
    quit = true;
  }
 else if (message == "openDoor"){ 
  digitalWrite8575(P00, LOW);
  wait(50);
  digitalWrite8575(P00, HIGH);
 }
 else if (message == "closeDoor"){ 
  digitalWrite8575(P02, LOW);
  wait(50);
  digitalWrite8575(P02, HIGH);
 }
 else if (message == "centerDoor"){ 
  digitalWrite8575(P01, LOW);
  wait(50);
  digitalWrite8575(P01, HIGH);
 }
 else if (message == "extendSlide"){ 
  digitalWrite8575(P06, LOW);  
 }
 else if (message == "retractSlide"){ 
  digitalWrite8575(P06, HIGH);  
 }
 else if (message == "extendDrawer1"){ 
  digitalWrite8575(P07, LOW);  
 }
 else if (message == "retractDrawer1"){ 
  digitalWrite8575(P07, HIGH);  
 }
 else if (message == "extendDrawer2"){ 
  digitalWrite8575(P10, LOW);  
 }
 else if (message == "retractDrawer2"){ 
  digitalWrite8575(P10, HIGH);  
 }
 else if (message == "openGarage"){ 
  digitalWrite8575(P17, LOW);  
 }
 else if (message == "closeGarage"){ 
  digitalWrite8575(P17, HIGH);  
 }
  if (!modeActive) {

    if (message == "myMode1") {
      modeActive = true;
      //add more modes here
      quit = false;
      modeActive = false;

    } else if (message == "myNewMode") {
      modeActive = true;
      //add more modes here
      quit = false;
      modeActive = false;
    }

  }

}

void setup() {
  
    //------------------------------------------------------IO Expansion board------------------------------------------------------
  Wire.begin();               // Initialize I2C bus
  writePCF8575(pcf8575State); // Initialize all pins to HIGH

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
}


void loop() {
  handleAll();

  /*
  // Example usage for IO Extender: toggle two pins. Remember you are sinking current, so writing HIGH will turn the output OFF.
  digitalWrite8575(P00, HIGH);
  digitalWrite8575(P17, HIGH);
  wait(500);
  digitalWrite8575(P00, LOW);
  digitalWrite8575(P17, LOW);
  wait(500);
  */
}









// Function to write to a specific pin on the PCF8575
void digitalWrite8575(uint8_t pin, uint8_t value) {
  if (pin > 15) return;  // Ignore invalid pins

  // Update the bit in pcf8575State according to the pin and value
  if (value == HIGH) {
    pcf8575State |= (1 << pin);   // Set the corresponding bit to 1
  } else {
    pcf8575State &= ~(1 << pin);  // Clear the corresponding bit to 0
  }

  // Write the updated state to the PCF8575
  writePCF8575(pcf8575State);
}

// Function to write 16-bit data to PCF8575
void writePCF8575(uint16_t data) {
  Wire.beginTransmission(PCF8575_ADDR);
  Wire.write(data & 0xFF);        // Write lower 8 bits
  Wire.write((data >> 8) & 0xFF); // Write upper 8 bits
  Wire.endTransmission();
}

// Function to read 16-bit data from PCF8575 (if needed)
uint16_t readPCF8575() {
  Wire.requestFrom(PCF8575_ADDR, 2);  // Request 2 bytes from PCF8575
  if (Wire.available() == 2) {
    uint8_t lowByte = Wire.read();    // Read lower byte
    uint8_t highByte = Wire.read();   // Read upper byte
    return (highByte << 8) | lowByte; // Combine into 16-bit value
  }
  return 0;  // Return 0 if no data available
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
  //if (usingSpiBus){  SPI.begin();// Init SPI bus
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
