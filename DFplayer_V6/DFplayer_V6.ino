#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <DFMiniMp3.h>
#include<ESP8266WiFi.h>

#ifndef STASSID
#define STASSID "Control booth"
#define STAPSK  "MontyLives"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;
const char* mqtt_server = "192.168.86.102";

#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"
#define STATUS_PIN D5

WiFiClient wifiClient;

PubSubClient client(wifiClient);

boolean playing = false;
char mqttTopicPrefix[64] = "/Renegade/SFX2/";
char mqttStatus[64] = "/Renegade/SFX2/Status/";
char mqttTopic[64];
int Folder = 1;

// forward declare the notify class, just the name
//
class Mp3Notify;

SoftwareSerial secondarySerial(D2, D1); // RX, TX
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(secondarySerial);

// implement a notification class,
// its member methods will get called
//
class Mp3Notify
{
  public:
    static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
    {
      if (source & DfMp3_PlaySources_Sd)
      {
        Serial.print("SD Card, ");
      }
      if (source & DfMp3_PlaySources_Usb)
      {
        Serial.print("USB Disk, ");
      }
      if (source & DfMp3_PlaySources_Flash)
      {
        Serial.print("Flash, ");
      }
      Serial.println(action);
    }
    static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode)
    {
      // see DfMp3_Error for code meaning
      //Serial.println();
      //Serial.print("Com Error ");
      //Serial.println(errorCode);
    }
    static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track)
    {
      Serial.print("Play finished for #");
    }
    static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
    {
      PrintlnSourceAction(source, "online");
    }
    static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
    {
      PrintlnSourceAction(source, "inserted");
    }
    static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
    {
      PrintlnSourceAction(source, "removed");
    }
};

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected() && (WiFi.status() == WL_CONNECTED)) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish(mqttStatus, "Renegade SFX2 connected. Loading audio");
      // ... and resubscribe
      
      client.subscribe(mqttFullTopic(""));
      client.subscribe(mqttFullTopic("Play/"));
      client.subscribe(mqttFullTopic("Pause/"));
      client.subscribe(mqttFullTopic("Resume/"));
      client.subscribe(mqttFullTopic("Stop/"));
      client.subscribe(mqttFullTopic("Volume/"));
      client.subscribe(mqttFullTopic("Folder/"));
      client.subscribe(mqttFullTopic("Interrupt/"));
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte *payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  int value = message.toInt();
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();

  if (!strcmp(topic, mqttFullTopic("Play/"))) {
    wait(2); 
    dfmp3.playFolderTrack(Folder, value); //sd:/Folder/track
  }
  else if (!strcmp(topic, "/Egypt/globalPlay/")) {
    //dfmp3.playFolderTrack(Folder, value); //sd:/Folder/track
  }
  else if (!strcmp(topic, mqttFullTopic("Pause/"))) {
    dfmp3.pause();
  }
  else if (!strcmp(topic, mqttFullTopic("Resume/"))) {
    dfmp3.start();
  }
  else if (!strcmp(topic, mqttFullTopic("Stop/"))) {
    dfmp3.stop();
  }
  else if (!strcmp(topic, mqttFullTopic("Volume/"))) {
    dfmp3.setVolume(value);
  }
  else if (!strcmp(topic, mqttFullTopic("Folder/"))) {
    Folder = value;
  }
  else if (!strcmp(topic, mqttFullTopic("Interrupt/")) & message != "Stop") {
    dfmp3.playAdvertisement(value); //sd:/advert/track
  }
  else if (!strcmp(topic, mqttFullTopic("Interrupt/")) & message == "Stop") {
    dfmp3.stopAdvertisement();
  }
}

void checkConnection() {
  if (!client.connected()) {
    reconnect();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(500);// Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  Serial.println("initializing...");

  dfmp3.begin();

  pinMode(STATUS_PIN, INPUT);

  uint16_t volume = dfmp3.getVolume();
  Serial.print("volume ");
  Serial.println(volume);
  dfmp3.setVolume(30);

  //uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  //Serial.print("files ");
  //Serial.println(count);

  client.publish(mqttStatus, "Renegade SFX2 ready");
}

void wait(uint16_t msWait)
{
  uint32_t start = millis();

  while ((millis() - start) < msWait)
  {
    // if you have loops with delays, its important to
    // call dfmp3.loop() periodically so it allows for notifications
    // to be handled without interrupts
    dfmp3.loop();
    client.loop();
    checkConnection();
    playing = !digitalRead(STATUS_PIN);
    //Serial.println(playing);
    delay(1);
  }
}

char* mqttFullTopic(const char action[]) {
  strcpy (mqttTopic, mqttTopicPrefix);
  strcat (mqttTopic, action);
  return mqttTopic;
}


void loop()
{
  checkConnection();
  client.loop();
}
