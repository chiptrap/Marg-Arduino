//WARNING!! USE ESP32 BOARD VERSION 3.0.4!<<<<i have not seen this to be a problem Frank 10_3_24
//Restricted generateTwister to avoid buttons below zero based 6, and above 72.---------Removed this feature in this version-----------
//6 and 72 can be part of random sequence
#include <PubSubClient.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "arduino_secrets.h"

//Wifi,MQTT parameters
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;
const char* mqtt_server = "192.168.86.102";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_SERIAL_PUBLISH_CH "/icircuit/ESP32/serialdata/tx"
#define MQTT_SERIAL_RECEIVER_CH "/icircuit/ESP32/serialdata/rx"
#define MQTT_MSG_SIZE 256
WiFiClient wifiClient;
PubSubClient client(wifiClient);

bool usingSpiBus = false; //Change to true if using spi bus
//Base notifications
const char* hostName = "Button_Wall"; // Set your controller unique name here
const char* quitMessage = "Button Wall force quitting...";
const char* onlineMessage = "Button Wall Online";
const char* watchdogMessage = "Button Wall Watchdog";
//mqtt Topics  ***NOTE*****  This code has not been changed to use the topics below in the actual routines
#define NUM_SUBSCRIPTIONS 7//Must set to match Qty in subscribeChannel below
const char* mainPublishChannel = "/Renegade/Engineering/"; //typically /Renegade/Room1/ or /Renegade/Engineering/
const char* dataChannel = "/Renegade/Engineering/";
const char* dataChannel2 = "/Renegade/Engineering/ButtonWall/Health/";
const char* watchdogChannel = "/Renegade/Engineering/";
const char* subscribeChannel[NUM_SUBSCRIPTIONS] = {
  "/Renegade/Engineering/ButtonWall/",
  "/Renegade/Engineering/ButtonWall/Lighting/ButtonToLight/",
  "/Renegade/Engineering/ButtonWall/Lighting/Hue/",
  "/Renegade/Engineering/ButtonWall/Lighting/Fade/",
  "/Renegade/Engineering/ButtonWall/Connect4Difficulty/",
  "/Renegade/Engineering/ButtonWall/TwisterPlayerCount/",
  "/Renegade/Engineering/ButtonWall/TwisterRounds/"
};
/*
  ... and resubscribe
      client.subscribe("/Renegade/Engineering/ButtonWall/");
      client.subscribe("/Renegade/Engineering/ButtonWall/Lighting/ButtonToLight/");
      client.subscribe("/Renegade/Engineering/ButtonWall/Lighting/Hue/");
      client.subscribe("/Renegade/Engineering/ButtonWall/Lighting/Fade/");
      client.subscribe("/Renegade/Engineering/ButtonWall/Connect4Difficulty/");
      client.subscribe("/Renegade/Engineering/ButtonWall/TwisterPlayerCount/");
      client.subscribe("/Renegade/Engineering/ButtonWall/TwisterRounds/");
 */
unsigned long previousMillis = 0;
const long interval = 10000;


// FastLED parameters
#define DATA_PIN 32
#define NUM_LEDS 320
#define MAX_POWER_MILLIAMPS 10000
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255
CRGB leds[NUM_LEDS];

// 165 Shift Register parameters

const int dataPin1 = 25;  /* Q7 First 4 registers*/
const int dataPin2 = 23;  /* Q7 last 6 registers*/
const int clockPin = 26; /* CP */
const int loadPin = 27; /* PL */
const int numBits = 80;

boolean previousButtonStates[numBits];
boolean currentButtonStates[numBits];

//Connect 4
#define COLS 8
#define ROWS 7
int connect4Depth = 3; //this sets difficulty level
int mistakeProbability = 15; //Choose integer between 0 and 100. Indicates probability for AI to make a mistake (random input)
int play = 0; //Sets initial state of var Play for connect4 strategy
int playDepth = 6; //Sets initial number of best moves before mistakeProbability factor comes into play

//Lights Out
#define SIZE 6

//Twister
byte twisterRounds = 5;
byte twisterPlayerCount = 2;
bool twisterRoundBypass = false;
bool firstRoundTwister = false;

//light handler
byte fadeBy = 1;
byte globalHue = 0;
byte lightThisButton = 0;

//Function prototypes with default parameters
void generateTwisterSequence(boolean sequence[], uint8_t numButtons, boolean randomColor = false);

