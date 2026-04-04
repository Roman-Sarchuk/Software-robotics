#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ===== WIFI =====
const char* ssid = "Qwerty";
const char* password = "xyse0000";

WebServer server(80);

// ===== DHT11 =====
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== MQ2 =====
#define MQ2_PIN 34

// ===== LEDs =====
#define BLUE_LED 23
#define GREEN_LED 22
#define YELLOW_LED 21
#define RED_LED 19

// ===== BUZZER =====
#define BUZZER 12

// ===== BUTTON =====
#define BUTTON 18

// ===== VARIABLES =====
volatile bool buzzerMuted = false;  // змінна для переривання
float temperature = 0;
float humidity = 0;
int gasValue = 0;

float lastTemp = 0;
bool fireAlert = false;
bool warning = false;

// ===== BUTTON INTERRUPT =====
void IRAM_ATTR toggleBuzzer() {
  buzzerMuted = !buzzerMuted;
}

// ===== HTML PAGE =====
String getHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background:#111;color:#fff;}";
  html += ".card{background:#222;padding:20px;margin:10px;border-radius:10px;}"; 
  html += ".led{display:inline-block;width:25px;height:25px;margin:5px;border-radius:50%;}";
  html += ".green{background:lime;} .red{background:red;} .blue{background:blue;} .yellow{background:yellow;} .off{background:#444;}";
  html += "</style></head><body>";

  html += "<h1>ESP32 Air Monitor</h1>";
  html += "<div class='card'>Temp: " + String(temperature) + " °C</div>";
  html += "<div class='card'>Humidity: " + String(humidity) + " %</div>";
  html += "<div class='card'>Gas: " + String(gasValue) + "</div>";

  html += "<h2>LED Status</h2><div>";
  html += "<div class='led " + String(!warning && !fireAlert ? "green" : "off") + "'></div>";
  html += "<div class='led " + String(fireAlert ? "red" : "off") + "'></div>";
  html += "<div class='led " + String(buzzerMuted ? "blue" : "off") + "'></div>";
  html += "<div class='led " + String(warning && !fireAlert ? "yellow" : "off") + "'></div>";
  html += "</div>";

  if (fireAlert) html += "<h2 style='color:red'>FIRE ALERT</h2>";
  else if (warning) html += "<h2 style='color:orange'>WARNING</h2>";
  else html += "<h2 style='color:lime'>ALL GOOD</h2>";

  if (buzzerMuted) html += "<p>Sound Muted</p>";
  html += "</body></html>";
  return html;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON), toggleBuzzer, FALLING); // кнопка через переривання

  // WIFI
  WiFi.begin(ssid, password);
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.begin();
}

// ===== LOOP =====
void loop() {
  // ===== READ SENSORS =====
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  gasValue = analogRead(MQ2_PIN);

  // ===== LOGIC =====
  warning = false;
  fireAlert = false;
  float tempRise = temperature - lastTemp;

  if (gasValue > 2000 || temperature > 35) warning = true;
  if (tempRise > 2.0 && gasValue > 2500) fireAlert = true;

  lastTemp = temperature;

  // ===== LED CONTROL =====
  digitalWrite(GREEN_LED, !warning && !fireAlert);
  digitalWrite(BLUE_LED, buzzerMuted);
  digitalWrite(RED_LED, fireAlert ? (millis() % 500 < 250) : LOW);
  digitalWrite(YELLOW_LED, warning && !fireAlert);

  // ===== SERVER =====
  server.handleClient();

  delay(2000);
}