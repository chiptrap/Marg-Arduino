//This code runs the steppers at correct speed as well as moves all of the maze servos when selected
//Updates8_8_24

#include <AccelStepper.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <FastLED.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
unsigned long previousMillis = 0;
unsigned long previousMillisMarble = 0;
const long interval = 10000;           // interval at which to send mqtt watchdog (milliseconds)


boolean mqttRst = false;
boolean mqttRun1 = false;
boolean mqttRun2 = false;
boolean mqttRun3 = false;
boolean stopCirculation = true;


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
//Base notifications
const char* hostName = "Room 1 Maze Controller"; // Set your controller unique name here
const char* quitMessage = ("Room 1 Maze force quitting...");
const char* onlineMessage = ("Room 1 Maze Online");
const char* watchdogMessage = "Room 1 Maze Watchdog";

//mqtt Topics
#define NUM_SUBSCRIPTIONS 3//Must set to match Qty in subscribeChannel below
const char* mainPublishChannel = "/Renegade/Room1/Maze/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Room1/Maze/";
const char* dataChannel2 = "/Renegade/Room1/Maze/Health/";
const char* watchdogChannel = "/Renegade/Room1/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Room1/Maze/",
  "/Renegade/Room1/MazeControl/",
  "/Renegade/Engineering/Maze/"
  //more subscriptions can be added here, separated by commas
};
/*
client.subscribe("/Renegade/Room1/Maze/");
      client.subscribe("/Renegade/Room1/MazeControl/");
      //client.subscribe("/Renegade/Engineering/Maze/");
      */
//-----------------------------------------------FastLED-----------------------------------------------
// FastLED parameters
#define DATA_PIN 23
#define NUM_LEDS 92
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];

//PCA9685 specific setup below
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// our servo # counter
uint8_t servo1 = 14; //assign an unused servo position for startup
uint8_t servo2 = 15; //assign an unused servo position for startup
uint8_t servoGuide = 6;
uint8_t mazeRelease = 7;


// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);
// you can also call it with a different address and I2C interface
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);

// Depending on your servo make, the pulse width min and max may vary, you
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
uint16_t SERVOMIN = 290; // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
uint16_t SERVOCENT = 325; // This is the 'center position' pulse length count (out of 4096)
uint16_t SERVOMAX = 360; // This is the 'maximum' pulse length count (out of 4096)
#define USMIN  600 // This is the 'minimum' pulse length count (out of 4096)
#define USMAX  2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

uint16_t pulselen0 = 325 ; //This sets intial position of maze 1 servos
uint16_t pulselen1 = 325 ;

//Digital Inputs
int sense1 = 25;
int sense2 = 26;
int sense3 = 27;
int sense4 = 32;
int sense5 = 33;

bool maze1 = false;
bool maze2 = false;
bool maze3 = false;
bool mazeMarbleReady = false;
bool feedMaze3 = false;
bool joyActive = false;