//Mode Flags
//bool defaultModeActive
boolean modeActive = false;
boolean forcequit = false;

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  int incomingValue = message.toInt();
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");
  Serial.write(payload, length);
  Serial.println();
  //State Logic:
  //Data passthrough
  if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Connect4Difficulty/")) {
    mistakeProbability = incomingValue;
    client.publish("/Renegade/Engineering/", String("New mistake probability set: " + String(mistakeProbability)).c_str());
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Connect4Depth/")) {
    connect4Depth = incomingValue;
    client.publish("/Renegade/Engineering/", String("New Depth set: " + String(connect4Depth)).c_str());
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Connect4PlayDepth/")) {
    playDepth = incomingValue;
    client.publish("/Renegade/Engineering/", String("New playDepth set: " + String(playDepth)).c_str());
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/TwisterRounds/")) {
    twisterRounds = incomingValue;
    client.publish("/Renegade/Engineering/", String("Twister rounds set: " + String(twisterRounds)).c_str());
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/TwisterPlayerCount/")) {
    twisterPlayerCount = incomingValue;
    client.publish("/Renegade/Engineering/", String("Twister player count set: " + String(twisterPlayerCount)).c_str());
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Lighting/Fade/")) {
    fadeBy = incomingValue;
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Lighting/Hue/")) {
    globalHue = incomingValue;
  }
  else if (!strcmp(topic, "/Renegade/Engineering/ButtonWall/Lighting/ButtonToLight/")) {
    lightThisButton = incomingValue;
  }

  //Force quit
  if (message == "Exit Mode (Forcequit)" && modeActive) {
    client.publish("/Renegade/Engineering/", "Attempting Forcequit");
    forcequit = true;
  } else if (message == "Twister round bypass") {
    client.publish("/Renegade/Engineering/", "Bypassing twister round");
    twisterRoundBypass = true;
  }

  //Mode select
  if (!modeActive) {
    if (message == "Play Connect 4 (AI)") {
      play = 0;
      modeActive = true;
      connect4(true);//True sets to player vs AI mode
      modeActive = false;
    }
    else if (message == "Play Connect 4 (2P)") {
      modeActive = true;
      connect4(false);
      modeActive = false;
    }
    else if (message == "Play Twister") {
      modeActive = true;
      firstRoundTwister = true;
      ringAnimation();
      twister(twisterRounds, twisterPlayerCount); //6 rounds, 5 players
      modeActive = false;
    }
    else if (message == "Play Lights Out") {
      modeActive = true;
      lightsOut();
      modeActive = false;
    }
    else if (message == "Play Match Pattern") {
      modeActive = true;
      drawMode();
      modeActive = false;
    }
    else if (message == "Play Memory Game") {
      modeActive = true;
      memoryGame();
      modeActive = false;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Booting");
  //MQTT Setup
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  //----Wifi setup----------
  WiFi.mode(WIFI_STA);
  delay(10);
  connectToStrongestWiFi();  

  //-----Start MQTT----------------
  //reconnectMQTT();This is handled inside connectToStrongestWiFi()
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

  // FastLED Setup
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  blackout();

  // 165 Shift Register Setup
  pinMode(dataPin1, INPUT);
  pinMode(dataPin2, INPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(loadPin, OUTPUT);
  memset(previousButtonStates, 0, numBits * sizeof(bool));
  memset(currentButtonStates, 0, numBits * sizeof(bool));


  // Seed the random number generator
  randomSeed(analogRead(2));
}

void loop() {

  //      byte ypos = map(beat8(60), 0, 255, 0, 7);
  //      byte ypos2 = beatsin8(30, 0, 7);
  //      byte hueWave = beatsin8(20, 120, 196);
  //      lightRow(ypos2, hueWave, 255, 255);
  //      fadeToBlackBy(leds, NUM_LEDS , beatsin8(30, 1, 20));
  //      EVERY_N_MILLISECONDS(200) {
  //        lightButton(random(80), random(256), 255, 255);
  //      }
  //      FastLED.show();

  lightButton(random(80), random(120, 130), 255, 255);
  fadeToBlackBy(leds, NUM_LEDS, 36);


  handleAll();

  //connect4(true);
  //  byte input = buttonPressed();
  //  lightButton(input, 0, 255, 255);
  FastLED.show();
  //  client.publish("/Renegade/Engineering/", String(input).c_str());
  PISOreadShiftRegister();

}

void animation1() {
  byte posWave = (beatsin8(60, 0, 9) + beatsin8(30, 0, 9, 0, beat8(40))) / 2;
  byte rowWave = (beatsin8(60, 0, 7) + beatsin8(30, 0, 7, 0, beat8(40))) / 2;
  lightColumn(posWave, beatsin8(15, 80, 196), 255, 255);
  lightRow(rowWave, beatsin8(45, 120, 196), 255, 255);

  //lightButton(map(beat8(30), 0, 255, 0, 79), beatsin8(45, 120, 196), 255, 255);
  //lightButton(79 - map(beat8(30), 0, 255, 0, 79), beatsin8(45, 0, 32), 255, 255);
  fadeToBlackBy(leds, NUM_LEDS, 8);
  FastLED.show();
}


//Modes
void connect4(boolean isSinglePlayerMode) {
  uint8_t input;
  uint8_t gameBoard[ROWS][COLS] = {};
  uint8_t minimaxBoard[ROWS][COLS] = {};
  uint8_t winningPositions[4][2];
  uint8_t currentPlayer;
  uint8_t activeColumn;
  uint8_t winningRow;
  uint8_t winningCol;
  uint8_t winningSequence[4];
  uint8_t border[20] = {2, 3, 4, 5, 6, 7, 15, 23, 31, 39, 47, 55, 63, 71, 74, 75, 76, 77, 78, 79};
  boolean piecePlaced;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < sizeof(border); i++) {
    lightButton(border[i], 135, 255, 255);
  }
  FastLED.show();

  client.publish("/Renegade/Engineering/", "Begin Connect 4");

  for (int i = 0; i < 28; i++) {
    for (int i = 0; i < sizeof(border); i++) { //refresh border in case of mqtt override
      lightButton(border[i], 135, 255, 255);
    }
    handleAll();
    currentPlayer = 1;
    client.publish("/Renegade/Engineering/", "Player 1 turn");
    input = getPlayerInput(currentPlayer);

    if (input == 90) {
      client.publish("/Renegade/Engineering/", "Forcequitting...");
      forcequit = false;
      blackout();
      return;
    }

    activeColumn = (input / 8) - 1;
    client.publish("/Renegade/Engineering/", String("Column selected: " + String(activeColumn)).c_str());
    piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);
    while (!piecePlaced) {
      handleAll();
      // The column is already full, ignore the input and keep the same player's turn
      activeColumn = (getPlayerInput(currentPlayer) / 8) - 1;
      client.publish("/Renegade/Engineering/", String("Column selected: " + String(activeColumn)).c_str());
      piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);
    }

    if (checkWin(gameBoard, currentPlayer)) {
      findWinningPositions(gameBoard, winningPositions);
      for (int i = 0; i < 4; i++) {
        winningRow = winningPositions[i][0];
        winningCol = winningPositions[i][1];
        winningSequence[i] = (winningCol + 1) * 8 + winningRow;
        lightButton(winningSequence[i], 96, 255, 255);
        FastLED.show();
        wait(100);
      }
      flashSequence(winningSequence);
      client.publish("/Renegade/Engineering/", "Player 1 Wins!");
      wait(1000);
      blackout();
      return;
    }

    currentPlayer = 2;
    play = play + 1;
    client.publish("/Renegade/Engineering/", "Player 2 turn");
    if (isSinglePlayerMode && currentPlayer == 2) {

      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }

      // Call the AI player's move function


      indicateTurn(currentPlayer);
      memcpy(minimaxBoard, gameBoard, sizeof(gameBoard));

      int randomNumber = random(101);  // Generate a random number between 0 and 100
      if (play == 1 || play == 2 || play == 3 ) { //Play best move
        activeColumn = findBestMove(minimaxBoard);
        client.publish("/Renegade/Engineering/", "Best move");
      }
      else if (randomNumber <= mistakeProbability || play == 5 ) {
        if (play <= 9) {
          activeColumn = (random(6) + 1); // Return a random number between 1 and 6
          client.publish("/Renegade/Engineering/", "Play Restricted Random");//Was String("Play Restricted Random =  " + String(activeColumn)).c_str());
        }
        else {
          activeColumn = random(8);  // Return a random number between 0 and 7
          client.publish("/Renegade/Engineering/", "Play Random");//Was String("Play Random =  " + String(activeColumn)).c_str());
        }
      }
      else {

        activeColumn = findBestMove(minimaxBoard);
        client.publish("/Renegade/Engineering/", "Best move");// Was String("Best move =  " + String(activeColumn)).c_str());
      }

      wait(1300 - (connect4Depth * 100));
      //piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);

    }

    else {
      input = getPlayerInput(currentPlayer);

      if (input == 90) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }

      activeColumn = (input / 8) - 1;
    }
    client.publish("/Renegade/Engineering/", String("Column selected: " + String(activeColumn)).c_str());
    piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);
    /*while (!piecePlaced) {// The column is already full, ignore the input and keep the same player's turn
      handleAll();
      activeColumn = (getPlayerInput(currentPlayer) / 8) - 1;

      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }

      client.publish("/Renegade/Engineering/", String("Column selected: " + String(activeColumn)).c_str());
      piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);
      }
    */
    while (!piecePlaced) {// The column is already full, ignore the input and keep the same player's turn
      handleAll();
      activeColumn = findBestMove(minimaxBoard);
      client.publish("/Renegade/Engineering/", "Best move");

      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }

      client.publish("/Renegade/Engineering/", "Inside While Loop");//String("Column selected: " + String(activeColumn)).c_str());
      piecePlaced = placePiece(gameBoard, currentPlayer, activeColumn);
    }

    if (checkWin(gameBoard, currentPlayer)) {
      findWinningPositions(gameBoard, winningPositions);
      for (int i = 0; i < 4; i++) {
        winningRow = winningPositions[i][0];
        winningCol = winningPositions[i][1];
        winningSequence[i] = (winningCol + 1) * 8 + winningRow;
        lightButton(winningSequence[i], 96, 255, 255);
        FastLED.show();
        wait(100);
      }
      flashSequence(winningSequence);
      client.publish("/Renegade/Engineering/", "Player 2 Wins!");
      wait(1000);
      blackout();
      return;
    }
  }
  client.publish("/Renegade/Engineering/", "Tie");
  blackout();
}

void indicateTurn(uint8_t currentPlayer) {
  if (currentPlayer == 1) {
    lightButton(0, 0, 255, beatsin8(45, 150, 255));
    lightButton(1, 0, 255, beatsin8(45, 150, 255));
    lightButton(72, 64, 255, 100);
    lightButton(73, 64, 255, 100);
    FastLED.show();
  } else {
    lightButton(0, 0, 255, 100);
    lightButton(1, 0, 255, 100);
    lightButton(72, 64, 255, beatsin8(45, 150, 255));
    lightButton(73, 64, 255, beatsin8(45, 150, 255));
    FastLED.show();
  }
}

