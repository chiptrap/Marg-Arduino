//This is Full Functional Comms Code as of 7_23_24
//Need to set Section 1 to specific comms station

#include <U8g2lib.h>
#include <Wire.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//-----------------SECTION 1---------------------------------------
#define Comms1 "--Engineering--"
#define Comms2 "-Cargo Bay-"
#define Comms3 "----Infirmary----"
#define Comms4 "-----Security-----"

#define mqttComms "/Renegade/Room1/Comms4/" //Set specific comms channel here
#define mqttCommsReset "/Renegade/Room1/Comms4/Reset/" //Set specific comms channel here
#define mqttCommsAll "/Renegade/Room1/CommsAll/" //Set channel to address ALL comms consoles
#define mqttCommsSet "/Renegade/Room1/Comms4Set/" //Set specific comms Set channel here
#define commsChannel "Comms 4" //Set Comms channel identifier here
#define radioId "RADIO ID # 23" //Set Radio Id # here
#define otaHostName "Comms 4" //Set unique identifier here
const char* correctStation = Comms4;

String predefinedNumber = "2468.3";//Set  default correct frequency here
//-------------------END SECTION 1---------------------------------------------------

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
unsigned long interval = 10000;
WiFiClient wifiClient;
#define NUM_SUBSCRIPTIONS 1
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Comms/LEDs/"
  //"myTopic0",
  //"myTopic1",
  //additional subscriptions can be added here, separated by commas
};
PubSubClient client(wifiClient);

// FastLED parameters
#include <FastLED.h>
#define DATA_PIN D7
#define NUM_LEDS 16
#define MAX_POWER_MILLIAMPS 1000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];
byte hueArray[16] = {128, 0, 64, 96, 64, 96, 128, 0, 96, 96, 128, 64, 128, 0, 128, 96};
//byte hueArray[4][4] = {
//  {0, 0, 0, 0},         //button 1
//  {25, 25, 25, 25},     //button 2
//  {50, 50, 50, 50},     //button 3
//  {75, 75, 75, 75}      //button 4
//};

//void lightButton(byte button, byte hue0, byte hue1, byte hue2, byte hue3) {
//  hueArray[button][0] = hue0;
//  hueArray[button][1] = hue1;
//  hueArray[button][2] = hue2;
//  hueArray[button][3] = hue3;
//  for (int i = 0; i < 4; i++) {
//    leds[i + (4 * button)] = hueArray[button][i];
//  }
//  FastLED.show();
//}

//bool lock = false;
bool digitsLocked = false;
bool lockedDisplay = false;
bool radioIdDisplay = false;
bool lockedDept = false;
bool deptLocked = false;
bool btnHeld = false;
bool buttonPressed = false;
bool unk = false;
bool lock = false;
//-----------------------------------------------Mode Logic-----------------------------------------------
boolean modeActive = false;
boolean quit = false;
uint32_t timebase = 0;



// Initialize the library for the SSD1309
U8G2_SSD1309_128X64_NONAME0_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ D1, /* data=*/ D2, /* reset=*/ U8X8_PIN_NONE);

// Pins connected to the 74HC165 shift registers
const int loadPin = D3;  // PL pin
const int clockPin = D5; // CP pin
const int dataPin = D6;  // Q7 pin

// Analog pin to read the analog value
const int analogPin = A0;

// Number of shift registers
const int numShiftRegisters = 4;

// Store the previous value of the complete number
String previousCompleteNumber = "";