bool s1 = true;
bool s2 = true;
bool s3 = true;
bool s4 = true;
/*
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
      client.publish("/Renegade/Room1/", "Renegade Maze puzzle online");
      // ... and resubscribe
      client.subscribe("/Renegade/Room1/Maze/");
      client.subscribe("/Renegade/Room1/MazeControl/");
      //client.subscribe("/Renegade/Engineering/Maze/");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      wait(5000);
    }
  }

}
*/
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  if (message == "stop") {
    client.publish("/Renegade/Room1/Maze/", "Maze Steppers Stopped");

    mqttRun1 = false;
    mqttRun2 = false;
    mqttRun3 = false;

  } else if (message == "run1") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper1 Started");
    mqttRun1 = true;
  }
  else if (message == "run2") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper2 Started");
    byte peakCounter;
    uint32_t timestamp = millis();

    while (peakCounter < 6) {
      if (beatsin8(60, 0, 255, timestamp) < 2) {
        peakCounter++;
      }
      fill_solid(leds, NUM_LEDS, CHSV(128, 255, beatsin8(60, 0, 255, timestamp)));
      FastLED.show();
      wait(1);
    }
    //    for (int i = 0; i < 2; i++) {
    //      fill_solid(leds, NUM_LEDS, CHSV(128, 255, 255));
    //      FastLED.show();
    //      wait(500);
    //      fill_solid(leds, NUM_LEDS, CHSV(128, 255, 0));
    //      FastLED.show();
    //      wait(500);
    //    }
    //      fill_solid(leds, NUM_LEDS, CHSV(128, 255, 255));
    //      FastLED.show();

    mqttRun2 = true;
  }
  else if (message == "run3") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper3 Started");
    mqttRun3 = true;
  }
  else if (message == "stop1") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper1 Stopped");
    mqttRun1 = false;
  }
  else if (message == "stop2") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper2 Stopped");
    mqttRun2 = false;
  }
  else if (message == "stop3") {
    client.publish("/Renegade/Room1/Maze/", "Maze Stepper3 Stopped");
    mqttRun3 = false;
  }  
  else if (message == "center" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle upDownCenter Requested");
        pwm.setPWM(servo2, 0, SERVOCENT);     
        pwm.setPWM(servo1, 0, SERVOCENT);    
  }
  else if (message == "up" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle up Requested");
        pwm.setPWM(servo2, 0, SERVOCENT);     
        pwm.setPWM(servo1, 0, SERVOMAX);    
  }
  else if (message == "down" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle Down Requested");
        pwm.setPWM(servo2, 0, SERVOCENT);      
        pwm.setPWM(servo1, 0, SERVOMIN);    
  }
  else if (message == "left" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle left Requested");
        pwm.setPWM(servo2, 0, SERVOMAX);    
        pwm.setPWM(servo1, 0, SERVOCENT);   
  }  
  else if (message == "right" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle right Requested");
        pwm.setPWM(servo2, 0, SERVOMIN);    
        pwm.setPWM(servo1, 0, SERVOCENT);
      }
  else if (message == "upRight" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle up Requested");
        pwm.setPWM(servo2, 0, SERVOMIN);      
        pwm.setPWM(servo1, 0, SERVOMAX);    
  }
  else if (message == "downRight" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle Down Requested");
        pwm.setPWM(servo2, 0, SERVOMIN);      
        pwm.setPWM(servo1, 0, SERVOMIN);    
  }
  else if (message == "upLeft" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle left Requested");
        pwm.setPWM(servo2, 0, SERVOMAX);    
        pwm.setPWM(servo1, 0, SERVOMAX);     
  }  
  else if (message == "downLeft" && joyActive) {
    client.publish("/Renegade/Room1/Maze/", "Maze Puzzle right Requested");
        pwm.setPWM(servo2, 0, SERVOMAX);    
        pwm.setPWM(servo1, 0, SERVOMIN);    
  }
  else if (message == "maze1") {
    startMaze1();
  }
  else if (message == "maze2") {
    startMaze2();
  }
  else if (message == "maze3") {
    startMaze3();
  }
  else if (message == "circulate") {
    client.publish("/Renegade/Engineering/Maze/", "Marble Circulate Requested");
    circulate();
    joyActive = false;
  }
  else if (message == "stopCirculate") {
    client.publish("/Renegade/Room1/Maze/", "Marble Circulate Stop Requested");
    pwm.setPWM(servoGuide, 0, 270);//Move "servoGuide" to position for marble run circulation
    pwm.setPWM(mazeRelease, 0, 240);//Move "mazeRelease" to stop position for maze circulation
    stopMarbleRun();
  }
  else if (message == "maze") {
    client.publish("/Renegade/Room1/Maze/", "Marble Maze Requested");
    pwm.setPWM(servoGuide, 0, 318);//Move "servoGuide" to position for maze circulation
    pwm.setPWM(mazeRelease, 0, 140);//Move "mazeRelease" to run position for maze circulation
    mqttRun1 = false;
    mqttRun2 = true;
    mqttRun3 = false;
  }
  else if (message == "startMazes") {
    client.publish("/Renegade/Room1/Maze/", "Renegade Maze Circulation Requested");
    startMazes();
  }
  else if (message == "Are you there?") {
    client.publish("/Renegade/Room1/Maze/", "Renegade Maze Puzzle OK");
  }
}
/*
void checkConnection() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    delay(100);
    connectToStrongestWiFi();
    //WiFi.begin(ssid, password);
    //WiFi.reconnect();
    previousMillis = currentMillis;
  }
  if ((!client.connected() ) && (WiFi.status() == WL_CONNECTED) ) {
    Serial.println("Lost mqtt connection");
    reconnect();
  }
  if ((currentMillis - previousMillis >= interval)) {
    client.publish("/Renegade/Room1/", "Room 1 Maze Watchdog");
    previousMillis = currentMillis;

  }

}
*/
// Define some steppers and the pins the will use
//AccelStepper stepper1; // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5
AccelStepper stepper1(AccelStepper::FULL2WIRE, 14, 15);//Round Wheel
AccelStepper stepper2(AccelStepper::FULL2WIRE, 16, 17);//elevator
AccelStepper stepper3(AccelStepper::FULL2WIRE, 18, 19);////4 legged wheel