uint8_t getPlayerInput(uint8_t currentPlayer) {
  uint8_t input = 0;
  while (input % 8 != 0 || input < 8 || input > 64) {
    indicateTurn(currentPlayer);
    handleAll();
    if (forcequit) {
      return 90;
    }
    input = buttonPressed();
  }
  client.publish("/Renegade/Engineering/", String("Input = " + String(input)).c_str());
  return input;
}

boolean placePiece(uint8_t gameBoard[][8], uint8_t currentPlayer, uint8_t activeColumn) {
  uint8_t hue;
  uint8_t buttonToLight;
  if (currentPlayer == 1) {
    hue = 0; //red if player 1
  } else {
    hue = 64; //yellow if player 2
  }
  for (int row = 0; row < 7; row++) {
    handleAll();
    if (gameBoard[0][activeColumn] != 0) {
      return false;
    } else if (gameBoard[row][activeColumn] != 0) {
      gameBoard[row - 1][activeColumn] = currentPlayer;
      //client.publish("/Renegade/Engineering/", String("Lighting row " + String(row - 1)).c_str());
      dropPiece(row - 1, activeColumn, hue);
      break;
    } else if (row == 6) {
      gameBoard[row][activeColumn] = currentPlayer;
      //client.publish("/Renegade/Engineering/", String("Lighting row " + String(row)).c_str());
      dropPiece(row, activeColumn, hue);
    }
  }
  return true;
}

bool checkWin(uint8_t gameBoard[7][8], uint8_t player) {
  const int numRows = 7;
  const int numColumns = 8;
  // Check horizontal
  for (int row = 0; row < numRows; row++) {
    for (int col = 0; col <= numColumns - 4; col++) {
      if (gameBoard[row][col] == player &&
          gameBoard[row][col + 1] == player &&
          gameBoard[row][col + 2] == player &&
          gameBoard[row][col + 3] == player) {
        return true; // Win condition found
      }
    }
  }

  // Check vertical
  for (int col = 0; col < numColumns; col++) {
    for (int row = 0; row <= numRows - 4; row++) {
      if (gameBoard[row][col] == player &&
          gameBoard[row + 1][col] == player &&
          gameBoard[row + 2][col] == player &&
          gameBoard[row + 3][col] == player) {
        return true; // Win condition found
      }
    }
  }

  // Check diagonal (top-left to bottom-right)
  for (int row = 0; row <= numRows - 4; row++) {
    for (int col = 0; col <= numColumns - 4; col++) {
      if (gameBoard[row][col] == player &&
          gameBoard[row + 1][col + 1] == player &&
          gameBoard[row + 2][col + 2] == player &&
          gameBoard[row + 3][col + 3] == player) {
        return true; // Win condition found
      }
    }
  }

  // Check diagonal (top-right to bottom-left)
  for (int row = 0; row <= numRows - 4; row++) {
    for (int col = numColumns - 1; col >= 3; col--) {
      if (gameBoard[row][col] == player &&
          gameBoard[row + 1][col - 1] == player &&
          gameBoard[row + 2][col - 2] == player &&
          gameBoard[row + 3][col - 3] == player) {
        return true; // Win condition found
      }
    }
  }

  return false; // No win condition found
}

int evaluateBoard(uint8_t board[ROWS][COLS]) {
  int score = 0;

  // Evaluate horizontally
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      int player1Count = 0;
      int player2Count = 0;

      for (int i = 0; i < 4; i++) {
        if (board[row][col + i] == 1) {
          player1Count++;
        } else if (board[row][col + i] == 2) {
          player2Count++;
        }
      }

      if (player1Count == 4) {
        score -= 100; // Player 1 (human) wins
      } else if (player2Count == 4) {
        score += 100; // Player 2 (Arduino) wins
      } else {
        score += player2Count - player1Count;
      }
    }
  }

  // Evaluate vertically
  for (int col = 0; col < COLS; col++) {
    for (int row = 0; row < ROWS - 3; row++) {
      int player1Count = 0;
      int player2Count = 0;

      for (int i = 0; i < 4; i++) {
        if (board[row + i][col] == 1) {
          player1Count++;
        } else if (board[row + i][col] == 2) {
          player2Count++;
        }
      }

      if (player1Count == 4) {
        score -= 100; // Player 1 (human) wins
      } else if (player2Count == 4) {
        score += 100; // Player 2 (Arduino) wins
      } else {
        score += player2Count - player1Count;
      }
    }
  }

  // Evaluate diagonally (positive slope)
  for (int row = 0; row < ROWS - 3; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      int player1Count = 0;
      int player2Count = 0;

      for (int i = 0; i < 4; i++) {
        if (board[row + i][col + i] == 1) {
          player1Count++;
        } else if (board[row + i][col + i] == 2) {
          player2Count++;
        }
      }

      if (player1Count == 4) {
        score -= 100; // Player 1 (human) wins
      } else if (player2Count == 4) {
        score += 100; // Player 2 (Arduino) wins
      } else {
        score += player2Count - player1Count;
      }
    }
  }

  // Evaluate diagonally (negative slope)
  for (int row = 3; row < ROWS; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      int player1Count = 0;
      int player2Count = 0;

      for (int i = 0; i < 4; i++) {
        if (board[row - i][col + i] == 1) {
          player1Count++;
        } else if (board[row - i][col + i] == 2) {
          player2Count++;
        }
      }

      if (player1Count == 4) {
        score -= 100; // Player 1 (human) wins
      } else if (player2Count == 4) {
        score += 100; // Player 2 (Arduino) wins
      } else {
        score += player2Count - player1Count;
      }
    }
  }

  return score;
}


int minimax(uint8_t board[ROWS][COLS], int depth, int player) {
  if (depth == 0) {
    // If the maximum depth is reached or game is over,
    // return the evaluation score of the board
    return evaluateBoard(board);
  }

  int bestScore;
  if (player == 2) {
    // Maximizing player (Arduino)
    bestScore = -1000; // Set to a low value
  } else {
    // Minimizing player (Human)
    bestScore = 1000; // Set to a high value
  }

  int validMoves[COLS]; // Array to store valid moves
  int numMoves = 0; // Number of valid moves

  // Iterate over each column to find valid moves
  for (int col = 0; col < COLS; col++) {
    // Check if the column is not full
    if (board[0][col] == 0) {
      validMoves[numMoves] = col;
      numMoves++;
    }
  }

  // Recursively evaluate each valid move
  for (int i = 0; i < numMoves; i++) {
    int col = validMoves[i];
    int row = 0;

    // Find the first empty row in the selected column
    while (row < ROWS - 1 && board[row + 1][col] == 0) {
      row++;
    }

    // Make the move
    board[row][col] = player;

    int score = minimax(board, depth - 1, 3 - player);

    // Undo the move
    board[row][col] = 0;

    // Update the best score
    if (player == 2) {
      if (score > bestScore) {
        bestScore = score;
      }
    } else {
      if (score < bestScore) {
        bestScore = score;
      }
    }
  }
  return bestScore;
}

