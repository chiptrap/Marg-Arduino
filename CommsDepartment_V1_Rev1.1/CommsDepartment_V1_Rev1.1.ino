
// 165 Shift Register parameters
const int data165 = D2;  /* Q7 */
const int clock165 = D3; /* CP */
const int latch165 = D4;  /* PL */
const int numBits = 8;  /* Set to 8 * number of shift registers */
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
}

void loop()
{
  PISOreadShiftRegister();
//  int convertedValue = binaryArrayToInt(buttonStates, arraySize);
//  SIPOregisterOutput(convertedValue);
}

void PISOreadShiftRegister()
{
  // Step 1: Sample
  digitalWrite(latch165, LOW);
  delayMicroseconds(0.1);
  digitalWrite(latch165, HIGH);

  // Step 2: Shift
  Serial.print("Bits: ");
  for (int i = 0; i < numBits; i++)
  {
    int bit = digitalRead(data165);
    buttonStates[i] = bit;
    Serial.print(buttonStates[i]);
    digitalWrite(clock165, HIGH); // Shift out the next bit
    delayMicroseconds(0.1);
    digitalWrite(clock165, LOW);
  }

  Serial.println();
}

int binaryArrayToInt(int binaryArray[], int size) {
  int result = 0;

  for (int i = 0; i < size; i++) {
    result = (result << 1) | binaryArray[i];
  }
  Serial.println(result);
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