//Button pin to read button
const int buttonPin = D4;

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
  ArduinoOTA.setHostname(otaHostName);

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
      client.publish("/Renegade/Room1/", commsChannel" Online");
      // ... and resubscribe
      client.subscribe(mqttComms);
      client.subscribe(mqttCommsSet);
      client.subscribe(mqttCommsAll);
      for (int i = 0; i < NUM_SUBSCRIPTIONS; i++) {
        client.subscribe(subscribeChannel[i]);
      }
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
  int val = message.toInt();//.tostrf(2,2);//toInt()
  /*
    Serial.println("-------new message from broker-----");
    Serial.print("channel:");
    Serial.println(topic);
    Serial.print("data:");
    Serial.write(payload, length);
    Serial.println();
  */
  if (message == "quit" && modeActive) {
    quit = true;
    FastLED.clear();
    FastLED.show();    
  }
  //-----------------------------------------------Mode Handling-----------------------------------------------

  if (!modeActive) {
    modeActive = true;

    if (message == "commsPuzzle") {
      //announce puzzle start (Only comms 1 does this)
      if (commsChannel == "Comms 1") {
        client.publish("/Renegade/Room1/", "Starting Comms Puzzle");
      }

      //run puzzle
      timebase = millis();
      while (!quit) {
        commsPuzzle();
      }
      u8g2.clearBuffer();            
      fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
      FastLED.show();

    } else if (!strcmp(subscribeChannel[0], topic)) {
      switch (val) {
        case 0:
          //"Low Power" mode
          u8g2.clearBuffer();
          u8g2.setDrawColor(1);
          u8g2.setFont(u8g2_font_spleen5x8_mf);
          u8g2.drawStr(0, 16, "critical error:");
          u8g2.drawStr(0, 26, "onboard power low");
          u8g2.sendBuffer();
          while (!quit) {
            fill_solid(leds, NUM_LEDS, CHSV(0, 255, 100));
            FastLED.show();
            wait(175);
            fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
            FastLED.show();
            wait(175);

            fill_solid(leds, NUM_LEDS, CHSV(0, 255, 100));
            FastLED.show();
            wait(175);
            fill_solid(leds, NUM_LEDS, CHSV(0, 255, 0));
            FastLED.show();

            timebase = millis();

            while (millis() - timebase < 3175) {
              //waits 3 seconds to restart animation, but if quit is received during this time, it will quit instantly.
              handleAll();
              if (quit) {
                break;
              }
            }
          }
          u8g2.clearBuffer();
          u8g2.sendBuffer();
          break;
        case 1:
          //Ambient mode
          u8g2.drawStr(0, 24, "comms unavailable          ");
          u8g2.sendBuffer();
          while (!quit) {
            EVERY_N_MILLISECONDS(500) {
              fill_solid(leds, NUM_LEDS, CHSV(128, 255, 255));
              for (int i = 0; i < 8; i++) {
                leds[random(NUM_LEDS)] = CHSV(random(256), 255, 255);
              }
              FastLED.show();
            }
            handleAll();
          }
          break;
        //case 2: add more animations here
        default:
          break;
      }
    }

    quit = false;
    modeActive = false;
    wait(100);
  }

  //-----------------------------------------------Frank's puzzle logic-----------------------------------------------
  if (!strcmp(topic, mqttCommsSet)) {
    predefinedNumber = message;
    String freq = String(predefinedNumber);
    client.publish("/Renegade/Room1/", freq.c_str());
    //checkConnection();
    client.publish("/Renegade/Room1/", commsChannel" Set");
  }

  else if (message == "unlock") {
    client.publish("/Renegade/Room1/", commsChannel" Unlocked");
    digitsLocked = false;
    lockedDisplay = false;
    deptLocked = false;
    lockedDept = false;
    lock = false;
    unk = false;
    previousCompleteNumber = "";
  }
  else if (message == "lock") {
    client.publish("/Renegade/Room1/", commsChannel" Locked");
    deptLocked = true;
    unk = false;
  }
  
  else if (message == "resetOk?") {
    reportFrequency();
  }
  else if (message == "report") {
    client.publish("/Renegade/Room1/", commsChannel" updated");
    //reportFrequency();
    previousCompleteNumber = "";
    unk = false;
  }
  else if (message == "lockFreq") {
    client.publish("/Renegade/Room1/", commsChannel" Freq Locked");
    digitsLocked = true;
    lock = true;
    unk = false;
  }
  else if (message == "Engineering" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Engineering");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(8, 36, "ENGINEERING"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;


  }
  else if (message == "Docking" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Docking");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(8, 36, "CARGO BAY"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Infirmary" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Infirmary");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(17, 36, "INFIRMARY"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Security" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Security");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(25, 36, "SECURITY"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;
  }
  else if (message == "Bridge" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Docking");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(28, 36, "BRIDGE"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Diagnostics" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Docking");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(8, 36, "SERVICE"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Research" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Docking");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(14, 36, "RESEARCH"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Telemetry" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Docking");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(14, 36, "TELEMETRY"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;

  }
  else if (message == "Unknown" && digitsLocked && !deptLocked) {
    client.publish("/Renegade/Room1/", commsChannel" Unknown");
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 17, 128, 28); //Clear the
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(20, 36, "UNKNOWN"); // Add header at the top
    u8g2.sendBuffer();
    unk = false;
  }


  else if (message == "allowButton") {
    client.publish("/Renegade/Room1/", commsChannel" Button Allowed");
    buttonPressed = false;
  }
  else if (message == "buttonsOn") {
    client.publish("/Renegade/Room1/", commsChannel" Button Led's On");
    for (int i = 0; i < 16; i++) {
      leds[i] = CHSV(hueArray[i], 255, 255);
    }
    FastLED.show();
  }
  else if (message == "buttonsBlue") {
    //This all happens when the puzzle is completed
    client.publish("/Renegade/Room1/", commsChannel" Button Led's On");
    for (int i = 0; i < 16; i++) {
      leds[i] = CHSV(128, 255, 255);
    }
    FastLED.show();
    //Start drive signature puzzle
    //wait(3500);
    //client.publish("/Renegade/Room1/Station1DriveSignature/Control/", "positionPuzzleMode");
  }
  else if (message == "buttonsOff") {
    client.publish("/Renegade/Room1/", commsChannel" Button Led's Off");
    FastLED.clear();
    FastLED.show();
  }

}