int findBestMove(uint8_t board[ROWS][COLS]) {
  int validMoves[COLS]; // Array to store valid moves
  int numMoves = 0; // Number of valid moves

  // Iterate over each column to find valid moves
  for (int col = 0; col < COLS; col++) {
    // Check if the column is not full
    if (board[0][col] == 0) {
      validMoves[numMoves] = col;
      numMoves++;
    }
  }

  int bestMove = validMoves[0];
  int bestScore = -1000; // Set to a low value

  // Evaluate each valid move using the minimax algorithm
  for (int i = 0; i < numMoves; i++) {
    int col = validMoves[i];
    int row = 0;

    // Find the first empty row in the selected column
    while (row < ROWS - 1 && board[row + 1][col] == 0) {
      row++;
    }

    // Make the move
    board[row][col] = 2; // Arduino's move

    // Calculate the score for this move
    int score = minimax(board, connect4Depth, 1); // Assuming Arduino looks 3 moves ahead

    // Undo the move
    board[row][col] = 0;

    // Update the best score and move
    if (score > bestScore) {
      bestScore = score;
      bestMove = col;
    }
  }
  client.publish("/Renegade/Engineering/", String("AI chose column " + String(bestMove)).c_str());
  return bestMove;
}

void findWinningPositions(uint8_t board[ROWS][COLS], uint8_t winningPositions[4][2]) {
  // Check horizontal winning positions
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      if (board[row][col] != 0 && board[row][col] == board[row][col + 1] &&
          board[row][col] == board[row][col + 2] &&
          board[row][col] == board[row][col + 3]) {
        winningPositions[0][0] = row;
        winningPositions[0][1] = col;
        winningPositions[1][0] = row;
        winningPositions[1][1] = col + 1;
        winningPositions[2][0] = row;
        winningPositions[2][1] = col + 2;
        winningPositions[3][0] = row;
        winningPositions[3][1] = col + 3;
        return;
      }
    }
  }

  // Check vertical winning positions
  for (int col = 0; col < COLS; col++) {
    for (int row = 0; row < ROWS - 3; row++) {
      if (board[row][col] != 0 && board[row][col] == board[row + 1][col] &&
          board[row][col] == board[row + 2][col] &&
          board[row][col] == board[row + 3][col]) {
        winningPositions[0][0] = row;
        winningPositions[0][1] = col;
        winningPositions[1][0] = row + 1;
        winningPositions[1][1] = col;
        winningPositions[2][0] = row + 2;
        winningPositions[2][1] = col;
        winningPositions[3][0] = row + 3;
        winningPositions[3][1] = col;
        return;
      }
    }
  }

  // Check diagonal (positive slope) winning positions
  for (int row = 0; row < ROWS - 3; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      if (board[row][col] != 0 &&
          board[row][col] == board[row + 1][col + 1] &&
          board[row][col] == board[row + 2][col + 2] &&
          board[row][col] == board[row + 3][col + 3]) {
        winningPositions[0][0] = row;
        winningPositions[0][1] = col;
        winningPositions[1][0] = row + 1;
        winningPositions[1][1] = col + 1;
        winningPositions[2][0] = row + 2;
        winningPositions[2][1] = col + 2;
        winningPositions[3][0] = row + 3;
        winningPositions[3][1] = col + 3;
        return;
      }
    }
  }

  // Check diagonal (negative slope) winning positions
  for (int row = 3; row < ROWS; row++) {
    for (int col = 0; col < COLS - 3; col++) {
      if (board[row][col] != 0 &&
          board[row][col] == board[row - 1][col + 1] &&
          board[row][col] == board[row - 2][col + 2] &&
          board[row][col] == board[row - 3][col + 3]) {
        winningPositions[0][0] = row;
        winningPositions[0][1] = col;
        winningPositions[1][0] = row - 1;
        winningPositions[1][1] = col + 1;
        winningPositions[2][0] = row - 2;
        winningPositions[2][1] = col + 2;
        winningPositions[3][0] = row - 3;
        winningPositions[3][1] = col + 3;
        return;
      }
    }
  }
}

void lightsOut() {
  uint8_t gameBoard[SIZE][SIZE] = {};
  uint8_t rowFromButtonNumber;
  uint8_t colFromButtonNumber;
  uint8_t rowAdjust;
  uint8_t colAdjust;
  uint8_t input;
  lightsOutBorder();


  if (SIZE == 6) {
    rowAdjust = 1;
    colAdjust = 2;
  } else {
    rowAdjust = 2;
    colAdjust = 3;
  }


  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      gameBoard[i][j] = random(2); // Generate a random value of 0 or 1
    }
  }

  displayGameBoard(gameBoard);

  handleAll();
  while (!checkLightsOutWin(gameBoard)) {
    handleAll();
    client.publish("/Renegade/Engineering/", "waiting for input");
    input = 0;
    while (input < 17 || input > 62 || input % 8 == 0 || (input + 1) % 8 == 0) {
      //(input < 17 || input > 62 || input % 8 == 0 || (input + 1) % 8 == 0) for 6x6
      //(input < 26 || input > 53 || input % 8 == 0 || (input + 1) % 8 == 0|| (input + 2) % 8 == 0|| (input - 1) % 8 == 0) for 4x4
      handleAll();
      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }
      input = buttonPressed();
    }
    client.publish("/Renegade/Engineering/", String(input).c_str());

    rowFromButtonNumber = (input % 8) - rowAdjust; //convert button number to row number
    colFromButtonNumber = (input / 8) - colAdjust; //convert button number to column number

    gameBoard[rowFromButtonNumber][colFromButtonNumber] = !gameBoard[rowFromButtonNumber][colFromButtonNumber]; // Toggle the button itself
    if (rowFromButtonNumber > 0) {
      gameBoard[rowFromButtonNumber - 1][colFromButtonNumber] = !gameBoard[rowFromButtonNumber - 1][colFromButtonNumber]; // Toggle the button above
    }
    if (rowFromButtonNumber < SIZE - 1) {
      gameBoard[rowFromButtonNumber + 1][colFromButtonNumber] = !gameBoard[rowFromButtonNumber + 1][colFromButtonNumber]; // Toggle the button below
    }
    if (colFromButtonNumber > 0) {
      gameBoard[rowFromButtonNumber][colFromButtonNumber - 1] = !gameBoard[rowFromButtonNumber][colFromButtonNumber - 1]; // Toggle the button to the left
    }
    if (colFromButtonNumber < SIZE - 1) {
      gameBoard[rowFromButtonNumber][colFromButtonNumber + 1] = !gameBoard[rowFromButtonNumber][colFromButtonNumber + 1]; // Toggle the button to the right
    }

    displayGameBoard(gameBoard);
  }
  //win
  client.publish("/Renegade/Engineering/", "Lights out completed!");
  blackout();
}

void displayGameBoard(uint8_t gameBoard[][SIZE]) {
  uint8_t offsetValue;
  if (SIZE == 6) {
    offsetValue = 17;
  } else {
    offsetValue = 26;
  }
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      // Perform action based on the state of the element
      if (gameBoard[i][j] == 1) {
        lightButton(i + offsetValue + (8 * j), 128, 255, 255);
      } else {
        lightButton(i + offsetValue + (8 * j), 128, 255, 0);
      }
    }
  }
  FastLED.show();
}


bool checkLightsOutWin(uint8_t gameBoard[][SIZE]) {
  for (uint8_t i = 0; i < SIZE; i++) {
    for (uint8_t j = 0; j < SIZE; j++) {
      if (gameBoard[i][j] != 0) {
        return false; // If any button is not "off" (0), the game is not won yet
      }
    }
  }
  return true; // All buttons are "off" (0), the game is won
}

void lightsOutBorder() {
  uint8_t borderButtons6[44] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
  uint8_t borderButtons4[64] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 17, 18, 19, 20, 21, 22, 25, 33, 41, 49, 57, 58, 59, 60, 61, 62, 30, 38, 46, 54};
  if (SIZE == 6) {
    for (int i = 0; i < sizeof(borderButtons6); i++) {
      lightButton(borderButtons6[i], 32, 255, 255);
    }
  } else {
    for (int i = 0; i < sizeof(borderButtons4); i++) {
      lightButton(borderButtons4[i], 32, 255, 255);
    }
    FastLED.show();
  }
}