void setup() {

  stepper1.setMaxSpeed(20000.0);
  //stepper1.setAcceleration(1000.0);
  stepper1.setSpeed(500.0);
  //stepper1.moveTo(24);

  stepper2.setMaxSpeed(300000.0);
  //stepper2.setAcceleration(1000.0);
  stepper2.setSpeed(300000.0);//set elevator screw speed
  //stepper2.moveTo(1000000);

  stepper3.setMaxSpeed(30000.0);
  //stepper3.setAcceleration(1000.0);
  stepper3.setSpeed(500.0);
  // stepper3.moveTo(1000000);

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

  //------------------------------------------------------FastLED------------------------------------------------------
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
  FastLED.show();
  //Servo Driver Setup Stuff
  pwm.begin();
  /*
     In theory the internal oscillator (clock) is 25MHz but it really isn't
     that precise. You can 'calibrate' this by tweaking this number until
     you get the PWM update frequency you're expecting!
     The int.osc. for the PCA9685 chip is a range between about 23-27MHz and
     is used for calculating things like writeMicroseconds()
     Analog servos run at ~50 Hz updates, It is importaint to use an
     oscilloscope in setting the int.osc frequency for the I2C PCA9685 chip.
     1) Attach the oscilloscope to one of the PWM signal pins and ground on
        the I2C PCA9685 chip you are setting the value for.
     2) Adjust setOscillatorFrequency() until the PWM update frequency is the
        expected value (50Hz for most ESCs)
     Setting the value here is specific to each individual I2C PCA9685 chip and
     affects the calculations for the PWM update frequency.
     Failure to correctly set the int.osc value will cause unexpected PWM results
  */

  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
  pwm.setPWM(0, 0, SERVOCENT);//set starting position of all servos here.
  pwm.setPWM(1, 0, SERVOCENT);
  pwm.setPWM(2, 0, SERVOCENT);
  pwm.setPWM(3, 0, SERVOCENT);
  pwm.setPWM(4, 0, SERVOCENT);
  pwm.setPWM(5, 0, SERVOCENT);
  pwm.setPWM(6, 0, 270);
  pwm.setPWM(7, 0, 240);
  delay(10);

  //Digital inputs
  pinMode(sense1, INPUT);
  pinMode(sense2, INPUT);
  pinMode(sense3, INPUT);
  pinMode(sense4, INPUT);
  pinMode(sense5, INPUT);



}



