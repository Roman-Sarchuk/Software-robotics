/*
  Smart Parking System
  
  Components:
  - Freenove ESP32 WROVER
  - 74HC595 shift register x2 → 7-segment displays (shows free spots count)
  - HC-SR04 x2 (entry sensor + exit/pass-through sensor)
  - Servo (barrier gate)
  - RGB LED (status indicator)
  - Piezo buzzer
*/

#include <ESP32Servo.h>
#include "pitches.h"

// ─── PIN DEFINITIONS ────────────────────────────────────────────────────────

#define SPEAKER_PIN   4

// 74HC595 shift register
#define LATCH_PIN     18
#define DATA_PIN      19
#define CLOCK_PIN      5

// HC-SR04 #1 — Entry sensor (detects approaching car BEFORE barrier)
#define TRIG1         25
#define ECHO1         26

// HC-SR04 #2 — Pass-through sensor (detects car crossing AFTER barrier)
#define TRIG2         27
#define ECHO2         14

// Servo (barrier gate)
#define SERVO_PIN     12

// RGB LED (common cathode)
#define LED_R         21
#define LED_G         22
#define LED_B         23

// ─── CONSTANTS ──────────────────────────────────────────────────────────────
#define MAX_SPOTS          2       // Total parking spots
#define DETECT_DISTANCE_CM 20     // Car detection threshold (cm)
#define SERVO_OPEN         90     // Barrier open angle
#define SERVO_CLOSED       0      // Barrier closed angle
#define GATE_OPEN_TIMEOUT  5000   // Max ms to wait for car to pass through

// ─── GLOBALS ────────────────────────────────────────────────────────────────

Servo barrier;

int freeSpots = MAX_SPOTS;   // Number of available spots
bool gateOpen = false;
unsigned long gateOpenedAt = 0;

// ─── 7-SEGMENT TABLE ────────────────────────────────────────────────────────

const uint8_t digitTable[] = {
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000, // 9
};
const uint8_t SEG_OFF  = 0xFF;  // All segments off
const uint8_t SEG_DASH = 0b10111111;

// ─── SETUP ──────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Shift register
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN,  OUTPUT);

  // Ultrasonic sensors
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);

  // RGB LED
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Buzzer
  pinMode(SPEAKER_PIN, OUTPUT);

  // Servo
  barrier.attach(SERVO_PIN);
  barrier.write(SERVO_CLOSED);

  setLED(255, 165, 0);  // Start: yellow (standby)
  displayFreeSpots();

  Serial.println("Parking system ready.");
}

// ─── MAIN LOOP ───────────────────────────────────────────────────────────────

void loop() {
  long dist1 = measureDistance(TRIG1, ECHO1);
  long dist2 = measureDistance(TRIG2, ECHO2);

  bool carApproaching  = (dist1 > 0 && dist1 <= DETECT_DISTANCE_CM);
  bool carPassedThrough = (dist2 > 0 && dist2 <= DETECT_DISTANCE_CM);

  // ── Car just entered (passed through barrier) ──
  if (gateOpen && carPassedThrough) {
    freeSpots--;
    if (freeSpots < 0) freeSpots = 0;

    Serial.printf("Car entered. Free spots: %d\n", freeSpots);
    beep(NOTE_C5, 150);

    closeGate();
    displayFreeSpots();
    setLED(255, 165, 0);   // yellow = standby
    delay(1500);           // debounce — wait for car to clear sensor
  }

  // ── Gate open timeout (car didn't enter) ──
  if (gateOpen && (millis() - gateOpenedAt > GATE_OPEN_TIMEOUT)) {
    Serial.println("Timeout — closing gate.");
    closeGate();
    setLED(255, 165, 0);
    displayFreeSpots();
  }

  // ── Car approaching and gate not yet open ──
  if (!gateOpen && carApproaching) {
    if (freeSpots > 0) {
      // ✅ Spots available
      Serial.println("Car detected. Spots available — opening gate.");
      setLED(0, 255, 0);   // green
      beep(NOTE_E5, 200);
      openGate();
    } else {
      // ❌ Full
      Serial.println("Car detected. Parking FULL.");
      setLED(255, 0, 0);   // red
      beep(NOTE_A3, 500);
      delay(2000);
      setLED(255, 165, 0); // back to yellow
    }
  }

  delay(100);
}

// ─── GATE CONTROL ────────────────────────────────────────────────────────────

void openGate() {
  barrier.write(SERVO_OPEN);
  gateOpen = true;
  gateOpenedAt = millis();
  Serial.println("Gate opened.");
}

void closeGate() {
  barrier.write(SERVO_CLOSED);
  gateOpen = false;
  Serial.println("Gate closed.");
}

// ─── ULTRASONIC DISTANCE ─────────────────────────────────────────────────────

long measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

// ─── RGB LED ─────────────────────────────────────────────────────────────────

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

// ─── BUZZER ──────────────────────────────────────────────────────────────────

void beep(int frequency, int duration) {
  tone(SPEAKER_PIN, frequency, duration);
  delay(duration + 20);
  noTone(SPEAKER_PIN);
}

// ─── 7-SEGMENT DISPLAY ───────────────────────────────────────────────────────

void sendNumber(uint8_t high, uint8_t low) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, low);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, high);
  digitalWrite(LATCH_PIN, HIGH);
}

void displayFreeSpots() {
  // Shows remaining free spots on the two 7-segment displays
  int val = freeSpots;
  int high = val / 10;
  int low  = val % 10;
  sendNumber(high > 0 ? digitTable[high] : SEG_OFF, digitTable[low]);
}