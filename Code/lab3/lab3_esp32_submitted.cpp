/*

Components:
- freenove esp32 wrover
- 74hc595 shift register
- 7-segment display (4 digits)
- 7 resistors (220 ohm)
- 3 resistors (220 ohm)
- Keypad 4x4
- Piezo buzzer

*/

#include <Keypad.h>

// ----- 74hc595 -----
#define HC_DATA 19
#define HC_LATCH 18
#define HC_CLOCK 5

// ----- seven segment display 4 digits -----
#define SSD_D1 25 
#define SSD_D2 26
#define SSD_D3 27
#define SSD_D4 14

// ----- RGB LED -----
#define LED_R 35
#define LED_G 32
#define LED_B 33

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

byte rowPins[ROWS] = {23, 22, 21, 4}; 
byte colPins[COLS] = {0, 2, 15, 12}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ----- Buzzer -----
#define BUZZER 5


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


// --- -- task functions --- --
int number = 0;
const int min_number = 1;
const int max_number = 10;
int guessedNumber = 0;

const int winMelodySize = 6;
int winMelodyFrequencies[winMelodySize] = {
  523, 659, 784, 1047, 784, 1047
};
int winMelodyDurations[winMelodySize] = {
  120, 120, 120, 250, 120, 400
};


unsigned long lastRefreshSSD4d = 0;
bool isTrigerShowGuessedNumber = false;

void task_SSD4d() {
  if (isTrigerShowGuessedNumber) {
    displayNumber(number);
    
    if (lastRefreshSSD4d == 0) lastRefreshSSD4d = millis();

    if (millis() - lastRefreshSSD4d > 1000) {
      isTrigerShowGuessedNumber = false;
      lastRefreshSSD4d = 0;
    }
  }
  else {
    displayNumber(number);
  }
}

void task_Keypad() {
  char key = keypad.getKey();

  if (key) {
    if (key == 'A') {
      tryGuessNumber();
    }
    else if (key == 'B') {
      // Show guessed number
      isTrigerShowGuessedNumber = true;
    }
    else if (key == 'C') {
      // Clear number
      number = 0;
    }
    else if (key == 'D') {
      // Delete digit
      number = number / 10;
    }
    else if (key == '#') {
      generateGuessedNumber();
    }
    else if (key == '*') {
      // Set random number (!= guessedNumber)
      do {
        number = randNumber(min_number, max_number);
      } while (number == guessedNumber);
    }
    else if (key >= '0' && key <= '9') {
      // Add digit
      number = number * 10 + (key - '0');
      if (number > 9999) number = key - '0'; // обмеження 4-digit
    }
  }
}

bool isTrigPlayWinMelody = false;
int melodyIndex = 0;
unsigned long lastRefreshMelody = 0;

void task_PlayWinMelody() {
  if (!isTrigPlayWinMelody) return;

  if (melodyIndex >= winMelodySize) {
    melodyIndex = 0;
    isTrigPlayWinMelody = false;
    noTone(BUZZER);
    return;
  }

  if (melodyIndex == 0 && lastRefreshMelody == 0) {
    tone(BUZZER, winMelodyFrequencies[0]);
    lastRefreshMelody = millis();
  }

  if (millis() - lastRefreshMelody > winMelodyDurations[melodyIndex]) {
    noTone(BUZZER);
    tone(BUZZER, winMelodyFrequencies[melodyIndex]);

    melodyIndex++;

    lastRefreshMelody = millis();
  }
}

// --- -- my functions --- --
void setRGB(byte colorIndex) {
  switch (colorIndex) {
      case 0:
        // none
        digitalWrite(LED_R, RGB_OFF_SIGNAL);
        digitalWrite(LED_G, RGB_OFF_SIGNAL);
        digitalWrite(LED_B, RGB_OFF_SIGNAL);
        break;

      case 1:
        // red
        digitalWrite(LED_R, RGB_ON_SIGNAL);
        digitalWrite(LED_G, RGB_OFF_SIGNAL);
        digitalWrite(LED_B, RGB_OFF_SIGNAL);
        break; 

      case 2:
        // green
        digitalWrite(LED_R, RGB_OFF_SIGNAL);
        digitalWrite(LED_G, RGB_ON_SIGNAL);
        digitalWrite(LED_B, RGB_OFF_SIGNAL);
        break;

      case 3:
        // blue
        digitalWrite(LED_R, RGB_OFF_SIGNAL);
        digitalWrite(LED_G, RGB_OFF_SIGNAL);
        digitalWrite(LED_B, RGB_ON_SIGNAL);
        break;
    }
}

int randNumber(int minV, int maxV) {
  return random(minV, maxV + 1);
}

void generateGuessedNumber() {
  guessedNumber = randNumber(min_number, max_number);
}

void tryGuessNumber() {
  if (number == guessedNumber) {
    setRGB(2);
    isTrigPlayWinMelody = true;
  } else {
    if (number > guessedNumber) {
      setRGB(1);
    } else {
      setRGB(3);
    }
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

  pinMode(BUZZER, OUTPUT);

  generateGuessedNumber();
  setRGB(0);
}

void loop() {
  task_SSD4d();

  task_Keypad();

  task_PlayWinMelody();

  delay(10);
}
 