/*
  Project: Kissan Saathi - Smart Irrigation
  Hardware:
  - ESP32
  - DHT11
  - Soil Moisture Sensor
  - 16x2 I2C LCD
  - Relay Module
  - DC Pump
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* =========================
   WiFi Credentials
========================= */

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

/* =========================
   Backend URLs
========================= */

const char* sensorUrl = "http://192.168.1.7:5000/api/esp32";
const char* pumpUrl   = "http://192.168.1.7:5000/api/pump";

/* =========================
   Pins
========================= */

#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define RELAY_PIN 23

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* =========================
   Settings
========================= */

float moistureThreshold = 30.0;
bool manualOverride = false;

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

/* =========================
   Setup
========================= */

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // OFF initially (active LOW)

  dht.begin();
  lcd.init();
  lcd.backlight();

  connectWiFi();
}

/* =========================
   Loop
========================= */

void loop() {

  if ((millis() - lastTime) > timerDelay) {

    if (WiFi.status() == WL_CONNECTED) {

      float temperature = dht.readTemperature();
      float humidity = dht.readHumidity();
      float moisture = readSoilMoisture();

      displayOnLCD(temperature, humidity, moisture);

      sendSensorData(temperature, humidity, moisture);

      checkPumpCommand();

      autoLogic(moisture);
    }

    lastTime = millis();
  }
}

/* =========================
   WiFi
========================= */

void connectWiFi() {

  WiFi.begin(ssid, password);
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1000);
  lcd.clear();
}

/* =========================
   Soil Sensor
========================= */

float readSoilMoisture() {
  int raw = analogRead(SOIL_PIN);
  float percent = map(raw, 4095, 0, 0, 100);
  return percent;
}

/* =========================
   LCD Display
========================= */

void displayOnLCD(float temp, float hum, float moist) {

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(temp);
  lcd.print(" H:");
  lcd.print(hum);

  lcd.setCursor(0,1);
  lcd.print("M:");
  lcd.print(moist);

  if (digitalRead(RELAY_PIN) == LOW)
    lcd.print(" P:ON");
  else
    lcd.print(" P:OFF");
}

/* =========================
   Send Data
========================= */

void sendSensorData(float temp, float hum, float moist) {

  HTTPClient http;
  http.begin(sensorUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["moisture"] = moist;

  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);

  Serial.print("Data Sent: ");
  Serial.println(httpCode);

  http.end();
}

/* =========================
   Fetch Pump Command
========================= */

void checkPumpCommand() {

  HTTPClient http;
  http.begin(pumpUrl);

  int httpCode = http.GET();

  if (httpCode == 200) {

    String payload = http.getString();

    if (payload.indexOf("ON") > 0) {
      digitalWrite(RELAY_PIN, LOW);
      manualOverride = true;
    }
    else if (payload.indexOf("OFF") > 0) {
      digitalWrite(RELAY_PIN, HIGH);
      manualOverride = true;
    }
  }

  http.end();
}

/* =========================
   Auto Logic
========================= */

void autoLogic(float moisture) {

  if (manualOverride) return;

  if (moisture < moistureThreshold) {
    digitalWrite(RELAY_PIN, LOW);   // Pump ON
  }
  else {
    digitalWrite(RELAY_PIN, HIGH);  // Pump OFF
  }
}
