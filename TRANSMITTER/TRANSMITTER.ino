#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

// -------- LoRa Pins (ESP32) --------
#define SS 5
#define RST 14
#define DIO0 26

// -------- GPS Pins (ESP32 Hardware Serial 2) --------
#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600
TinyGPSPlus gps;

// -------- Green LED Pin --------
#define GREEN_LED_PIN 25  // Connect a Green LED to GPIO 25

// -------- Transmission Settings --------
String targetUnit = "N01";               // Default target ID
bool isSending = true;                   // Starts sending immediately
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // Send packet every 1 second

// ========== Convert GPS Course (Degrees) to Cardinal Direction ==========
String getCardinalDirection(double courseDeg) {
  const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int index = (int)((courseDeg + 22.5) / 45.0) % 8;
  return String(directions[index]);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize LED
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH); // Turn ON by default
  
  // Initialize GPS Serial
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  
  // Initialize LoRa
  SPI.begin(18, 19, 23, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("[ERROR] LoRa Initialization FAILED! Check wiring.");
    while (1); // Halt execution if LoRa fails
  }
  
  Serial.println("=========================================");
  Serial.println(" LoRa GPS Transmitter Ready!");
  Serial.println(" Commands: START, END, N01-START, N01-END");
  Serial.println("=========================================\n");
}

void loop() {
  // -------- 1. Handle Serial Commands --------
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();        // Remove spaces and hidden characters
    input.toUpperCase(); // Ensure commands are uppercase
    
    if (input == "END") {
      isSending = false;
      digitalWrite(GREEN_LED_PIN, LOW); // Turn OFF Green LED
      Serial.println("[CMD] Transmission STOPPED. Green LED OFF.");
    } 
    else if (input == "START") {
      isSending = true;
      digitalWrite(GREEN_LED_PIN, HIGH); // Turn ON Green LED
      Serial.println("[CMD] Transmission STARTED. Green LED ON.");
    } 
    else {
      int separatorIndex = input.indexOf('-');
      if (separatorIndex != -1) {
        String newTarget = input.substring(0, separatorIndex);
        String command = input.substring(separatorIndex + 1);
        
        targetUnit = newTarget;
        
        if (command == "START") {
          isSending = true;
          digitalWrite(GREEN_LED_PIN, HIGH);
          Serial.print("[CMD] Target updated to: "); Serial.println(targetUnit);
          Serial.println("[CMD] Transmission STARTED. Green LED ON.");
        } 
        else if (command == "END") {
          isSending = false;
          digitalWrite(GREEN_LED_PIN, LOW);
          Serial.print("[CMD] Target updated to: "); Serial.println(targetUnit);
          Serial.println("[CMD] Transmission STOPPED. Green LED OFF.");
        } 
        else {
          Serial.println("[ERROR] Unknown target command. Format: N01-START");
        }
      } 
    }
  }

  // -------- 2. Read Incoming GPS Data --------
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // -------- 3. Send LoRa Packet at Interval --------
  if (isSending && (millis() - lastSendTime >= sendInterval)) {
    lastSendTime = millis();

    String latStr = "0.000000";
    String lonStr = "0.000000";
    String dirStr = "N/A";
    
    // Process GPS coordinates if a valid lock is found
    if (gps.location.isValid()) {
      double lat = gps.location.lat();
      double lon = gps.location.lng();
      
      latStr = String(lat, 6);
      lonStr = String(lon, 6);
      
      // Get movement direction from GPS Course
      if (gps.course.isValid()) {
        dirStr = getCardinalDirection(gps.course.deg());
      }
    }

    // Build the packet format: N01,LAT=9.450060,LON=77.564244,DIR=NW
    String loraPacket = targetUnit + ",LAT=" + latStr + ",LON=" + lonStr + ",DIR=" + dirStr;

    // Transmit over LoRa
    LoRa.beginPacket();
    LoRa.print(loraPacket);
    LoRa.endPacket();

    // Print to Serial Monitor for debugging
    Serial.print("Sent: ");
    Serial.println(loraPacket);
  }
}