/*

Components:
- freenove esp32 wrover
- 74hc595 shift register
- 7-segment display (4 digits)
- 7 resistors (220 ohm)
- rgb led
- 3 resistors (220 ohm)
- Potentiometer
- Piezo buzzer
- Button

*/

// ----- 74hc595 -----
// #define HC_DATA 16
// #define HC_LATCH 17
// #define HC_CLOCK 18
#define HC_DATA 19
#define HC_LATCH 18
#define HC_CLOCK 5

// ----- seven segment display 4 digits -----
// #define SSD_D1 19 
// #define SSD_D2 21
// #define SSD_D3 22
// #define SSD_D4 23
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

// ----- Potentiometer -----
#define POTENTIOMETER 34

// ----- Buzzer -----
#define BUZZER 22

// ----- Button -----
#define TONE_BUTTON 23
#define SPECTOR_BUTTON 21


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
unsigned long lastInterruptToneButton = 0;
volatile bool isTone = false;

void IRAM_ATTR handleToneButtonInterrupt() {
  unsigned long now = millis();
  if(now - lastInterruptToneButton > 200){
    isTone = !isTone;

    lastInterruptToneButton = now;
  }
}

unsigned long lastInterruptSpectorButton = 0;
volatile byte spectorState = 0;

void IRAM_ATTR handleSpectorButtonInterrupt() {
  unsigned long now = millis();
  if(now - lastInterruptSpectorButton > 200){
    if (spectorState >= 2) {
      spectorState = 0;
    } else {
      spectorState++;
    }

    lastInterruptSpectorButton = now;
  }
}


// --- -- task functions --- --
int number = 0;

void task_SSD4d() {
  displayNumber(number);
}

void task_Potentiometer() {
  int adc = analogRead(POTENTIOMETER);
  
  // SSD4d
  number = adc;

  // RGB led
  int r=0,g=0,b=0;
  switch (spectorState) {
    case 0:
      r = map(adc, 0, 4095, 0, 255);
      g = map(adc, 0, 4095, 255, 0);
      b = map(adc, 0, 4095, 0, 128);
      break;
    
    case 1:
      r = map(adc, 0, 4095, 0, 128);
      g = map(adc, 0, 4095, 255, 0);
      b = map(adc, 0, 4095, 0, 255);
      break;

    case 2:
      r = map(adc, 0, 4095, 0, 255);
      g = map(adc, 0, 4095, 0, 128);
      b = map(adc, 0, 4095, 255, 0);
      break;
  }

  analogWrite(LED_R, 255 - r);
  analogWrite(LED_G, 255 - g);
  analogWrite(LED_B, 255 - b);

  // buzzer
  if (isTone) {
    int freq = map(adc, 0, 4095, 100, 2000);
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
  attachInterrupt(digitalPinToInterrupt(TONE_BUTTON), handleToneButtonInterrupt, FALLING);
  pinMode(SPECTOR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SPECTOR_BUTTON), handleSpectorButtonInterrupt, FALLING);
}

void loop() {
  task_SSD4d();

  task_Potentiometer();

  // delay(10);
}
