#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "BluetoothSerial.h"

// --- CONFIG ---
const char* ssid = "Pixel_8844";
const char* password = "ezhsrejkcnslla";
const char* serverURL = "http://10.232.128.152:5000/api/sos";

// --- PINS (From your Table) ---
#define MQ135_PIN 34
#define SOS_BUTTON 4
#define BUZZER 13
#define LED_G 25
#define LED_Y 26
#define LED_R 27

BluetoothSerial SerialBT;
Adafruit_BME280 bme;
bool streamActive = false;
const int MPU_ADDR = 0x68;

void setup() {
  SerialBT.begin("TrekGuard_v1");
  Wire.begin(21, 22);
  pinMode(SOS_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);

  digitalWrite(LED_G, HIGH); delay(200); digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, HIGH); delay(200); digitalWrite(LED_Y, LOW);
  digitalWrite(LED_R, HIGH); delay(200); digitalWrite(LED_R, LOW);
  digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
  digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
  digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
  
  // Initialize BMP280
  if (!bme.begin(0x76)) { SerialBT.println("BMP280 Error!"); }

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission(true);
}

void triggerSOS(String type, float t, float p, float a, int g) {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  digitalWrite(BUZZER, HIGH);
  
  SerialBT.println("!!! SOS ACTIVE: " + type + " !!!");

  // Connect to Wi-Fi to notify HQ
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) { delay(500); retry++; }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    // Prepare JSON data with all sensor values
    String json = "{\"id\":\"TREKKER_01\",\"type\":\""+type+"\",\"temp\":"+String(t)+",\"pres\":"+String(p)+",\"alt\":"+String(a)+",\"gas\":"+String(g)+"}";
    http.POST(json);
    http.end();
    SerialBT.println("HQ Notified successfully.");
   }
  digitalWrite(BUZZER,LOW);
  delay(2000);
  digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); delay(1000); 
  digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); delay(1000); 
  digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW); delay(1000); 
  digitalWrite(LED_R, LOW);


}

void loop() {
  if (SerialBT.available()) {
    char cmd = SerialBT.read();
    if (cmd == 'D') { streamActive = true; SerialBT.println("CONNECTED: Streaming Health Data..."); }
  }

  // 1. Read MPU6050 for Automatic Fall Detection
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  float ax = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float ay = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float az = (Wire.read() << 8 | Wire.read()) / 16384.0;
  float totalAcc = sqrt(ax*ax + ay*ay + az*az);

  // 2. Read all sensor
  int gas = analogRead(MQ135_PIN);
  float temp = bme.readTemperature();
  float pres = bme.readPressure() / 100.0F; // hPa
  float alt = bme.readAltitude(1013.25);

  // 3. LOGIC: Automatic Fall Detection
  if (totalAcc < 0.4) { // Near weightless (Fall)
     delay(100);
     if (totalAcc > 2.8) { triggerSOS("AUTO_FALL", temp, pres, alt, gas); }
  }

  // 4. LOGIC: Manual SOS Button
  if (digitalRead(SOS_BUTTON) == LOW) {
    triggerSOS("MANUAL_SOS", temp, pres, alt, gas);
    delay(10000);
  }

  // 5. LED Visual Feedback (Based on Gas Levels)
  if (gas > 2000) { // Dangerous Gas
    digitalWrite(LED_G, LOW);
    delay(1000);
    digitalWrite(LED_Y, HIGH);
    SerialBT.println("Local trigger for Dangerous gas...");
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); delay(100);
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); // Chirp
  } else {
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_Y, LOW);
  }

   if (temp > 32.0) { // more temperature
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    delay(1000);
    SerialBT.println("Local trigger for more temperature...");
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); delay(100);
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); delay(100);
     digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); delay(100);
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);  // Chirp
  } else {
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_Y, LOW);
  }

  // 6. Bluetooth Monitoring (Send to Phone)
 if (streamActive) {
    SerialBT.printf("Temperature:%.1fC \n", temp);
    SerialBT.printf("Pressure:%.1f \n",pres);
    SerialBT.printf(" Attitude:%.1fm \n",alt);
    SerialBT.printf(" Gas value:%d \n", gas);
    digitalWrite(LED_G, HIGH); delay(50); digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH); delay(200); digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, HIGH); delay(200); digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH); delay(50); digitalWrite(LED_G, LOW);
    streamActive = false;
  }

  delay(500);
}
 