void setup() {
  // Initialize the display
  u8g2.begin();
  Serial.begin(115200); // Initialize serial communication for printing

  // Set the pins as outputs or inputs
  pinMode(loadPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // FastLED Setup
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  fill_solid(leds, NUM_LEDS, CHSV(0, 0, 0));
  FastLED.show();


  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
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
    client.publish("/Renegade/Room1/", commsChannel" Watchdog");
    //reportFrequency();
    previousMillis = currentMillis;

  }
}
void wait(uint16_t msWait) {
  uint32_t start = millis();
  while ((millis() - start) < msWait) {
    handleAll();
  }
}

void handleAll() {
  ArduinoOTA.handle();
  client.loop();
  checkConnection();
  yield();
  readBtn();
}

void loop() {
  handleAll();

  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_spleen5x8_mf);
  u8g2.drawStr(0, 16, "C 2099 RennSoft            ");
  u8g2.sendBuffer();

  u8g2.drawStr(0, 24, "comms unavailable.         ");
  u8g2.sendBuffer();
  wait(800);
  u8g2.drawStr(0, 24, "comms unavailable..        ");
  u8g2.sendBuffer();
  wait(800);
  u8g2.drawStr(0, 24, "comms unavailable...       ");
  u8g2.sendBuffer();
  wait(800);




}

void commsPuzzle() {
  if (unk) { //Runs 1 time each time "unk" is set to true
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 45); //Clear the Lines
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_ncenB12_tr); // Choose a suitable font
    u8g2.drawStr(6, 16, "SET DEPT ID"); // Add header at the top
    u8g2.setFont(u8g2_font_ncenB10_tr); // Choose a suitable font
    u8g2.drawStr(20, 36, "UNKNOWN"); // Add header at the top
    u8g2.sendBuffer();
    wait(100);
    unk = false;
    return;
  }
  if (deptLocked) { //Loops back to here when deptLocked is true
    if (!lockedDept) { //draws 1 time
      u8g2.clearBuffer();
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_ncenB12_tr); // Choose a suitable font
      u8g2.drawStr(0, 14, correctStation); // Add footer at the bottom
      u8g2.setFont(u8g2_font_ncenB08_tr); // Choose a suitable font
      u8g2.drawStr(8, 32, "To contact: Press "); // Add footer at the bottom
      u8g2.drawStr(0, 48, "and hold the button"); // Add footer at the bottom
      u8g2.setFont(u8g2_font_ncenB12_tr); // Choose a suitable font
      u8g2.drawStr(0, 64, "CONTACT >>>"); // Add footer at the bottom
      u8g2.sendBuffer();
      lockedDept = true;
    }
    wait(50);//proccesses all code in wait function
    readBtnLocked();
    return;
  }

  if (digitsLocked) { //Loops back to here when digitsLocked is true
    if (!lockedDisplay) { //draws 1 time
      u8g2.setDrawColor(0);
      u8g2.drawBox(0, 41, 128, 23); //Clear the Lines
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_ncenB08_tr); // Choose a suitable font
      u8g2.drawStr(0, 54, "-------FREQUENCY-------"); // Add footer at the bottom
      u8g2.drawStr(0, 64, "----------LOCKED----------"); // Add footer at the bottom
      u8g2.sendBuffer();
      lockedDisplay = true;
      unk = true;
      for (int i = 0; i < 16; i++) {
        leds[i] = CHSV(hueArray[i], 255, 255);
      }
      FastLED.show();
    }
    wait(50);//proccesses all code in wait function
    return;
  }

  fill_solid(leds, NUM_LEDS, CHSV(0, 255, beatsin8(20, 0, 255, timebase)));
  FastLED.show();
  // Read the switch values from the shift registers
  uint32_t switchValues = readShiftRegisters();

  // Read the analog value and scale it to 0-9
  int analogValue = analogRead(analogPin);
  int scaledValue = map(analogValue, 0, 1023, 0, 9);

  // Update the OLED display with the switch positions and analog value
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr); // Choose a suitable font
  u8g2.drawStr(0, 8, "FOLLOW PROCEDURE"); // Add header at the top
  u8g2.drawStr(6, 18, "TO SET FREQUENCY"); // Add header at the top
  u8g2.setFont(u8g2_font_ncenB12_tr); // Choose a suitable font
  u8g2.drawStr(2, 60, radioId); // Set unique Radio Id

  String completeNumber = displaySwitchValues(switchValues, scaledValue);
  // Print the complete number if it has changed
  if (completeNumber != previousCompleteNumber) {
    u8g2.sendBuffer();
    //Serial.println(completeNumber);
    String frequency = String(completeNumber);
    client.publish(mqttComms, frequency.c_str());
    previousCompleteNumber = completeNumber;
  }

  if ((completeNumber == predefinedNumber) && lock) { //if correct frequency selected, "lock" didsplay
    client.publish(mqttComms, commsChannel" Locked");
    digitsLocked = true;
  }
  wait(10); // Wait for 1/10th a second before updating
}

