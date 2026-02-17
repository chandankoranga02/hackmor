/*
  Project: Kissan Saathi - Stable IoT Version
  Features:
  - Proper JSON parsing
  - Mode based control (AUTO / MANUAL)
  - WiFi reconnect
  - Safe pump fallback
  - Clean architecture
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

const char* ssid = "Airtel_Muh me lega?";
const char* password = "Lega_nhi_dega1";

/* =========================
   Backend URLs
========================= */

const char* sensorUrl = "http://192.168.1.37:5000/api/esp32";
const char* pumpUrl   = "http://192.168.1.37:5000/api/pump";

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
unsigned long lastSendTime = 0;
unsigned long interval = 5000;  // 5 seconds
unsigned long lastCommandTime = 0;

/* =========================
   Setup
========================= */

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Pump OFF (active LOW relay)

  dht.begin();
  lcd.init();
  lcd.backlight();

  connectWiFi();
}

/* =========================
   Main Loop
========================= */

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  if (millis() - lastSendTime > interval) {

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float moisture = readSoilMoisture();

    displayOnLCD(temperature, humidity, moisture);

    sendSensorData(temperature, humidity, moisture);

    checkPumpCommand(moisture);

    lastSendTime = millis();
  }

  // Safety Fail-Safe (20 sec no command)
  if (millis() - lastCommandTime > 20000) {
    digitalWrite(RELAY_PIN, HIGH); // Force OFF
  }
}

/* =========================
   WiFi Connect
========================= */

void connectWiFi() {

  WiFi.begin(ssid, password);

  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1000);
  lcd.clear();

  Serial.println("WiFi Connected");
}

/* =========================
   Soil Moisture
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

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print(" H:");
  lcd.print(hum, 0);

  lcd.setCursor(0, 1);
  lcd.print("M:");
  lcd.print(moist, 0);

  if (digitalRead(RELAY_PIN) == LOW)
    lcd.print(" P:ON");
  else
    lcd.print(" P:OFF");
}

/* =========================
   Send Sensor Data
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

  Serial.print("Sensor POST Code: ");
  Serial.println(httpCode);

  http.end();
}

/* =========================
   Pump Control Logic
========================= */

void checkPumpCommand(float moisture) {

  HTTPClient http;
  http.begin(pumpUrl);

  int httpCode = http.GET();

  if (httpCode == 200) {

    String payload = http.getString();

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {

      String state = doc["state"];
      String mode = doc["mode"];
      bool safety = doc["safetyActive"];

      lastCommandTime = millis();

      if (safety) {
        digitalWrite(RELAY_PIN, HIGH);
        return;
      }

      if (mode == "MANUAL") {

        if (state == "ON")
          digitalWrite(RELAY_PIN, LOW);
        else
          digitalWrite(RELAY_PIN, HIGH);
      }
      else if (mode == "AUTO") {

        if (moisture < moistureThreshold)
          digitalWrite(RELAY_PIN, LOW);
        else
          digitalWrite(RELAY_PIN, HIGH);
      }
    }
  }

  http.end();
}
