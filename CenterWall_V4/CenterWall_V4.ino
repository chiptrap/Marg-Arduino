#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <FastLED.h>
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

bool usingSpiBus = false; //Change to true if using spi bus
//Base notifications
const char* hostName = "Center Wall LEDs"; // Set your controller unique name here
const char* quitMessage = "Center Wall LEDs quitting...";
const char* onlineMessage = "Center Wall LEDs Online";
const char* watchdogMessage = "Center Wall LEDs Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Engineering/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/CenterWallLEDs/data/";
const char* dataChannel2 = "/Renegade/CenterWallLEDs/data/Health/";
const char* watchdogChannel = "/Renegade/Engineering/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/CenterWallLEDs/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);

//-----------------------------------------------FastLED-----------------------------------------------
#define DATA_PIN    33
#define DATA_PIN1    32
#define DATA_PINV6   27
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    108
#define NUM_LEDS_V6 293

CRGB leds[NUM_LEDS];
CRGB leds1[NUM_LEDS];
CRGB ledsV6[NUM_LEDS_V6];

#define BRIGHTNESS         255
#define FRAMES_PER_SECOND  120

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

  if (!modeActive) {

    if (message == "ambient") {
      modeActive = true;
      while (!quit) {
        handleAll();
        byte speed = 20;

        leds[beatsin8(speed, 0, 53)] = CHSV(128, 255, 255);
        leds[beatsin8(speed, 54, 107)] = CHSV(128, 255, 255);

        leds[beatsin8(speed + 2, 0, 53)] = CHSV(180, 255, 255);
        leds[beatsin8(speed + 2, 54, 107)] = CHSV(180, 255, 255);

        leds[beatsin8(speed + 4, 0, 53)] = CHSV(115, 255, 255);
        leds[beatsin8(speed + 4, 54, 107)] = CHSV(115, 255, 255);
        fadeToBlackBy(leds, NUM_LEDS, 2);

        leds1[beatsin8(speed + 1, 0, 53)] = CHSV(150, 255, 255);
        leds1[beatsin8(speed + 1, 54, 107)] = CHSV(150, 255, 255);

        leds1[beatsin8(speed + 3, 0, 53)] = CHSV(175, 255, 255);
        leds1[beatsin8(speed + 3, 54, 107)] = CHSV(175, 255, 255);

        leds1[beatsin8(speed + 5, 0, 53)] = CHSV(120, 255, 255);
        leds1[beatsin8(speed + 5, 54, 107)] = CHSV(120, 255, 255);
        fadeToBlackBy(leds1, NUM_LEDS, 2);

        FastLED.show();
      }
      quit = false;
      modeActive = false;

    } else if (message == "doorActivity") {
      modeActive = true;
      client.publish(mainPublishChannel, "Door Activity CMD Recieved");
      //for (int i = 0; i < 500; i++) {
      while (!quit) {
        fill_gradient(ledsV6, 210, CHSV(128, 255, 255), NUM_LEDS_V6 - 1, CHSV(128, 255, 255));
        FastLED.show();
        wait(250);
        fill_gradient(ledsV6, 210, CHSV(128, 255, 0), NUM_LEDS_V6 - 1, CHSV(128, 255, 0));
        FastLED.show();
        wait(250);
      }
      quit = false;
      modeActive = false;
    }

  }

}

void setup() {  

  //------------------------------------------------------FastLED------------------------------------------------------
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PINV6, COLOR_ORDER>(ledsV6, NUM_LEDS_V6).setCorrection(TypicalLEDStrip);
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid( leds, NUM_LEDS, CHSV(0, 0, 0));
  fill_solid( leds1, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();

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
  fill_solid(ledsV6, NUM_LEDS_V6, CHSV(beatsin8(60), 255, 80));
  //  fill_solid(leds, NUM_LEDS, CHSV(0, 255, beatsin8(30, 0, 255)));
  //  fill_solid(leds1, NUM_LEDS, CHSV(0, 255, beatsin8(30, 0, 255, 375)));

  fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
  fill_solid(leds1, NUM_LEDS, CHSV(0, 255, 0));
  FastLED.show();

  wait(1000);

  fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
  fill_solid(leds1, NUM_LEDS, CHSV(0, 255, 255));
  FastLED.show();

  wait(1000);
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
