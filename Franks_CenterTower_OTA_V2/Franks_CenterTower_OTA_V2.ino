#include <PubSubClient.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

unsigned long previousMillis = 0;
const long interval = 10000;    // interval at which to send mqtt watchdog (milliseconds)

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

// FastLed Stuff
#define MAX_POWER_MILLIAMPS 30000
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    2816

int BRIGHTNESS   =      255;
//#define FRAMES_PER_SECOND  120

#define DATA_PIN_0 25
#define DATA_PIN_1 26
#define DATA_PIN_2 27

CRGB leds0[NUM_LEDS];
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

boolean rainbow = true;

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
  ArduinoOTA.setHostname("RenegadeCenterColumn_Esp32_OTA");

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
      client.publish("/Renegade/Engineering/Center Column/", "Renegade Center Column Online");
      // ... and resubscribe
      client.subscribe("/Renegade/Engineering/");
      client.subscribe("/Renegade/Engineering/CenterColumn/");
      client.subscribe("/Renegade/Engineering/CenterColumn/brightness/");


    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //wait(1000);
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
  if (message == "on") {
    client.publish("/Renegade/Engineering/CenterColumn/", "Center Column On Requested");
    rainbow = true;
    //leds_white();
    //MqttStop = false;
    //CloseDoor();
    //disable = false;
    //mqttRst = true;
  } else if (message == "off") {
    client.publish("/Renegade/Engineering/CenterColumn/", "Center Column Off Requested");

    fill_solid(leds0, NUM_LEDS, CRGB::Black);
    FastLED.show();
    rainbow = false;
    //MqttStop = false;

    //OpenDoor();
    //mqttSolve = true;
  }
  else if (message == "green") {
    client.publish("/Renegade/Engineering/CenterColumn/", "Center Column Green Requested");
    //leds_solved();
    //MqttStop = true;
  }
  //else if (message == "brightness") {
  else if (!strcmp(topic, "/Renegade/Engineering/CenterColumn/brightness/")) {
    BRIGHTNESS = val;
    // Report current input status via MQTT
    //reportInputStatus();
    //reportSwitcheStatus();
    client.publish("/Renegade/Engineering/CenterColumn/", "Center Column Brightness Set");
  }
}

void checkConnection() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= 1500)) {
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
    int rssi = WiFi.RSSI();
    int channel = WiFi.channel();

    //Serial.print("RSSI: ");//uncomment to print to serial
    ///Serial.println(rssi);

    // Publish RSSI to MQTT
    publishRSSI(rssi);
    publishWiFiChannel(channel);
    client.publish("/Renegade/Engineering/", "Center Tower Watchdog");
    previousMillis = currentMillis;

  }

}

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(500);  // Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  while (!Serial);           // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN_0, COLOR_ORDER>(leds0, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN_1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN_2, COLOR_ORDER>(leds2, NUM_LEDS).setCorrection(TypicalLEDStrip);
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid( leds0, NUM_LEDS, CHSV(0, 0, 0));
  fill_solid( leds1, NUM_LEDS, CHSV(0, 0, 0));
  fill_solid( leds2, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();
}

void handleAll() {
  checkConnection();
  ArduinoOTA.handle();
  client.loop();
}

void wait(uint16_t msWait) {
  uint32_t start = millis();
  while ((millis() - start) < msWait) {
    handleAll();
  }
}
void loop() {
  //  while (rainbow) {
  //    ArduinoOTA.handle();
  //    client.loop();
  //    checkConnection();
  //    rainbow_beat();
  //    FastLED.show();
  //  }
  //leds0[map(beat16(60), 0, 65535, 0, 255)] = CHSV(0, 255, 255);

  EVERY_N_MILLISECONDS(50) {
    handleAll();
  }
  leds0[beatsin16(45, 0, NUM_LEDS)] = CHSV(0, 255, 255);
  leds1[beatsin16(45, 0, NUM_LEDS)] = CHSV(0, 255, 255);
  leds2[beatsin16(45, 0, NUM_LEDS)] = CHSV(0, 255, 255);
  fadeToBlackBy(leds0, NUM_LEDS, 10);
  FastLED.show();
}


// Function to Publish RSSI
void publishRSSI(int rssi) {
  String rssiString = String(rssi);
  client.publish("/Renegade/Engineering/WireTracePuzzle/Rssi/", rssiString.c_str());
}
// Function to Publish WiFi Channel
void publishWiFiChannel(int channel) {
  String channelString = String(channel);
  client.publish("/Renegade/Engineering/WireTracePuzzle/Channel/", channelString.c_str());
}

//void leds_white() {
//  fill_solid( leds, NUM_LEDS, CHSV(100, 0, 50));
//  FastLED.show();
//}
//
//void leds_solved() { //Lights all Leds to green
//  fill_solid( leds, NUM_LEDS, CHSV(95, 255, 100));
//  FastLED.show();
//}
//void leds_off() {
//  fill_solid( leds, NUM_LEDS, CHSV(0, 0, 0));
//  FastLED.show();
//}
//void rainbow_beat() {
//
//  uint8_t beatA = beatsin8(17, 0, 255);  // Starting hue
//  uint8_t beatB = beatsin8(13, 0, 255);
//  fill_rainbow(leds, NUM_LEDS, (beatA + beatB) / 2, 8);  // Use FastLED's fill_lightningbow routine.
//}
