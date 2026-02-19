#define dataPin 6
#define clockPin 7
#define latchPin 8

#define Dig1 5
#define Dig2 4
#define Dig3 3
#define Dig4 2

#define S1 A0
#define S2 A2

byte display[4];
byte symb[] = {
  B00011100, // L (0)
  B11111100, // O (1)
  B11101110, // A (2)
  B11111100, // D (3)
  B11001110, // P (4)
  B01110110, // Y (5)
  B10110110, // S (6)
  B00011110  // T (7)
};

void setup() {
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
 
  pinMode(Dig1, OUTPUT);
  pinMode(Dig2, OUTPUT);
  pinMode(Dig3, OUTPUT);
  pinMode(Dig4, OUTPUT);

  pinMode(S1, INPUT_PULLUP);
  pinMode(S2, INPUT_PULLUP);

  loadLOAD();
}

void loop() {
  if (digitalRead(S1) == LOW) {
    loadPLAY();
    delay(300);
  }

  if (digitalRead(S2) == LOW) {
    loadSTOP();
    delay(300);
  }

  show();
}

void loadLOAD() {
  display[0] = symb[0]; // L
  display[1] = symb[1]; // O
  display[2] = symb[2]; // A
  display[3] = symb[3]; // D
}

void loadPLAY() {
  display[0] = symb[4]; // P
  display[1] = symb[0]; // L
  display[2] = symb[2]; // A
  display[3] = symb[5]; // Y
}

void loadSTOP() {
  display[0] = symb[6]; // S
  display[1] = symb[7]; // T
  display[2] = symb[1]; // O
  display[3] = symb[4]; // P
}

void show() {
  showDigit(Dig1, display[0]);
  showDigit(Dig2, display[1]);
  showDigit(Dig3, display[2]);
  showDigit(Dig4, display[3]);
}

void showDigit(byte digPin, byte val) {
  digitalWrite(digPin, HIGH);
  shiftOut(dataPin, clockPin, MSBFIRST, val);
  digitalWrite(latchPin, HIGH);
  digitalWrite(latchPin, LOW);
  delay(3);
  digitalWrite(digPin, LOW);
}