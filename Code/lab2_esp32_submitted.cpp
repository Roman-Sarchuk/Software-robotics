/*

Components:
- freenove esp32 wrover
- 3 LEDs (red, yellow, green)
- 3 resistors (220 ohm)
- 7-segment display (4 digits)
- 7 resistors (220 ohm)
- Ultrasonic sensor HC-SR04
- Piezo buzzer

*/

// ----- LED pins -----
const int greenPin  = 15;
const int yellowPin = 4;
const int redPin    = 19;

// ----- Piezo pin -----
const int piezoPin = 13;

// ----- HC-SR04 pins -----
const int trigPin = 21;
const int echoPin = 34;

// ----- Buttons -----
const int powerButton = 12;

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

// ----- Digit patterns 0-9 (common-anode) -----
const bool digits[10][7] = {
  {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1},
  {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0},
  {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
};

// ----- System params -----
enum SoundType { GREEN, YELLOW, RED, CRITICAL };
volatile bool isSystemEnabled = true;
unsigned long lastInterrupt = 0;

// ----- Distance -----
float dist = 0;
int digitsToShow[4] = {0,0,0,0};
unsigned long lastDistanceRead = 0;

// ----- Segment functions -----
void writeSegments(int digitIndex) {
  for (int i = 0; i < 7; i++)
    digitalWrite(segPins[i], digits[digitIndex][i] ? LOW : HIGH);
}

void clearSegments() {
  for (int i = 0; i < 7; i++) digitalWrite(segPins[i], HIGH);
}

// ----- Show one digit (non-blocking) -----
void showDigit(int digitValue, int position) {
  for (int i = 0; i < 4; i++) digitalWrite(digitPins[i], LOW);
  writeSegments(digitValue);
  digitalWrite(digitPins[position], HIGH);
}

// ----- Ultrasonic -----
long readUltrasonicDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 50000);
  if(duration == 0) return -1;
  return duration;
}

// ----- Piezo -----
void makeSound(SoundType soundType) {
  switch(soundType){
    case GREEN: tone(piezoPin, 150, 200); break;
    case YELLOW: tone(piezoPin, 300, 200); break;
    case RED: tone(piezoPin, 400, 200); break;
    case CRITICAL: tone(piezoPin, 523, 200); break;
  }
}

// ----- Button interrupt -----
void IRAM_ATTR handleButtonInterrupt() {
  unsigned long now = millis();
  if(now - lastInterrupt > 200){
    isSystemEnabled = !isSystemEnabled;
    lastInterrupt = now;
  }
}

// ----- Setup -----
void setup() {
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  for(int i=0;i<7;i++){ pinMode(segPins[i], OUTPUT); digitalWrite(segPins[i], HIGH); }
  for(int i=0;i<4;i++){ pinMode(digitPins[i], OUTPUT); digitalWrite(digitPins[i], LOW); }

  pinMode(powerButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(powerButton), handleButtonInterrupt, FALLING);
}

// ----- Loop -----
void loop() {
  static int currentDigit = 0;
  unsigned long now = millis();

  // ----- Read distance 5 раз на секунду -----
  if(now - lastDistanceRead > 200 && isSystemEnabled){
    long duration = readUltrasonicDistance(trigPin, echoPin);
    dist = 0.01723 * duration;
    if(dist > 9999) dist = 9999;
    int temp = (int)dist;
    digitsToShow[0] = temp/1000%10;
    digitsToShow[1] = temp/100%10;
    digitsToShow[2] = temp/10%10;
    digitsToShow[3] = temp%10;

    // LED + Piezo
    if (dist > 60) { digitalWrite(greenPin,HIGH); digitalWrite(yellowPin,LOW); digitalWrite(redPin,LOW); makeSound(GREEN);}
    else if (dist > 30) { digitalWrite(greenPin,LOW); digitalWrite(yellowPin,HIGH); digitalWrite(redPin,LOW); makeSound(YELLOW);}
    else if (dist > 15) { digitalWrite(greenPin,LOW); digitalWrite(yellowPin,LOW); digitalWrite(redPin,HIGH); makeSound(RED);}
    else { digitalWrite(greenPin,LOW); digitalWrite(yellowPin,LOW); digitalWrite(redPin,HIGH); makeSound(CRITICAL);}
    
    lastDistanceRead = now;
  }

  // ----- Update display one digit at a time -----
  showDigit(digitsToShow[currentDigit], currentDigit);
  currentDigit = (currentDigit+1)%4; 

  delay(2); 
}