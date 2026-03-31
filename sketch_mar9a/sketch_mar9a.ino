#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <math.h>

// --- 🌐 NETWORK CONFIG ---
const char* ssid = "Pixel_8844";
const char* password = "ezhsrejkcnslla";
const String serverIP = "http://10.25.242.152:5000"; 

// --- 📍 PINS ---
#define MQ135_PIN 34
#define SOS_BUTTON 4
#define BUZZER 13
#define LED_G 25
#define LED_Y 26
#define LED_R 27

// --- SENSORS & TIMING ---
Adafruit_BME280 bme;
const int MPU_ADDR = 0x68;
unsigned long lastUpdateTime = 0;

// --- 🔥 UPDATED CONFIG ---
const int updateInterval = 1000;  // 1-second heartbeat
String currentTrigger = "STABLE"; // Tracks status for User App

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  pinMode(SOS_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);

  connectToWiFi();

  if (!bme.begin(0x76)) { Serial.println("BME280 Error!"); }

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission(true);

  // Ready Beeps
  digitalWrite(LED_G, HIGH); delay(500); digitalWrite(LED_G, LOW);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
}

void postToServer(String endpoint, String jsonPayload) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverIP + endpoint);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonPayload);
    http.end();
  }
}

void triggerSOS(String type, float t, float p, float a, int g) {
  digitalWrite(LED_R, HIGH);
  digitalWrite(BUZZER, HIGH);
  
  String json = "{\"id\":\"TREKKER_01\",\"type\":\"" + type + 
                "\",\"temp\":" + String(t) + 
                ",\"pres\":" + String(p) + 
                ",\"alt\":" + String(a) + 
                ",\"gas\":" + String(g) + "}";

  postToServer("/api/sos", json);

  delay(1000); 
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_R, LOW);
}

void loop() {
  // 1. Read MPU6050 (For Fall Detection)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  float ax = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float ay = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float az = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float totalAcc = sqrt(ax*ax + ay*ay + az*az);

  // 2. Read Environment Sensors
  int gas = analogRead(MQ135_PIN);
  float temp = bme.readTemperature();
  float pres = bme.readPressure() / 100.0F; 
  float alt = bme.readAltitude(1013.25);

  // 3. 🔥 LOGIC: Automatic Trigger Status for User App
  if (gas > 2500) currentTrigger = "GAS_ALERT";
  else if (temp > 38.0) currentTrigger = "HEAT_ALERT";
  else currentTrigger = "STABLE";

  // 4. 🔥 Every 1 Second: Send Data with Status Trigger
  if (millis() - lastUpdateTime > updateInterval) {
    String sensorJson = "{\"temp\":" + String(temp, 1) + 
                        ",\"pres\":" + String(pres, 0) + 
                        ",\"alt\":" + String(alt, 0) + 
                        ",\"gas\":" + String(gas) + 
                        ",\"status\":\"" + currentTrigger + "\"}"; // Status included
    
    postToServer("/api/sensors", sensorJson);
    lastUpdateTime = millis();
    
    // Heartbeat Blink
    digitalWrite(LED_G, HIGH); delay(20); digitalWrite(LED_G, LOW);
  }

  // 5. Physical SOS Button
  if (digitalRead(SOS_BUTTON) == LOW) {
    triggerSOS("MANUAL_SOS", temp, pres, alt, gas);
    delay(2000); 
  }

  // 6. Automatic Fall Detection logic
  if (totalAcc < 0.3) { 
      delay(50);
      if (totalAcc > 2.5) { 
          triggerSOS("AUTO_FALL", temp, pres, alt, gas);
      }
  }

  // 7. Local Hardware Alerts (Buzzer/LED_Y)
  if (currentTrigger != "STABLE") {
    digitalWrite(LED_Y, HIGH);
    digitalWrite(BUZZER, HIGH); delay(50); digitalWrite(BUZZER, LOW);
  } else {
    digitalWrite(LED_Y, LOW);
  }

  delay(100);
}