/*

Components:
- freenove esp32 wrover
- 3 LEDs (red, yellow, green)
- 3 resistors (220 ohm)
- 7-segment display (4 digits)
- 7 resistors (220 ohm)

*/

// ----- LED pins -----
const int greenPin  = 15;
const int yellowPin = 4;
const int redPin    = 19;

// ----- Segment pins (a, b, c, d, e, f, g) -----
const int segA = 23;
const int segB = 18;
const int segC = 14;
const int segD = 2;
const int segE = 27;
const int segF = 5;
const int segG = 22;

const int segPins[7] = {segA, segB, segC, segD, segE, segF, segG};

// ----- Digit select pins -----
const int digit1 = 32;
const int digit2 = 33;
const int digit3 = 25;
const int digit4 = 26;

const int digitPins[4] = {digit1, digit2, digit3, digit4};

// ----- Digits 0-9 -----
const bool digits[10][7] = {
  {1,1,1,1,1,1,0}, // 0
  {0,1,1,0,0,0,0}, // 1
  {1,1,0,1,1,0,1}, // 2
  {1,1,1,1,0,0,1}, // 3
  {0,1,1,0,0,1,1}, // 4
  {1,0,1,1,0,1,1}, // 5
  {1,0,1,1,1,1,1}, // 6
  {1,1,1,0,0,0,0}, // 7
  {1,1,1,1,1,1,1}, // 8
  {1,1,1,1,0,1,1}  // 9
};

void writeSegments(int digitIndex) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segPins[i], digits[digitIndex][i] ? LOW : HIGH);
  }
}

void clearSegments() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segPins[i], HIGH); 
  }
}

void showDigit(int digitValue, int position) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(digitPins[i], LOW);
  }

  
  writeSegments(digitValue);

 
  if (position >= 1 && position <= 4) {
    digitalWrite(digitPins[position - 1], HIGH);
  }

  delay(3); 

  
  clearSegments();
}


void displayNumber(int num) {
  int d1 = (num / 1000) % 10;
  int d2 = (num / 100)  % 10;
  int d3 = (num / 10)   % 10;
  int d4 =  num         % 10;

  for (int i = 0; i < 20; i++) { 
    showDigit(d1, 1);
    showDigit(d2, 2);
    showDigit(d3, 3);
    showDigit(d4, 4);
  }
}

void setup() {
  pinMode(greenPin,  OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin,    OUTPUT);

  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
    digitalWrite(segPins[i], HIGH);
  }

  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], LOW); 
  }
}


void countdown(int pin, int seconds, bool timerMode = false) {
  digitalWrite(greenPin,  LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin,    LOW);
  digitalWrite(pin, HIGH);

  if (timerMode) {
    for (int i = seconds; i > 0; i--) {
      unsigned long start = millis();
      while (millis() - start < 1000) {
        displayNumber(i);
      }
    }
  } else {
    unsigned long start = millis();
    displayNumber(0);
  }
}

void loop() {
  countdown(redPin,    5, true);
  countdown(yellowPin, 2, false);
  countdown(greenPin,  5, true);
  countdown(yellowPin, 2, false);
}