void twister(uint8_t roundCount, uint8_t playerCount) {
  boolean sequence[80] = {};
  const int maxRounds = 20;
  uint8_t hue = 128;
  uint8_t endButtonCount = 16;//This is total buttons to complete twister
  uint8_t startButtonCount = 3;//This is number of buttons Twister starts with
  uint8_t numberOfButtonsPerRoundByPlayer[7][maxRounds] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //ignore 1 player gets special array
    {2, 2, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6}, //2 players
    {2, 3, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}, //3 players
    {3, 4, 5, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, //4 players
    {3, 4, 6, 8, 10, 10, 10, 12, 13, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15}, //5 players
    {3, 4, 6, 8, 10, 12, 12, 12, 13, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16}, //6+ players
  };
  boolean win;
  boolean incorrectEntries[numBits];
  boolean lastIncorrectEntries[numBits];
  memset(incorrectEntries, 0, numBits * sizeof(bool));
  memset(lastIncorrectEntries, 0, numBits * sizeof(bool));

  blackout();
  client.publish("/Renegade/Engineering/", "Twister Started");
  twisterRoundBypass = false;

  for (int i = 0; i < roundCount; i++) {
    win = false;

    if (playerCount == 1) {
      //Single player has predefined pattern
      switch (i) {
        case 1:
          sequence[27] = 1;
          lightButton(27, hue, 255, 255);
          break;
        case 2:
          sequence[36] = 1;
          sequence[58] = 1;
          lightButton(36, hue, 255, 255);
          lightButton(58, hue, 255, 255);
          break;
        case 3:
          sequence[11] = 1;
          sequence[29] = 1;
          lightButton(11, hue, 255, 255);
          lightButton(29, hue, 255, 255);
          break;
        case 4:
          sequence[20] = 1;
          sequence[43] = 1;
          lightButton(20, hue, 255, 255);
          lightButton(43, hue, 255, 255);
          break;
        case 5:
          sequence[66] = 1;
          sequence[67] = 1;
          sequence[53] = 1;
          lightButton(66, hue, 255, 255);
          lightButton(67, hue, 255, 255);
          lightButton(53, hue, 255, 255);
          break;
        case 6:
          sequence[75] = 1;
          sequence[67] = 1;
          sequence[44] = 1;
          lightButton(75, hue, 255, 255);
          lightButton(67, hue, 255, 255);
          lightButton(44, hue, 255, 255);
          break;
          //yes I know this is a stupid way to write it but it works. sue me
      }
    } else {
      generateTwisterSequence(sequence, numberOfButtonsPerRoundByPlayer[playerCount][i]);

    }

    while (!win) {

      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }

      if (twisterRoundBypass) {
        twisterRoundBypass = false;
        break;
      }

      handleAll();
      PISOreadShiftRegister();
      FastLED.setBrightness(beatsin8(45, 80, 255));
      FastLED.show();
      win = compareBooleanArrays(sequence, currentButtonStates);

      for (int i = 0; i < numBits; i++) {
        //        //OR bitmask
        //        incorrectEntries[i] = sequence[i] || currentButtonStates[i];
        //
        //        //XOR bitmask
        //        incorrectEntries[i] = sequence[i] ^ incorrectEntries[i];
        //
        //        if (incorrectEntries[i] != lastIncorrectEntries[i]) {
        //          if (incorrectEntries[i]) {
        //            client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", String("Button " + String(i) + " is not part of the sequence!").c_str());
        //          } else  {
        //            client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", String("Button " + String(i) + " released").c_str());
        //          }
        //          lastIncorrectEntries[i] = incorrectEntries[i];
        //          break;
        //        }

        if (currentButtonStates[i] != previousButtonStates[i]) {
          if (currentButtonStates[i]) {
            client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", String("Pressed " + String(i)).c_str());
          } else  {
            client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", String("Released " + String(i)).c_str());
          }
          previousButtonStates[i] = currentButtonStates[i];
          break;
        }
      }

    }
    client.publish("/Renegade/Engineering/", "Twister round completed");
    blackout();
    for (int i = 0; i < numBits; i++) {
      sequence[i] = 0;
    }
  }
  client.publish("/Renegade/Engineering/", "Twister completed!");
  ringAnimation();
  blackout();  
}

void generateTwisterSequence(boolean sequence[], uint8_t numButtons, boolean randomColor) {
  uint8_t hue;
  uint8_t index;

  if (randomColor) {
    hue = random(256);
  } else {
    hue = 128;
  }
  if (firstRoundTwister){//Set the buttons you want to light on first round here
      sequence[35] = 1;
      sequence[52] = 1;
      lightButton(35, hue, 255, 255);
      lightButton(52, hue, 255, 255);
      String arrayString = "";
  for (int i = 0; i < numBits; i++) {
    handleAll();
    arrayString += String(sequence[i]);
  }
  client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", arrayString.c_str());
  //  client.publish("/Renegade/Engineering/", (String(indexSequence[0]) + " " + String(indexSequence[1]) + " " + String(indexSequence[2]) + " " + String(indexSequence[3]) + " ").c_str());
  FastLED.show();
    firstRoundTwister = false;
    return;
  }

  // Set all elements to 0
  //  for (int i = 0; i < numBits; i++) {
  //    sequence[i] = 0;
  //    indexSequence[i] = 0;
  //  }

  // Populate the specified number of elements with 1s
  for (uint8_t i = 0; i < numButtons; i++) {

    index = random(numBits);
    
    // Ensure the index is within the desired range
    while (sequence[index] == 1) {//Was   while (sequence[index] == 1 || index <= 6 || index >= 73)    for restricted twister
      index = random(numBits);
    }
    /*
    // If the element is already 1, find the next available index
    while (sequence[index] == 1) {
      index = random(numBits);
    }
    */
    sequence[index] = 1; // Set the element at the index to 1
    lightButton(index, hue, 255, 255);
  }

  String arrayString = "";
  for (int i = 0; i < numBits; i++) {
    handleAll();
    arrayString += String(sequence[i]);
  }
  client.publish("/Renegade/Engineering/ButtonWall/TwisterData/", arrayString.c_str());
  //  client.publish("/Renegade/Engineering/", (String(indexSequence[0]) + " " + String(indexSequence[1]) + " " + String(indexSequence[2]) + " " + String(indexSequence[3]) + " ").c_str());
  FastLED.show();
}

bool compareBooleanArrays(const bool array1[], const bool array2[]) {
  for (uint8_t i = 0; i < numBits; i++) {
    if (array1[i] != array2[i]) {
      //client.publish("/Renegade/Engineering/", "false");
      return false; // Arrays are not the same
    }
  }
  //client.publish("/Renegade/Engineering/", "true");
  return true; // Arrays are the same
}

bool compareUint8Arrays(const uint8_t array1[], const uint8_t array2[], uint8_t arraysize) {
  for (uint8_t i = 0; i < arraysize; i++) {
    if (array1[i] != array2[i]) {
      return false; // Arrays are not the same
    }
  }
  client.publish("/Renegade/Engineering/", "Arrays Match");
  return true; // Arrays are the same
}

