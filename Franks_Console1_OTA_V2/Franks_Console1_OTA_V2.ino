#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

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

unsigned long previousMillis = 0;
unsigned long interval = 30000;
int previousState[16] = {0}; // Initialize all inputs to Low
bool Initialized = false;
int expected_array[] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1}; //This is correct switch positions. 0=closed/correct
bool solved = false;


WiFiClient wifiClient;

PubSubClient client(wifiClient);

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
  ArduinoOTA.setHostname("Console_1_OTA");

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
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish("/Renegade/Engineering/", "Console 1 online");
      // ... and resubscribe
      client.subscribe("/Renegade/Engineering/Console1/");
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
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  if (message == "Console 1 Status") {
    checkConnection();
    client.publish("/Renegade/Engineering/", "Console 1 online");
    //mqttHint=true;
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
    client.publish("/Renegade/Engineering/", "Console 1 Watchdog");
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



// 165 Shift Register parameters
const int data165 = D2;  /* Q7 */
const int clock165 = D3; /* CP */
const int latch165 = D4;  /* PL */
const int numBits = 16;  /* Set to 8 * number of shift registers */
int buttonStates[numBits];
int arraySize = sizeof(buttonStates) / sizeof(buttonStates[0]);

//595 Shift Register Parameters
const int latch595 = D6;
const int clock595 = D5;
const int data595 = D7;

void setup()
{
  Serial.begin(115200);

  // 165 Shift Register Setup
  pinMode(data165, INPUT);
  pinMode(clock165, OUTPUT);
  pinMode(latch165, OUTPUT);

  //595 Shift Register Setup
  pinMode(latch595, OUTPUT);
  pinMode(clock595, OUTPUT);
  pinMode(data595, OUTPUT);

  Serial.setTimeout(500);// Set time out for
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();


}

void loop()
{
  PISOreadShiftRegister();  //Read in input states
  int convertedValue = binaryArrayToInt(buttonStates, arraySize);
  SIPOregisterOutput(convertedValue); //Write array state to outputs
  //wait(1);
}

void handleAll() {
  ArduinoOTA.handle();
  client.loop();
  checkConnection();
}

void PISOreadShiftRegister()
{
  // Step 1: Sample
  digitalWrite(latch165, LOW);
  delayMicroseconds(0.1);
  digitalWrite(latch165, HIGH);

  // Step 2: Shift
  //Serial.print("Bits: ");
  for (int i = 0; i < numBits; i++)
  {
    int bit = digitalRead(data165);
    if (bit != previousState[i] || (!Initialized)) {
      previousState[i] = bit; // Update previous state
      buttonStates[i] = bit; //Update current state
      // Publish state change over MQTT
      String message = "Input " + String(i + 1) + " changed to " + String(bit);
      client.publish("/Renegade/Engineering/Console1/", message.c_str());
      reportInputStatus();
    }
    //buttonStates[i] = bit;
    //Serial.print(buttonStates[i]);
    digitalWrite(clock165, HIGH); // Shift out the next bit
    delayMicroseconds(0.1);
    digitalWrite(clock165, LOW);
  }
  Initialized = true;
  //Serial.println();
  // Compare input state with expected array
  boolean match = compareInputState(buttonStates);

  // Publish result over MQTT
  if ((match) && !solved) {
    client.publish("/Renegade/Engineering/Console1/", "Console 1 Puzzle Solved");
    solved = true;
    //leds_solved();
  }
  if ((!match) && solved) {
    client.publish("/Renegade/Engineering/Console1/", "Console 1 Puzzle Not Solved");
    solved = false;
    //leds_white();
  }

}



int binaryArrayToInt(int binaryArray[], int size) {
  int result = 0;

  for (int i = 0; i < size; i++) {
    result = (result << 1) | binaryArray[i];
  }
  //Serial.println(result);
  return result;
}



void SIPOregisterOutput(int output) {
  // ST_CP LOW to keep LEDs from changing while reading serial data
  digitalWrite(latch595, LOW);
  // Shift out the bits
  shiftOut(data595, clock595, MSBFIRST, output);
  // ST_CP HIGH change LEDs
  digitalWrite(latch595, HIGH);
}

// Function to report the current input status of each input via MQTT
void reportInputStatus() {
  String message = "Current input status: ";
  for (int i = 0; i < 16; i++) {
    //String message = "Input " + String(i+1) + " changed to " + String(currentState);
    message += String ((buttonStates[i])) + ",";   //String message = String(digitalRead(inputPins[i])+ ",");
  }
  client.publish("/Renegade/Engineering/Console1/", message.c_str());
}

// Function to compare the input state with the expected array
boolean compareInputState(int buttonStates[]) {
  for (int i = 0; i < 16; i++) {
    if (buttonStates[i] != expected_array[i]) {
      return false;
    }
  }
  return true;
}
