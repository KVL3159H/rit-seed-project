#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

// -------- LoRa Pins --------
#define SS 5
#define RST 14
#define DIO0 26

// -------- GPS Pins --------
#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600
TinyGPSPlus gps;

// -------- Compass Pins --------
#define SDA_PIN 21
#define SCL_PIN 22
Adafruit_HMC5883_Unified compass = Adafruit_HMC5883_Unified(12345);
bool compassFound = false;
float X_OFFSET = 0.0, Y_OFFSET = 0.0;

// -------- Destination (change to your hospital/base) --------
const double DEST_LAT = 9.500000;   // e.g., hospital lat
const double DEST_LON = 77.520000;  // e.g., hospital lon

// -------- Transmission settings --------
String targetUnit = "N01";     // default target (change as needed)
bool isSending = true;
unsigned long lastSend = 0;
const unsigned long interval = 1000;   // send every 1 sec

// ========== Compass calibration ==========
void calibrateCompass() {
  if (!compassFound) return;
  Serial.println("Calibrating compass... Rotate 360° slowly for 5 sec.");
  float minX = 9999, maxX = -9999, minY = 9999, maxY = -9999;
  unsigned long start = millis();
  while (millis() - start < 5000) {
    sensors_event_t e;
    compass.getEvent(&e);
    if (e.magnetic.x < minX) minX = e.magnetic.x;
    if (e.magnetic.x > maxX) maxX = e.magnetic.x;
    if (e.magnetic.y < minY) minY = e.magnetic.y;
    if (e.magnetic.y > maxY) maxY = e.magnetic.y;
    delay(50);
  }
  X_OFFSET = (minX + maxX) / 2.0;
  Y_OFFSET = (minY + maxY) / 2.0;
  Serial.print("Calibration done: X_OFFSET="); Serial.print(X_OFFSET);
  Serial.print("  Y_OFFSET="); Serial.println(Y_OFFSET);
}

// ========== 4‑direction from heading ==========
String getDirection4(float headingDeg) {
  const char* dirs[] = {"N", "E", "S", "W"};
  int idx = (int)((headingDeg + 45.0) / 90.0) % 4;
  return String(dirs[idx]);
}

// ========== Bearing between two points ==========
double bearingTo(double lat1, double lon1, double lat2, double lon2) {
  double dLon = radians(lon2 - lon1);
  double y = sin(dLon) * cos(radians(lat2));
  double x = cos(radians(lat1)) * sin(radians(lat2)) -
             sin(radians(lat1)) * cos(radians(lat2)) * cos(dLon);
  double brng = atan2(y, x);
  brng = degrees(brng);
  if (brng < 0) brng += 360.0;
  return brng;
}

// ========== Get direction to destination ==========
String getDirectionToDest(double lat, double lon) {
  double bearing = bearingTo(lat, lon, DEST_LAT, DEST_LON);
  return getDirection4(bearing);
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);

  // GPS
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  // Compass
  Wire.begin(SDA_PIN, SCL_PIN);
  compassFound = compass.begin();
  if (compassFound) calibrateCompass();

  // LoRa
  SPI.begin(18, 19, 23, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Fail");
    while (1);
  }

  Serial.println("Transmitter Ready (GPS + Compass)");
  Serial.println("Commands: N01-START or N01-END");
}

// ========== Main Loop ==========
void loop() {
  // -------- Serial input for control --------
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "END") {
      isSending = false;
      Serial.println("Transmission STOPPED");
      return;
    }
    if (input == "START") {
      isSending = true;
      Serial.println("Transmission STARTED");
      return;
    }

    int sep = input.indexOf('-');
    if (sep != -1) {
      String newTarget = input.substring(0, sep);
      String command = input.substring(sep + 1);
      newTarget.trim();
      command.trim();

      if (command == "START") {
        targetUnit = newTarget;
        isSending = true;
        Serial.print("Target set to "); Serial.println(targetUnit);
      } else if (command == "END") {
        targetUnit = newTarget;
        isSending = false;
        Serial.print("Target set to "); Serial.println(targetUnit);
        Serial.println("Transmission stopped");
      } else {
        Serial.println("Invalid command. Use N01-START or N01-END");
      }
    } else {
      Serial.println("Format: N01-START");
    }
  }

  // -------- Read GPS data --------
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // -------- Send packet if GPS fix and enabled --------
  if (isSending && millis() - lastSend >= interval) {
    lastSend = millis();

    double lat = 0.0, lon = 0.0;
    bool fix = gps.location.isValid();
    if (fix) {
      lat = gps.location.lat();
      lon = gps.location.lng();
    }

    // Compute direction (only if GPS fix)
    String dir = "N/A";
    if (fix) {
      dir = getDirectionToDest(lat, lon);
    }

    // Build packet: targetUnit, GPS data, and direction
    String packet = targetUnit + ",";
    packet += "LAT=" + String(lat, 6) + ",";
    packet += "LON=" + String(lon, 6) + ",";
    packet += "DIR=" + dir;

    // Send via LoRa
    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();

    Serial.print("Sent: ");
    Serial.println(packet);
  }
}