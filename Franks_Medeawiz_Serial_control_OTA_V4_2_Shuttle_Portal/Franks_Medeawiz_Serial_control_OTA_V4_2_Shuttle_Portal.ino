#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//#include <SoftwareSerial.h>

//SoftwareSerial Serial(D2, D1); // RX, TX

#ifndef STASSID
#define STASSID "Control booth"
#define STAPSK  "MontyLives"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;
const char* mqtt_server = "192.168.86.101";

#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"

char mqttTopicPrefix[64] = "/Egypt/Shuttle/MedeaWiz/";  //Set to proper listening prefix
char mqttStatus[64] = "/Egypt/MedeaWiz/";                           //Set to proper reporting prefix
char mqttTopic[64];

bool estop = false;
bool solved = false;


/*
 * MedeaWiz command definitions.
#define CMD_REQUEST_NUMBER_OF_FILES   0xCB
#define CMD_HDMI_720P_50              0xD6
#define CMD_HDMI_720P_60              0xD7
#define CMD_HDMI_1080P_24             0xD8
#define CMD_HDMI_1080P_50             0xD9
#define CMD_HDMI_1080P_60             0xDA
#define CMD_HDMI_1080I_50             0xDB
#define CMD_HDMI_1080I_60             0xDC
#define CMD_ADD_E1                    0xE1
#define CMD_ADD_E2                    0xE2
#define CMD_ADD_E3                    0xE3
#define CMD_ADD_E4                    0xE4
#define CMD_ADD_E5                    0xE5
#define CMD_FULL_VOLUME               0xE7
#define CMD_MUTE_KEY                  0xE8
#define CMD_VOLUME_UP                 0xE9
#define CMD_VOLUME_DWN                0xEA
#define CMD_PLAY                      0xEF
#define CMD_PAUSE                     0xF0
#define CMD_FB                        0xF1
#define CMD_FF                        0xF2
#define CMD_PREV                      0xF3
#define CMD_NEXT                      0xF4
#define COMPLETED_PLAY                0xEE
#define LOOP_FILE                     0xFC
 
*/

unsigned long previousMillis = 0;
unsigned long interval = 30000;
WiFiClient wifiClient;

PubSubClient client(wifiClient); 

void setup_wifi() { 
  delay(10);
  
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("MedeaWiz_Shuttle_Portal_OTA");

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
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected() && (WiFi.status() == WL_CONNECTED)) {
    //Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      //Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish(mqttStatus,"MedeaWiz Portal Controller Online");
      // ... and resubscribe
      client.subscribe("/Egypt/Shuttle/MedeaWiz/");
      client.subscribe(mqttFullTopic("Play/"));
      client.subscribe(mqttFullTopic("Loop/"));
    } else {
      //Serial.print("failed, rc=");
      //Serial.print(client.state());
      //Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte *payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  //Serial.println("-------new message from broker-----");
  //Serial.print("channel:");
  //Serial.println(topic);
  //Serial.print("data:");
  //Serial.write(payload, length);
  //Serial.println();
  
//if (!strcmp(topic, mqttFullTopic("Play/"))) {
   // wait(2); 
    //dfmp3.playFolderTrack(Folder, value); //sd:/Folder/track
 // }  
  
  if (message == "0x01") {
    Serial.write(0x01);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x01 cmd received");
    
  }
  else if (message == "0x00") {
    Serial.write(0x00);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x00 cmd received");
    
  }
  else if (message == "0x02") {
    Serial.write(0x02);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x02 cmd received");
    
  }
  else if (message == "0x03") {
    Serial.write(0x03);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x03 cmd received");
    
  }
  else if (message == "0x04") {
    Serial.write(0x04);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x04 cmd received");
    
  }
  else if (message == "0x05") {
    Serial.write(0x05);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x05 cmd received");
    
  }
  else if (message == "0x06") {
    Serial.write(0x06);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x06 cmd received");
    
  }
  else if (message == "0x07") {    
    Serial.write(0x07);
    //checkConnection();
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle play 0x07 cmd received");
    
  }
   
  else if (message == "reset") {
    solved = false;
    estop = false;
    client.publish("/Egypt/MedeaWiz/","MedeaWiz shuttle reset cmd received");    
  }
  
  else if (message == "loopRenegade") {    
    byte command[] = {0xFC, 0x00};
    Serial.write(command, 2);    
    client.publish("/Egypt/MedeaWiz/","MedeaWiz Shuttle Set looping to 000 cmd received");    
  }
  /*
  else if (message == "openShuttleDoor") {
    digitalWrite(D1,HIGH);    
    client.publish("/Egypt/MedeaWiz/","MedeaWiz openShuttleDoor cmd received");    
  }
  else if (message == "closeShuttleDoor") {
    digitalWrite(D1,LOW);    
    client.publish("/Egypt/MedeaWiz/","MedeaWiz closeShuttleDoor cmd received");    
  }
  */
    
}

void setup() {
  //Serial.begin(115200);
  Serial.begin(9600);// Serial to Sprite MedeaWiz  
  //Serial.println(__FILE__ __DATE__);
  Serial.setTimeout(500);// Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  pinMode(D1 , INPUT_PULLUP);
  pinMode(D2 , INPUT_PULLUP);
  //digitalWrite (D1,HIGH);//default to door closed. Low = relay off
}

void checkConnection() {
  unsigned long currentMillis = millis();
// if WiFi is down, try reconnecting
if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
  //Serial.print(millis());
  //Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  //WiFi.reconnect();
  previousMillis = currentMillis;
}
if ((!client.connected() ) && (WiFi.status() == WL_CONNECTED) ){
    //Serial.println("Lost mqtt connection");
    reconnect();
  }
if ((currentMillis - previousMillis >=interval)) {
    client.publish("/Egypt/MedeaWiz/", "MedeaWiz Shuttle Portal Watchdog");
    previousMillis = currentMillis;
    
  }
}
void wait(uint16_t msWait)
{
  uint32_t start = millis();

  while ((millis() - start) < msWait)
  {
    ArduinoOTA.handle();
    client.loop();
    checkConnection();
    //readButtons();
  }
}
char* mqttFullTopic(const char action[]) {
  strcpy (mqttTopic, mqttTopicPrefix);
  strcat (mqttTopic, action);
  return mqttTopic;
}

void loop() {
  
  checkConnection();
  client.loop();
  ArduinoOTA.handle();
  readInputs();
  //if (Serial.available()) {     // If anything comes in Serial1 (pins 0 & 1)
    //Serial.write(Serial.read());   // read it and send it out Serial (USB)
  //}
 
 delay(25); 
  
}
void readInputs() {
  if ((digitalRead(D2) == LOW) && (!estop)) {
    estop = true;
    client.publish("/Egypt/MedeaWiz/", "MedeaWiz Estop cmd received");
  }

  if ((digitalRead(D1) == LOW) && (!solved)) {
    solved = true;
    client.publish("/Egypt/MedeaWiz/", "MedeaWiz keypad correct code entered");
  }
}