void drawMode() {
  //uint8_t hueArray[numBits] = {0};
  //hue of 250 = off
  //0 = red, 50 = yellow, 100 = green, 150 = blue, 200 = purple
  uint8_t nextHue[numBits] = {0};
  uint8_t correctHue[numBits] = {0};
  uint8_t validInputs[36] = {8, 9, 10, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24, 25, 26, 29, 30, 31, 48, 49, 50, 53, 54, 55, 56, 57, 58, 61, 62, 63, 64, 65, 66, 69, 70, 71};
  uint8_t invalidInputs[44] = {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 19, 20, 27, 28, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 51, 52, 59, 60, 67, 68, 72, 73, 74, 75, 76, 77, 78, 79};
  memset(nextHue, 250, sizeof(nextHue));
  memset(correctHue, 250, sizeof(correctHue));
  compareUint8Arrays(nextHue, correctHue, numBits);

  uint8_t hue;
  uint8_t value;
  uint8_t input;
  boolean win = false;

  blackout();
  client.publish("/Renegade/Engineering/", "Starting Match Pattern");
  for (int i = 0; i < 44; i++) {
    lightButton(invalidInputs[i], 0, 0, 255);
  }
  FastLED.show();

  //0 = red, 50 = yellow, 100 = green, 150 = blue, 200 = purple
  //When hardcoding your patterns, the format should be as follows: correctHue[n] = h; where n is the button number, and h is the hue that it "should" be to count as correct.
  //For example:
  //  correctHue[8] = 50; //"Button Number 8 should be red"
  //  correctHue[16] = 100; //"Button Number 16 should be yellow"
  //Both must be true to complete puzzle.

  //Upper Left Corner
  correctHue[8] = 200;
  correctHue[9] = 150;
  correctHue[10] = 200;
  correctHue[16] = 100;
  correctHue[17] = 200;
  correctHue[18] = 100;
  correctHue[24] = 200;
  correctHue[25] = 150;
  correctHue[26] = 200;

  //Upper Right Corner
  correctHue[48] = 150;
  correctHue[49] = 50;
  correctHue[50] = 150;
  correctHue[56] = 100;
  correctHue[57] = 150;
  correctHue[58] = 50;
  correctHue[64] = 150;
  correctHue[65] = 100;
  correctHue[66] = 150;

  //Lower Left Corner
  correctHue[13] = 150;
  correctHue[14] = 0;
  correctHue[15] = 150;
  correctHue[21] = 50;
  correctHue[22] = 150;
  correctHue[23] = 0;
  correctHue[29] = 150;
  correctHue[30] = 50;
  correctHue[31] = 150;

  //Lower Right Corner
  correctHue[53] = 0;
  correctHue[54] = 200;
  correctHue[55] = 0;
  correctHue[61] = 200;
  correctHue[62] = 0;
  correctHue[63] = 200;
  correctHue[69] = 0;
  correctHue[70] = 200;
  correctHue[71] = 0;

  while (!win) {
    handleAll();
    if (forcequit) {
      client.publish("/Renegade/Engineering/", "Forcequitting...");
      forcequit = false;
      blackout();
      return;
    }

    input = buttonPressed();
    if (input != 80) {
      client.publish("/Renegade/Engineering/", String(input).c_str());
    }

    for (int i = 0; i < 36; i++) {
      handleAll();
      if (input == validInputs[i]) {
        nextHue[input] += 50;
        nextHue[input] = (nextHue[input] == 44) ? 0 : nextHue[input];

        if (nextHue[input] == 250) {
          value = 0;
        } else {
          value = 255;
        }
        lightButton(input, nextHue[input], 255, value);
        //        client.publish("/Renegade/Engineering/", String("Button pressed: " + String(input) + " Hue: " + String(nextHue[input])).c_str());
        //        client.publish("/Renegade/Engineering/", String("Button " + String(input) + " should have hue: " + String(correctHue[input])).c_str());
        FastLED.show();
      }
    }

    win = compareUint8Arrays(nextHue, correctHue, numBits);
  }

  client.publish("/Renegade/Engineering/", "Completed Match Pattern");
  ringAnimation();
  wait(1000);
  blackout();
}

