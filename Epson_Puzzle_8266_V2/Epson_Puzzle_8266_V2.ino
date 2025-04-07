#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
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
const long interval = 20000;           // interval at which to send mqtt watchdog (milliseconds)


//Base notifications
const char* hostName = "Epson_Puzzle"; // Set your controller unique name here
const char* quitMessage = "Epson_Puzzle force quitting...";
const char* onlineMessage = "Epson_Puzzle Online";
const char* watchdogMessage = "Epson_Puzzle Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 1
const char* mainPublishChannel = "/Renegade/Engineering/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Engineering/Epson_Puzzle/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel2 = "/Renegade/Engineering/Epson_Puzzle/Health/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* watchdogChannel = "/Renegade/Engineering/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/EpsonPuzzle/",
  //"myTopic0",
  //"myTopic1",
  //more subscriptions can be added here, separated by commas
};

WiFiClient wifiClient;
PubSubClient client(wifiClient);

//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
// Define the pins for the CD74HC4067 control lines
const int S0 = D1; // GPIO5
const int S1 = D2; // GPIO4
const int S2 = D3; // GPIO0
const int S3 = D4; // GPIO2

// Define the threshold value
int threshold = 140; // Adjust this value as needed

// Flag to store the previous status
bool previousStatus = false;
//-----------------------------------------------------------End Header-----------------------------------------------------------

void handleAll() {
  ArduinoOTA.handle();
  checkConnection();
  client.loop();
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

// Update threshold if the message is a valid number
  int newThreshold = message.toInt();
  if (newThreshold > 0 && newThreshold <= 1023) {
    threshold = newThreshold;
    client.publish(dataChannel, "Epson Puzzle Threshold Updated");
    Serial.print("Threshold updated to: ");
    Serial.println(threshold);
  } else {
    client.publish(dataChannel, "Epson Puzzle Threshold Not Updated");
    Serial.println("Invalid threshold value received");
  }
  
  if (message == "force quit" && modeActive) {
    client.publish(mainPublishChannel, quitMessage);
    quit = true;
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
  Serial.begin(115200);

  //------------------------------------------------------Wifi------------------------------------------------------
  Serial.setTimeout(500);  // Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  // Set up the control pins as outputs
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
}


void loop() {
  handleAll();
  wait(1000);
  Serial.println("Loop");
  bool allAboveThreshold = true; // Assume all are above threshold initially

  // Loop through the 9 channels
  for (int i = 4; i < 13; i++) {
    selectChannel(i); // Select the channel
    int analogValue = analogRead(A0); // Read the analog value

    // Check if the analog value is below the threshold
    if (analogValue <= threshold) {
      allAboveThreshold = false; // Set to false if any value is below the threshold
    }
    // Print the analog value
    Serial.print("Channel ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(analogValue);   
  }
  
  // Only output the result if there is a change in status
  if (allAboveThreshold != previousStatus) {
    if (allAboveThreshold) {
      Serial.println("All channels are above the threshold: TRUE");
      client.publish(dataChannel, "Epson Puzzle Solved");
      
    } else {
      Serial.println("Not all channels are above the threshold: FALSE");
      client.publish(dataChannel, "Epson Puzzle Not Solved");
    }
    previousStatus = allAboveThreshold; // Update the previous status
  }

  wait(1000);
}







//------------------------------------------------------Service functions------------------------------------------------------
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
    Serial.printf("Progress: % u % % \r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[ % u]: ", error);
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
  while (!client.connected() && (WiFi.status() == WL_CONNECTED)) {
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
        client.subscribe(subscribeChannel[i]);//change this to correct topic(s)
      }
    } else {
      Serial.print("failed, rc = ");
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
    reportRSSI();
    reportFreeHeap();
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

void reportRSSI() {
  int rssi = WiFi.RSSI(); // Get the RSSI value
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.println(" dBm");
  client.publish(dataChannel2, String("RSSI:" + String(rssi)).c_str());
  
}
// Function to report the free heap memory over MQTT
void reportFreeHeap() {
  uint32_t freeHeap = ESP.getFreeHeap();  // Get free heap memory
  // Publish the heap memory data to the MQTT topic
  client.publish(dataChannel2,String("FreeMemory:" +  String(freeHeap)).c_str());
  //Serial.print("Published free heap: ");
  //Serial.println(heapStr);  // Print to Serial for debugging
}


// Function to select a channel on the CD74HC4067



void selectChannel(int channel) {
  int controlPin[] = {S0, S1, S2, S3};

  int muxChannel[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };

  //loop through the 4 sig
  for(int i = 0; i < 4; i ++){
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  }

  
}