uint32_t readShiftRegisters() {
  uint32_t value = 0;

  // Load the shift register
  digitalWrite(loadPin, LOW);
  delayMicroseconds(5);
  digitalWrite(loadPin, HIGH);
  delayMicroseconds(5);

  // Read each shift register
  for (int i = 0; i < numShiftRegisters * 8; i++) {
    value |= ((uint32_t)digitalRead(dataPin) << (numShiftRegisters * 8 - 1 - i));
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
  }
  value = ~value;
  return value;
}

String displaySwitchValues(uint32_t switchValues, int scaledValue) {
  int x = 0; // X position for the digit display
  int y = 40; // Y position for the digit display (adjusted to fit header)
  String completeNumber = ""; // To store the complete number as a string

  // Iterate over each switch (8 bits per switch)
  for (int i = 0; i < numShiftRegisters; i++) {
    uint8_t switchPosition = (switchValues >> (i * 8)) & 0xFF;
    int activePosition = 0;

    // Find the active contact position (only one bit should be set)
    for (int j = 0; j < 8; j++) {
      if (switchPosition & (1 << j)) {
        activePosition = j + 1; // Convert to 1-based position
        break;
      }
    }

    char buffer[2]; // Buffer to hold the switch position as a string
    sprintf(buffer, "%d", activePosition);
    completeNumber += String(activePosition);
    u8g2.setFont(u8g2_font_ncenB14_tr); // Choose a suitable font
    u8g2.drawStr(x + 15, y, buffer);
    x += 20; // Move to the next position for the next digit
  }

  // Add a decimal point and the scaled analog value after the last digit
  int dpX = x + 13; // Position for the decimal point
  u8g2.drawStr(dpX, y, ".");
  completeNumber += ".";
  char buffer[2]; // Buffer to hold the scaled analog value as a string
  sprintf(buffer, "%d", scaledValue);
  completeNumber += String(scaledValue);
  u8g2.drawStr(dpX + 8, y, buffer); // Position the scaled value after the decimal point

  return completeNumber;
}
void readBtn() {
  if (digitsLocked && !deptLocked) { //Only read button state when needed
    if (!buttonPressed && digitalRead (D4) == LOW) {
      client.publish(mqttComms, commsChannel" Button Pressed");
      buttonPressed = true;
      delay(10);
    }

    uint32_t held = millis();
    while (buttonPressed && !btnHeld && digitalRead (D4) == LOW) {
      if  ((millis() - held) > 1000) {
        client.publish(mqttComms, commsChannel" Button Held");
        btnHeld = true;
      }
      delay(10);
    }

    if (buttonPressed && digitalRead (D4) == HIGH) {
      client.publish(mqttComms, commsChannel" Button Released");
      buttonPressed = false;
      btnHeld = false;
      delay(10);
    }
  }
}

void readBtnLocked() {
  if (!buttonPressed && digitalRead (D4) == LOW) {
    client.publish(mqttComms, commsChannel" Button Pressed");
    buttonPressed = true;
    delay(10);
  }

  uint32_t held = millis();
  while (buttonPressed && !btnHeld && digitalRead (D4) == LOW) {
    if  ((millis() - held) > 1000) {
      client.publish(mqttComms, commsChannel" Button Held");
      btnHeld = true;
    }
    delay(10);
  }

  if (buttonPressed && digitalRead (D4) == HIGH) {
    client.publish(mqttComms, commsChannel" Button Released");
    buttonPressed = false;
    btnHeld = false;
    delay(10);
  }

}
void reportFrequency(){

  // Read the switch values from the shift registers
  uint32_t switchValues = readShiftRegisters();

  // Read the analog value and scale it to 0-9
  int analogValue = analogRead(analogPin);
  int scaledValue = map(analogValue, 0, 1023, 0, 9);

  String completeNumber = displaySwitchValues(switchValues, scaledValue);
  String frequency = String(completeNumber);
  client.publish(mqttCommsReset, frequency.c_str());

  
}