void memoryGame() {
  client.publish("/Renegade/Engineering/", "Starting Memory Game");
  unsigned long previousTime = 0;
  unsigned long currentTime;
  byte gameBoardLH[4][4] = {
    { 2, 1, 6, 4},
    { 3, 5, 2, 7},
    { 1, 0, 6, 4},
    { 3, 0, 5, 7}
  };
  byte gameBoardRH[4][4] = {
    {13, 12, 15, 10},
    {11, 8, 9, 14 },
    {10, 14, 8, 13 },
    {15, 9, 12, 11 }
  };
  byte usedInputsLH[16] =  {}; //usedInputs stores already solved buttons to reject their inputs.
  byte usedInputsRH[16] =  {};
  byte hueArray[16][4] = {     //hueArray stores the patterns for each pair of buttons. For example, on the left side game board, the upper left button is "2"
    { 64, 80, 208, 176 },      //which means it will use row 2 of hueArray: { 224, 128, 192, 64 }. Each value is the hue of the individual 4 LEDs on a button, starting with the upper left and moving clockwise.
    { 128, 176, 176, 128 },
    { 224, 128, 192, 64 },
    { 176, 176, 80, 224 },
    { 64, 176, 128, 64 },
    { 144, 224, 192, 144 },
    { 208, 48, 128, 224 },
    { 224, 64, 64, 176 },
    { 224, 128, 144, 64 },
    { 176, 96, 176, 176 },
    { 192, 176, 64, 192 },
    { 176, 48, 224, 224 },
    { 128, 240, 208, 48},
    { 96, 224, 64, 160},
    { 80, 208, 128, 32},
    { 176, 32, 112, 224}
  };
  int boardButtons[32] = {2, 3, 4, 5, 10, 11, 12, 13, 18, 19, 20, 21, 26, 27, 28, 29, 50, 51, 52, 53, 58, 59, 60, 61, 66, 67, 68, 69, 74, 75, 76, 77};

  byte cardLH[2] = {};
  byte buttonLH[2] = {};
  byte lightWhiteLH[2] = {};
  byte lightOffLH[2] = {};
  boolean disableLH = false;

  byte cardRH[2] = {};
  byte buttonRH[2] = {};
  byte lightWhiteRH[2] = {};
  byte lightOffRH[2] = {};
  boolean disableRH = false;

  byte input;
  byte lastInput;
  byte LHwinCounter = 0;
  byte LHcounter = 0;
  byte RHwinCounter = 0;
  byte RHcounter = 0;
  boolean win = false;

  //Light Border
  fill_solid(leds, NUM_LEDS, CHSV(128, 255, 200));
  for (int i = 0; i < 32; i++) {
    lightButton(boardButtons[i], 0, 0, 200);
  }
  FastLED.show();

  while (!win) {
    input = 0;
    handleAll();

    //Reject non-active buttons for first card flip
    while ((input > 29 && input < 50) || input % 8 == 0 || input % 8 == 1 || input % 8 == 6 || input % 8 == 7) {
      currentTime = millis();
      handleAll();
      input = buttonPressed();
      if (forcequit) {
        client.publish("/Renegade/Engineering/", "Forcequitting...");
        forcequit = false;
        blackout();
        return;
      }
      for (int i = 0; i < 16; i++) {
        if (input == usedInputsLH[i] || input == usedInputsRH[i] || lastInput == input || (input <= 29 && disableLH) || (input >= 50 && disableRH)) {
          input = 0;
        }
      }
      if (currentTime - previousTime >= 2000) {
        if (lightWhiteLH[0] + lightWhiteLH[1] != 0) {
          lightButton(lightWhiteLH[0] , 96, 0, 255);
          lightButton(lightWhiteLH[1] , 96, 0, 255);
          memset(lightWhiteLH, 0, sizeof(lightWhiteLH));
          FastLED.show();
        } else if (lightOffLH[0] + lightOffLH[1] != 0) {
          lightButton(lightOffLH[0] , 96, 255, 0);
          lightButton(lightOffLH[1] , 96, 255, 0);
          FastLED.show();
          memset(lightOffLH, 0, sizeof(lightOffLH));
        }
        if (lightWhiteRH[0] + lightWhiteRH[1] != 0) {
          lightButton(lightWhiteRH[0] , 96, 0, 255);
          lightButton(lightWhiteRH[1] , 96, 0, 255);
          memset(lightWhiteRH, 0, sizeof(lightWhiteRH));
          FastLED.show();
        } else if (lightOffRH[0] + lightOffRH[1] != 0) {
          lightButton(lightOffRH[0] , 96, 255, 0);
          lightButton(lightOffRH[1] , 96, 255, 0);
          FastLED.show();
          memset(lightOffRH, 0, sizeof(lightOffRH));
        }
        previousTime = currentTime;
        disableLH = false;
        disableRH = false;
      }
    }
    if (!disableLH || !disableRH) {
      previousTime = currentTime;
    }


    //Sort LH or RH
    if (input <= 29) {
      cardLH[LHcounter] = gameBoardLH[(input % 8) - 2][(input / 8)];
      buttonLH[LHcounter] = input;
      lightButtonMulti(buttonLH[LHcounter], hueArray[cardLH[LHcounter]]);
      FastLED.show();
      client.publish("/Renegade/Engineering/", String("Hidden Value (Left Side): " + String(cardLH[LHcounter])).c_str());
      lastInput = input;
      LHcounter++;
    } else if (input >= 50) {
      cardRH[RHcounter] = gameBoardRH[(input % 8) - 2][(input / 8 - 6)];
      buttonRH[RHcounter] = input;
      client.publish("/Renegade/Engineering/", String(input).c_str());
      lightButtonMulti(buttonRH[RHcounter], hueArray[cardRH[RHcounter]]);
      FastLED.show();
      client.publish("/Renegade/Engineering/", String("Hidden Value (Right Side): " + String(cardRH[RHcounter])).c_str());
      lastInput = input;
      RHcounter++;
    }

    //Evaluate LH
    if (LHcounter == 2 ) {
      lastInput = 0;
      LHcounter = 0;
      if (cardLH[0] != cardLH[1]) {
        client.publish("/Renegade/Engineering/", "No Match");
        memcpy(lightWhiteLH, buttonLH, sizeof(buttonLH));
        disableLH = true;
      } else {
        client.publish("/Renegade/Engineering/", "Match!");
        memcpy(lightOffLH, buttonLH, sizeof(buttonLH));
        disableLH = true;
        wait(250);
        lightButton(buttonLH[0] , 96, 255, 255);
        lightButton(buttonLH[1] , 96, 255, 255);
        FastLED.show();
        usedInputsLH[LHwinCounter] = buttonLH[0];
        usedInputsLH[16 - LHwinCounter] = buttonLH[1];
        LHwinCounter++;
      }
    }

    //Evaluate RH
    if (RHcounter == 2 ) {
      lastInput = 0;
      RHcounter = 0;
      if (cardRH[0] != cardRH[1]) {
        client.publish("/Renegade/Engineering/", "No Match");
        memcpy(lightWhiteRH, buttonRH, sizeof(buttonRH));
        disableRH = true;
      } else {
        client.publish("/Renegade/Engineering/", "Match!");
        memcpy(lightOffRH, buttonRH, sizeof(buttonRH));
        disableRH = true;
        wait(250);
        lightButton(buttonRH[0] , 96, 255, 255);
        lightButton(buttonRH[1] , 96, 255, 255);
        FastLED.show();
        usedInputsRH[RHwinCounter] = buttonRH[0];
        usedInputsRH[16 - RHwinCounter] = buttonRH[1];
        RHwinCounter++;
      }
    }

    //Check Partial Win
    if (LHwinCounter == 8) {
      client.publish("/Renegade/Engineering/", "Left side completed");
    }
    if (RHwinCounter == 8) {
      client.publish("/Renegade/Engineering/", "Right side completed");
    }

    //Check for total win
    if (RHwinCounter + LHwinCounter == 16) {
      win = true;
    }
  }
  client.publish("/Renegade/Engineering/", "Completed Memory Game");
  ringAnimation();
  blackout();
  return;
}
void defaultMode() {//This is new version that splits shift registers to Groups of 4 and 6 on different data pins
  boolean incoming = 0;

  // Step 1: Sample
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Arrays to store the two groups of bits
  boolean firstGroup[16];
  boolean secondGroup[64];

  // Step 2: Shift and read both groups in a single loop
  for (int i = 0; i < 64; i++) {
    handleAll();

    // Read from both data pins for the first 32 bits; only read second group after
    if (i < 16) {
      firstGroup[i] = digitalRead(dataPin1); // Data pin for the first 32-bit group
      delayMicroseconds(2);  // Small delay between reads to stabilize signals
    }
    secondGroup[i] = digitalRead(dataPin2);  // Data pin for the second 48-bit group

    // Shift the next bit
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
  }

  // Combine the two groups into the currentButtonStates array
  for (int i = 0; i < 80; i++) {
    currentButtonStates[i] = (i < 16) ? firstGroup[i] : secondGroup[i - 16];
  }

  // Process button states and update LED lighting immediately based on button state change
  for (int i = 0; i < 80; i++) {
    if (currentButtonStates[i] != previousButtonStates[i]) {
      if (currentButtonStates[i] != 0) {
        lightButton(i, 0, 255, 255);
        String buttonNumber = String("Button " + String(i) + " pressed");
        client.publish("/Renegade/Engineering/", buttonNumber.c_str());
      } else {
        lightButton(i, 0, 0, 0);
      }
      FastLED.show();
      // Update previous button state
      previousButtonStates[i] = currentButtonStates[i];
    }
  }
}

/*
void defaultMode() {
  boolean incoming = 0;
  // Step 1: Sample
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Step 2: Shift
  for (int i = 0; i < numBits; i++)
  {
    handleAll();
    incoming = digitalRead(dataPin);
    currentButtonStates[i] = incoming;

    // Update LED lighting immediately based on button state change
    if (currentButtonStates[i] != previousButtonStates[i]) {
      if (currentButtonStates[i] != 0) {
        lightButton(i, 0, 255, 255);
        String buttonNumber = String("Button " + String(i) + " pressed");
        client.publish("/Renegade/Engineering/", buttonNumber.c_str());
      } else {
        lightButton(i, 0, 0, 0);
      }
      FastLED.show();
      // Update previous button state
      previousButtonStates[i] = currentButtonStates[i];
    }
    digitalWrite(clockPin, HIGH); // Shift out the next bit
    digitalWrite(clockPin, LOW);

  }

}
*/

//Utility functions

uint8_t buttonPressed() {//This is new version that splits shift registers to Groups of 4 and 6 on different data pins
  boolean incoming = 0;
  // Step 1: Sample (common load pin)
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Arrays to store the two groups of bits
  boolean firstGroup[16];
  boolean secondGroup[64];

  // Step 2: Shift and read both groups in a single loop
  for (int i = 0; i < 64; i++) {
    handleAll();

    // Read from both data pins for the first 32 bits; only read second group after
    if (i < 16) {
      firstGroup[i] = digitalRead(dataPin1); // Data pin for the first 32-bit group
      delayMicroseconds(2);  // Small delay between reads to stabilize signals
    }
    secondGroup[i] = digitalRead(dataPin2);  // Data pin for the second 48-bit group

    // Shift the next bit
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
  }

  // Combine the two groups into the currentButtonStates array
  for (int i = 0; i < 80; i++) {
    currentButtonStates[i] = (i < 16) ? firstGroup[i] : secondGroup[i - 16];
  }

  // Process button states and publish changes
  for (int i = 0; i < 80; i++) {
    if (currentButtonStates[i] != previousButtonStates[i]) {
      if (currentButtonStates[i] != 0) {
        String buttonNumber = String("Button " + String(i) + " pressed");
        client.publish("/Renegade/Engineering/", buttonNumber.c_str());
        previousButtonStates[i] = currentButtonStates[i];
        return i;
      } else {
        previousButtonStates[i] = currentButtonStates[i];
      }
    }
  }

  // Optional: Print to NodeRED
  String arrayString = "";
  for (int i = 0; i < 80; i++) {
    handleAll();
    arrayString += String(currentButtonStates[i]);
  }
  client.publish("/Renegade/Engineering/", arrayString.c_str());

  return 80;
}