void loop() {
  handleAll();
  
  //  byte wave = beatsin8(20, 0, 255);

  if (digitalRead (sense1)) {
    if (!maze1) {
      startMaze1();
    }
  }
  if (digitalRead (sense2)) {
    if (!maze2) {
      pwm.setPWM(0, 0, SERVOCENT);//return maze 1 servos to center
      pwm.setPWM(1, 0, SERVOCENT);
      startMaze2();
      mqttRun2 = false;
    }
  }
  if (digitalRead (sense3)) {
    if (!maze3) {
      pwm.setPWM(2, 0, SERVOCENT);//return maze 2 servos to center
      pwm.setPWM(3, 0, SERVOCENT);
      mqttRun1 = false;
      startMaze3();
    }
  }
  if (digitalRead (sense4) && (!mazeMarbleReady)) { //This should be the sensor at base of tower behind maze start servo
    mazeMarbleReady = true;
    feedMaze3 = false;
    client.publish("/Renegade/Room1/Maze/", "Maze Marble in Ready Position");
    pwm.setPWM(4, 0, SERVOCENT);//return maze 3 servos to center
    pwm.setPWM(5, 0, SERVOCENT);
    stopMazes(); //Turn off all maze control
    mqttRun1 = false;//Stop all steppers
    mqttRun2 = false;
    mqttRun3 = false;
    joyActive = false;
  }
  if (digitalRead (sense5) && (maze2) && (!feedMaze3)) { //This should be the sensor at base of wheel
    mqttRun1 = true;
    feedMaze3 = true;

    client.publish("/Renegade/Room1/Maze/", "Maze Marble Moving to Maze 3");
    pwm.setPWM(servoGuide, 0, 318);//Move "servoGuide" to position for maze circulation

  }
  if (mqttRun1) {
    stepper1.runSpeed();
  }
  if (mqttRun2) {
    stepper2.runSpeed();
  }
  if (mqttRun3) {
    stepper3.runSpeed();
  }
  
     
  }


void startMaze1() {
  client.publish("/Renegade/Room1/Maze/", "Maze 1 Control Requested");
  pwm.setPWM(mazeRelease, 0, 240);//Move "mazeRelease" to position to stop maze circulation
  maze1 = true;
  maze2 = false;
  maze3 = false;
  feedMaze3 = false;
  joyActive = true;
  servo1 = 0;
  servo2 = 1;
  SERVOMAX = 360;
  SERVOMIN = 290;
  SERVOCENT = 325;

}

void startMaze2() {
  client.publish("/Renegade/Room1/Maze/", "Maze 2 Control Requested");
  maze1 = false;
  maze2 = true;
  maze3 = false;
  joyActive = true;
  servo1 = 2;
  servo2 = 3;
  SERVOMAX = 360;
  SERVOMIN = 290;
  SERVOCENT = 325;

}

void startMaze3() {
  client.publish("/Renegade/Room1/Maze/", "Maze 3 Control Requested");
  pwm.setPWM(servoGuide, 0, 270);//Move "servoGuide" to position for marble run circulation
  maze1 = false;
  maze2 = false;
  maze3 = true;
  joyActive = true;
  servo1 = 4;
  servo2 = 5;
  SERVOMAX = 360;
  SERVOMIN = 290;
  SERVOCENT = 325;

}
void stopMazes() {
  client.publish("/Renegade/Room1/Maze/", "All Maze Control Off");
  pwm.setPWM(servoGuide, 0, 270);//Move "servoGuide" to position for marble run circulation
  maze1 = false;
  maze2 = false;
  maze3 = false;
  joyActive = false;
  servo1 = 14; //select servo positions that are not in use
  servo2 = 15; //select servo positions that are not in use

}

void stopMarbleRun() {
  client.publish("/Renegade/Room1/Maze/", "Circulation Stop Requested");
  mqttRun1 = true;
  mqttRun2 = true;
  mqttRun3 = false;//Stop 4 legged wheel
  stopCirculation = true;
}
void startMazes() { //This should only be called after stopMarbleRun has completed
  mqttRun1 = false;
  mqttRun2 = true;
  mqttRun3 = false;
  pwm.setPWM(mazeRelease, 0, 110);//Move "mazeRelease" to position to start maze circulation
  wait(300);
  mazeMarbleReady = false;

}
void circulate() {
  pwm.setPWM(servoGuide, 0, 270);//Move "servoGuide" to position for marble run circulation
  pwm.setPWM(mazeRelease, 0, 240);//Move "mazeRelease" to stop position for maze circulation
  mqttRun1 = true;
  mqttRun2 = true;
  mqttRun3 = true;
  joyActive = false;
}
//------------------Service functions-------
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
