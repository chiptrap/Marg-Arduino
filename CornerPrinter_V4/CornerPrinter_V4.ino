//Working version.
//Date 9_26_24


#include <PubSubClient.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AccelStepper.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

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

bool usingSpiBus = false; //Change to true if using spi bus
//Base notifications
const char* hostName = "Engineering Corner Printer and Table"; // Set your controller unique name here
const char* quitMessage = ("Engineering Corner Printer and Table force quitting...");
const char* onlineMessage = ("Corner Controller Online");
const char* watchdogMessage = "Corner Controller Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1//Must set to match Qty in subscribeChannel below
const char* mainPublishChannel = "/Renegade/Engineering/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Engineering/CornerController/";
const char* dataChannel2 = "/Renegade/Engineering/CornerController/Health/";
const char* watchdogChannel = "/Renegade/Engineering/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Engineering/CornerController/Control/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};
//-----------------------------------------------FastLED-----------------------------------------------
#define DATA_PIN    18
#define DATA_PIN1    32
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    38
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          150
#define FRAMES_PER_SECOND  120
// How many leds are in the strip?
//-----------------------------------------------Stepper and Brake Beam-----------------------------------------------

/**
 * @brief AccelStepper instance for controlling a stepper motor
 * 
 * Creates a stepper motor controller using the FULL2WIRE (standard) mode,
 * which requires two pins: one for step (pin 26) and one for direction (pin 27).
 * The FULL2WIRE mode drives the stepper with just direction and step signals.
 * 
 * @param mode AccelStepper::FULL2WIRE - Indicates 2-wire control mode (step and direction)
 * @param stepPin 26 - Arduino pin connected to the step input of the stepper driver (orange wire)
 * @param dirPin 27 - Arduino pin connected to the direction input of the stepper driver (white wire)
 * 
 * @note Wire colors: orange (step), white (direction)
 */
AccelStepper stepper(AccelStepper::FULL2WIRE, 26, 27);//orange, white
#define frontBeam 23
#define backBeam  25
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;

unsigned long previousMillis = 0;
unsigned long interval = 10000;
unsigned long relay = 10000; //Set relay on time here
unsigned long previousRelay = 0;

boolean mqttRst = false;
boolean mqttSolve = false;

#define relay1 12
#define relay2 13
#define relay3 14
#define relay4 15
#define relay5 16
#define relay6 17
#define doorLock 19
#define doorClose 21

bool relayOn = false;

bool solved = false;
bool report = true;


