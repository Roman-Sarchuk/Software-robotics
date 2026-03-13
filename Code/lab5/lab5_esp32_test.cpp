/**
   Simon Game for ESP32-C3 with Score display

   Copyright (C) 2023, Uri Shaked

   Released under the MIT License.
*/

#include "pitches.h"

/* Define pin numbers for LEDs, buttons and speaker: */
const uint8_t buttonPins[] = {13,12,14,27};
const uint8_t ledPins[] = {23,22,25,26};
#define SPEAKER_PIN 32

// These are connected to 74HC595 shift register (used to show game score):
const int LATCH_PIN = 18;  // 74HC595 pin 12
const int DATA_PIN = 19;   // 74HC595 pin 14
const int CLOCK_PIN = 5;  // 74HC595 pin 11

#define MAX_GAME_LENGTH 100

const int gameTones[] = { NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

/* Global variables - store the game state */
uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex = 21;

/**
   Set up the Arduino board and initialize Serial communication
*/
void setup() {
  Serial.begin(9600);
  for (byte i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);

  // The following line primes the random number generator.
  // It assumes pin 4 is floating (disconnected):
  randomSeed(analogRead(4));
}

/* Digit table for the 7-segment display */
const uint8_t digitTable[] = {
  0b11000000,
  0b11111001,
  0b10100100,
  0b10110000,
  0b10011001,
  0b10010010,
  0b10000010,
  0b11111000,
  0b10000000,
  0b10010000,
};
const uint8_t DASH = 0b10111111;

void sendScore(uint8_t high, uint8_t low) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, low);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, high);
  digitalWrite(LATCH_PIN, HIGH);
}

void displayScore() {
  int high = gameIndex % 100 / 10;
  int low = gameIndex % 10;
  sendScore(high ? digitTable[high] : 0xff, digitTable[low]);
}

/**
    Waits until the user pressed one of the buttons,
    and returns the index of that button
*/
byte readButtons() {
  for (byte i = 0; i < 4; i++) {
      byte buttonPin = buttonPins[i];
      if (digitalRead(buttonPin) == LOW) {
        return i;
      }
    }
  return -1;
}

int button_answer = 0;
int nums[10] = {12,23,34,45,56,67,78,89,91,0};

void test_SSD() {
  for (int i = 0; i < 10; i++) {
    gameIndex = nums[i];
    displayScore();
    delay(500);
  }
}
void led_test() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(500);
    digitalWrite(ledPins[i], LOW);
  }
}
void button_test() {
  // buttons
  button_answer = readButtons();

  // leds
  for (byte i = 0; i < 4; i++) {
    if (i == button_answer)
      digitalWrite(ledPins[button_answer], HIGH);
    else
      digitalWrite(ledPins[i], LOW);
  }
}

/**
   The main game loop
*/
void loop() {
  led_test();

  // button_test();

  // test_SSD();
}