/*
uint8_t buttonPressed() {
  boolean incoming = 0;
  // Step 1: Sample
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Step 2: Shift
  for (int i = 0; i < numBits; i++) {
    handleAll();
    incoming = digitalRead(dataPin);
    currentButtonStates[i] = incoming;

    if (currentButtonStates[i] != previousButtonStates[i]) {
      if (currentButtonStates[i] != 0) {
        String buttonNumber = String("Button " + String(i) + " pressed");
        client.publish("/Renegade/Engineering/", buttonNumber.c_str());
        previousButtonStates[i] = currentButtonStates[i];
        return i;
      } else {
        previousButtonStates[i] = currentButtonStates[i];
      }
    }
    digitalWrite(clockPin, HIGH); // Shift out the next bit
    digitalWrite(clockPin, LOW);
  }
   /*
  // Step 5: Print to NodeRED
  String arrayString = "";
  for (int i = 0; i < numBits; i++) {
    handleAll();
    arrayString += String(currentButtonStates[i]);
  }
  client.publish("/Renegade/Engineering/", arrayString.c_str());
  */
  /*
  return 80;
}
*/
void PISOreadShiftRegister()//This is new version that splits shift registers to Groups of 4 and 6 on different data pins
{
  // Step 1: Sample (common load pin)
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Arrays to store the two groups of bits
  boolean firstGroup[16];
  boolean secondGroup[64];

  // Step 2: Shift and read both groups in a single loop
  for (int i = 0; i < 64; i++) {
    handleAll();

    // Read from both data pins for the first 32 bits; only read second group after
    if (i < 16) {
      firstGroup[i] = digitalRead(dataPin1); // Data pin for the first 32-bit group
      delayMicroseconds(2);  // Small delay between reads to stabilize signals
    }
    secondGroup[i] = digitalRead(dataPin2);  // Data pin for the second 48-bit group

    // Shift the next bit
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
  }

  // Combine the two groups into the currentButtonStates array
  for (int i = 0; i < 80; i++) {
    currentButtonStates[i] = (i < 16) ? firstGroup[i] : secondGroup[i - 16];
  }

  // Step 5: Print or process as before
  String arrayString = "";
  for (int i = 0; i < numBits; i++) {
    handleAll();
    arrayString += String(currentButtonStates[i]);
  }

  // client.publish("/Renegade/Engineering/", arrayString.c_str());
}

/*
void PISOreadShiftRegister()
{
  // Step 1: Sample
  digitalWrite(loadPin, LOW);
  digitalWrite(loadPin, HIGH);

  // Step 2: Shift
  //Serial.print("Bits: ");
  for (int i = 0; i < numBits; i++)
  {
    handleAll();
    boolean incoming = digitalRead(dataPin);
    currentButtonStates[i] = incoming;
    //Serial.print(digitalRead(dataPin));
    digitalWrite(clockPin, HIGH); // Shift out the next bit
    delayMicroseconds(5);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
  }


  // Step 5: Print to NodeRED
  String arrayString = "";
  for (int i = 0; i < numBits; i++) {
    handleAll();
    arrayString += String(currentButtonStates[i]);
  }
  // client.publish("/Renegade/Engineering/", arrayString.c_str());
  //    Serial.println();
  //   wait(250);
}
*/
//Button Mapping and Animations
void lightButton(int buttonNumber, int H, int S, int V)
{
  for (int i = 0; i < 4; i++)
  {
    leds[i + (4 * buttonNumber)] = CHSV(H, S, V);
  }
}

void lightButtonMulti(int buttonNumber, byte hueArray[]) {
  for (int i = 0; i < 4; i++)
  {
    leds[i + (4 * buttonNumber)] = CHSV(hueArray[i], 255, 200);
  }
}

void lightButtonSingle(int buttonNumber, int ledPosition, int hue) {
  leds[ledPosition + (4 * buttonNumber)] = CHSV(hue, 255, 255);
}

void lightColumn(int columnNumber, int H, int S, int V) {
  for (int i = 0; i < 8; i++) {
    lightButton(i + (8 * columnNumber), H, S, V);
  }
}

void lightRow(int rowNumber, int H, int S, int V) {
  for (int i = 0; i < 10; i++) {
    lightButton((i * 8) + rowNumber, H, S, V);
  }
}

void dropPiece(uint8_t row, uint8_t col, uint8_t hue) {
  uint8_t currentButton;
  currentButton = (col + 1) * 8;
  lightButton(currentButton, hue, 255, 255);
  FastLED.show();
  wait(100);
  for (int i = 1; i <= row; i++) {
    currentButton = (col + 1) * 8 + i;
    lightButton(currentButton - 1, hue, 255, 0);
    FastLED.show();
    wait(100);
    lightButton(currentButton, hue, 255, 255);
    FastLED.show();
    wait(100);
  }
}

void sequentialButtonLight() {

  for (int i = 0; i < numBits; i++)
  {
    handleAll();
    fadeToBlackBy(leds, NUM_LEDS, fadeBy);  // 64/255 = 25%
    //    lightButton(i, beatsin8(1, 0, 255), 255, 255);
    lightButton(i, globalHue, 255, 255);
    FastLED.show();
  }

  //  for (int i = 0; i < numBits; i++)
  //  {
  //    handleAll();
  //    FastLED.delay(50);
  //    lightButton(i, 0, 0, 0);
  //    FastLED.show();
  //  }
}

void flashSequence(uint8_t winningSequence[4]) {
  //Connect 4 handler animation
  for (int i = 0; i < 5; i++) {
    for (int i = 0; i < 4; i++) {
      lightButton(winningSequence[i], 96, 255, 0);
    }
    FastLED.show();

    wait(500);
    for (int i = 0; i < 4; i++) {
      lightButton(winningSequence[i], 96, 255, 255);
    }
    FastLED.show();
    wait(500);
  }
}

void ringAnimation() {
  uint32_t startTime = millis();
  while (millis() - startTime < 4000) {
    byte wave = beatsin8(90, 0, 255, startTime);
    //do for 2 seconds
    handleAll();
    fill_solid(leds, NUM_LEDS, CHSV(96, 255, wave));
    FastLED.show();
  }
  //while (millis() - startTime < 6000) {
    handleAll();
    blackout();//Was fadeToBlackBy(leds, NUM_LEDS, 10);
    FastLED.show();
  //}
}


void lightRing(uint8_t ring[], uint8_t sizeofring, uint8_t hue, uint8_t saturation, uint8_t value) {
  //handler for ring animation
  for (int i = 0; i < sizeofring; i++) {
    lightButton(ring[i], hue, saturation, value);
  }
}

void idle() {
  boolean sequence[numBits] = {};
  for (int i = 0; i < 4; i++) {
    generateTwisterSequence(sequence, random(20, 50), true);
    wait(1000);
    blackout();
  }
  wait(5000);
}

void blackout() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
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