void callback(char* topic, byte * payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  //client.publish("/Egypt/Archives/", "in the loop");
  uint32_t timestamp = 0;

  if (message == "quit" && modeActive) {
    //client.publish(mainPublishChannel, quitMessage);
    quit = true;
  }

  if (!modeActive) {

    if (message == "feedIn") {
      modeActive = true;
      stepper.setCurrentPosition(0);

      while (!digitalRead(frontBeam)) {
        if (quit) break;
        wait(1);
      }

      while (digitalRead(frontBeam)) {
        if (quit) break;
        handleAll();
        //Rule of thumb for stepper code, if you want it to constantly turn, use stepper.move, negative for backwards and positive for forwards. bigger numbers are faster.
        //Always call stepper.run(); right after, and make sure the two lines are being constantly called.
        stepper.move(-2000);
        stepper.run();
      }

      quit = false;
      modeActive = false;

    } else if (message == "feedOut") {
      modeActive = true;

      //If you want it to turn a certain distance, use this following sequence of code:
      stepper.setCurrentPosition(0);
      stepper.moveTo(50000);
      while (stepper.currentPosition() != 50000) {
        handleAll();
        if (quit) break;
        stepper.run();
      }

      quit = false;
      modeActive = false;
    }

  }


  if (message == "reset") {
    client.publish("/Renegade/Engineering/", "Renegade Corner reset requested");

  }
  else if (message == "solve") {

    client.publish("/Renegade/Engineering/", "Renegade Encoder puzzle solved by operator");
    mqttSolve = true;
  }
  
  else if (message == "green") {

    client.publish("/Renegade/Engineering/", "Renegade Green requested by operator");
    leds_green();
    //leds_green1();
  }
  else if (message == "blue") {

    client.publish("/Renegade/Engineering/", "Renegade Blue requested by operator");
    leds_blue();
    // leds_blue1();
  }
  else if (message == "bluegreen") {

    client.publish("/Renegade/Engineering/", "Renegade Blue/Green requested by operator");
    leds_green();
    //leds_blue1();
  }
  else if (message == "greenblue") {

    client.publish("/Renegade/Engineering/", "Renegade Green/Blue requested by operator");
    leds_blue();
    //leds_green1();
  }
  else if (message == "redblue") {

    client.publish("/Renegade/Engineering/", "Renegade Red/Blue requested by operator");
    leds_blue();
    //leds_red1();
  }
  else if (message == "redgreen") {

    client.publish("/Renegade/Engineering/", "Renegade Red/Green requested by operator");
    leds_red();
    //leds_green1();
  }
  else if (message == "red") {

    client.publish("/Renegade/Engineering/", "Renegade Red requested by operator");
    leds_red();
    //leds_red1();
  }
  else if (message == "off") {

    client.publish("/Renegade/Engineering/", "Renegade Off requested by operator");
    leds_off();
    //leds_off1();
  }


  else if (message == "oneUp") {
    client.publish("/Renegade/Engineering/", "Renegade One Up requested by operator");
    digitalWrite(relay2, LOW);
    digitalWrite(relay1, HIGH);
    relayOn = true;
    previousRelay = millis();
  }
  else if (message == "oneDown") {
    client.publish("/Renegade/Engineering/", "Renegade One Down requested by operator");
    digitalWrite(relay1, LOW);
    digitalWrite(relay2, HIGH);
    relayOn = true;
    previousRelay = millis();
  }

  else if (message == "twoUp") {
    client.publish("/Renegade/Engineering/", "Renegade Two Up requested by operator");
    digitalWrite(relay4, LOW);
    digitalWrite(relay3, HIGH);
    relayOn = true;
    previousRelay = millis();
  }
  else if (message == "twoDown") {
    client.publish("/Renegade/Engineering/", "Renegade Two Down requested by operator");
    digitalWrite(relay3, LOW);
    digitalWrite(relay4, HIGH);
    relayOn = true;
    previousRelay = millis();
  }

  else if (message == "threeUp") {
    client.publish("/Renegade/Engineering/", "Renegade Three Up requested by operator");
    digitalWrite(relay6, LOW);
    digitalWrite(relay5, HIGH);
    relayOn = true;
    previousRelay = millis();
  }
  else if (message == "threeDown") {
    client.publish("/Renegade/Engineering/", "Renegade Three Down requested by operator");
    digitalWrite(relay5, LOW);
    digitalWrite(relay6, HIGH);
    relayOn = true;
    previousRelay = millis();
  }
  else if (message == "doorUnlock") {
    client.publish("/Renegade/Engineering/", "Renegade Buttonwall Door Unlock requested by operator");
    digitalWrite(doorLock, HIGH);
    //relayOn = true;
    //previousRelay = millis();
  }
  else if (message == "doorLock") {
    client.publish("/Renegade/Engineering/", "Renegade Buttonwall Door Lock requested by operator");
    digitalWrite(doorLock, LOW);
    //relayOn = true;
    //previousRelay = millis();
  }




}




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
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  //FastLED.addLeds<LED_TYPE, DATA_PIN1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection(TypicalLEDStrip);
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid( leds, NUM_LEDS, CHSV(0, 0, 0));
  //fill_solid( leds1, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();

  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);
  pinMode(relay5, OUTPUT);
  pinMode(relay6, OUTPUT);
  pinMode(doorLock, OUTPUT);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  digitalWrite(relay5, LOW);
  digitalWrite(relay6, LOW);
  digitalWrite(doorLock, LOW);

  stepper.setMaxSpeed(5000);
  stepper.setAcceleration(5000);
  pinMode(frontBeam, INPUT);
  pinMode(backBeam, INPUT);
}




void loop() {
  EVERY_N_MILLISECONDS(1000) {
    //Serial.println("Loop");
  }
  handleAll();


  unsigned long currentRelay = millis();

  if ((currentRelay - previousRelay >= relay) && (relayOn)) {
    client.publish("/Renegade/Engineering/", "Corner Controller All Relays Off");
    digitalWrite(relay1, LOW);
    digitalWrite(relay2, LOW);
    digitalWrite(relay3, LOW);
    digitalWrite(relay4, LOW);
    digitalWrite(relay5, LOW);
    digitalWrite(relay6, LOW);
    relayOn = false;

  }



  wait (1);
}

void leds_blue() { //160 is Blue
  fill_solid( leds, NUM_LEDS, CHSV(160, 255, 30));
  FastLED.show();
}

void leds_red() { //160 is Blue
  fill_solid( leds, NUM_LEDS, CHSV(0, 255, 30));
  FastLED.show();
}

void leds_green() { //Lights all Leds to green
  fill_solid( leds, NUM_LEDS, CHSV(95, 255, 30));
  FastLED.show();
}

void leds_off() {
  fill_solid( leds, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();
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
