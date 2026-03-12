/*

Components:
- freenove esp32 wrover
- 74hc595 shift register
- 7-segment display (4 digits)
- 7 resistors (220 ohm)
- rgb led
- 3 resistors (220 ohm)
- Keypad 4x4
- Piezo buzzer

*/

#include <Keypad.h>

// ----- 74hc595 -----
#define HC_DATA 16
#define HC_LATCH 17
#define HC_CLOCK 18

// ----- seven segment display 4 digits -----
#define SSD_D1 19 
#define SSD_D2 21
#define SSD_D3 22
#define SSD_D4 23

// ----- RGB LED -----
#define LED_R 25
#define LED_G 26
#define LED_B 27

#define RGB_OFF_SIGNAL HIGH
#define RGB_ON_SIGNAL LOW

// ----- Keypad 4x4 -----
#define ROWS 4 
#define COLS 4

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {33, 32, 2, 0}; // підключені до рядів
byte colPins[COLS] = {14, 12, 13, 15}; // підключені до колонок

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ----- Potentiometer -----
#define POTENTIOMETER 34

// ----- Buzzer -----
#define BUZZER 5

// ----- Button -----
#define TONE_BUTTON 4


// ----- params -----
byte digits[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

const bool invertBitsForSSD = true;


// --- -- 74hc595 & SSD4d functions -- ---
void shiftOut595(byte data)
{
  digitalWrite(HC_LATCH, LOW);

  shiftOut(HC_DATA, HC_CLOCK, MSBFIRST, data);

  digitalWrite(HC_LATCH, HIGH);
}

void showDigit(int digitPin, byte number)
{
  digitalWrite(SSD_D1, LOW);
  digitalWrite(SSD_D2, LOW);
  digitalWrite(SSD_D3, LOW);
  digitalWrite(SSD_D4, LOW);

  shiftOut595(invertBitsForSSD ? ~digits[number] : digits[number]);

  digitalWrite(digitPin, HIGH);

  delay(2);
}

void displayNumber(int num)
{
  int d1 = num / 1000;
  int d2 = (num / 100) % 10;
  int d3 = (num / 10) % 10;
  int d4 = num % 10;


  if (d1 != 0) showDigit(SSD_D1, d1);
  if (d2 != 0 || d1 != 0) showDigit(SSD_D2, d2);
  if (d3 != 0 || d2 != 0 || d1 != 0) showDigit(SSD_D3, d3);
  showDigit(SSD_D4, d4);
}


// ----- Button interrupt -----
unsigned long lastInterruptButton = 0;
bool isTone = false;

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long now = millis();
  if(now - lastInterruptButton > 200){
    isTone = !isTone;

    lastInterruptButton = now;
  }
}


// --- -- my functions --- --
int number = 1234;
unsigned long lastRefreshSSD4d = 0;

void testSSD4d() {
  if (millis() - lastRefreshSSD4d > 2) {
    displayNumber(number);
    lastRefreshSSD4d = millis();
  }
}

byte colorIndex = 0;
unsigned long lastRefreshRGBLED = 0;

void testRGBLED() {
  if (millis() - lastRefreshRGBLED > 1000) {
    switch (colorIndex) {
      case 0:
        // red
        digitalWrite(LED_R, RGB_ON_SIGNAL);
        digitalWrite(LED_G, RGB_OFF_SIGNAL);
        digitalWrite(LED_B, RGB_OFF_SIGNAL);
        break; 

      case 1:
        // green
        digitalWrite(LED_R, RGB_OFF_SIGNAL);
        digitalWrite(LED_G, RGB_ON_SIGNAL);
        digitalWrite(LED_B, RGB_OFF_SIGNAL);
        break;

      case 2:
        // blue
        digitalWrite(LED_R, RGB_OFF_SIGNAL);
        digitalWrite(LED_G, RGB_OFF_SIGNAL);
        digitalWrite(LED_B, RGB_ON_SIGNAL);
        break;
    }

    colorIndex++;
    if (colorIndex > 2) colorIndex = 0;

    lastRefreshRGBLED = millis();
  }
}

char keypadValue = '?';

void testKeypad() {
  char key = keypad.getKey();

  if (key) {
    if (key >= '0' && key <= '9') {
      number = number * 10 + (key - '0');
      if (number > 9999) number = key - '0'; // обмеження 4-digit
    }
  }
}

void testPotentiometer() {
  int adc = analogRead(POTENTIOMETER);
  
  // SSD4d
  number = adc;

  // RGB led
  int r = map(adc, 0, 4095, 0, 255);
  int g = map(adc, 0, 4095, 255, 0);
  int b = map(adc, 0, 4095, 0, 128);

  analogWrite(LED_R, 255 - r);
  analogWrite(LED_G, 255 - g);
  analogWrite(LED_B, 255 - b);

  // buzzer
  if (isTone) {
    int freq = map(adc, 0, 4095, 200, 2000);
    tone(BUZZER, freq);
  }
  else {
    noTone(BUZZER);
  }
}


// --- -- core functions -- ---
void setup() {

  pinMode(HC_DATA, OUTPUT);
  pinMode(HC_CLOCK, OUTPUT);
  pinMode(HC_LATCH, OUTPUT);

  pinMode(SSD_D1, OUTPUT);
  pinMode(SSD_D2, OUTPUT);
  pinMode(SSD_D3, OUTPUT);
  pinMode(SSD_D4, OUTPUT);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  pinMode(POTENTIOMETER, INPUT);

  pinMode(BUZZER, OUTPUT);
  pinMode(TONE_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TONE_BUTTON), handleButtonInterrupt, FALLING);
}

void loop() {
  testSSD4d();

  //testRGBLED();  

  //testKeypad();

  //testPotentiometer();

  delay(10);
}
