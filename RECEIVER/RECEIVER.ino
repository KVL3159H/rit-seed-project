#include <SPI.h>
#include <LoRa.h>

// -------- LoRa Pins --------
#define SS 27
#define RST 25
#define DIO0 26

String unitID = "N01";   // 👈 Ensure this matches the transmitter target

// -------- Signal Pins --------
// North
#define N_RED 2
#define N_GRN 5

// South
#define S_RED 12
#define S_GRN 14

// East
#define E_RED 15
#define E_GRN 22

// West
#define W_RED 32
#define W_GRN 13

// -------- Timing --------
const unsigned long greenTime = 5000;
const unsigned long overrideTimeout = 3000;

enum Mode { NORMAL, PRIORITY };
Mode currentMode = NORMAL;

int currentSignal = 0;
unsigned long previousMillis = 0;
unsigned long lastReceiveTime = 0;

String priorityDirection = ""; // Stores the parsed direction (N, NE, S, etc.)

void setup() {
  Serial.begin(115200);

  pinMode(N_RED, OUTPUT); pinMode(N_GRN, OUTPUT);
  pinMode(S_RED, OUTPUT); pinMode(S_GRN, OUTPUT);
  pinMode(E_RED, OUTPUT); pinMode(E_GRN, OUTPUT);
  pinMode(W_RED, OUTPUT); pinMode(W_GRN, OUTPUT);

  allRed();

  // SPI for LoRa
  SPI.begin(18, 19, 23, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Failed!");
    while (1);
  }

  Serial.println("Receiver Ready. Waiting for GPS Packets...");
}

void loop() {
  // -------- Check for LoRa Packet --------
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String receivedData = "";

    // Read exactly packetSize bytes
    for (int i = 0; i < packetSize; i++) {
      char c = (char)LoRa.read();
      receivedData += c;
    }

    // Clean the received text
    receivedData.trim();
    receivedData.replace("\n", "");
    receivedData.replace("\r", "");

    Serial.print("Received: [");
    Serial.print(receivedData);
    Serial.println("]");

    // Parse the new packet format (e.g., N01,LAT=9.450064,LON=77.564289,DIR=NE)
    int firstCommaIndex = receivedData.indexOf(',');

    if (firstCommaIndex != -1) {
      String targetUnit = receivedData.substring(0, firstCommaIndex);
      targetUnit.trim();

      // ✅ ONLY ACCEPT IF MATCHES THIS UNIT
      if (targetUnit == unitID) {
        
        // Find where the direction starts
        int dirIndex = receivedData.indexOf("DIR=");
        if (dirIndex != -1) {
          priorityDirection = receivedData.substring(dirIndex + 4);
          priorityDirection.trim();

          currentMode = PRIORITY;
          lastReceiveTime = millis();

          Serial.print("PRIORITY MODE ACTIVATED (MATCHED) -> Direction: ");
          Serial.println(priorityDirection);
        } else {
          Serial.println("Invalid format: Missing DIR= tag");
        }
      } else {
        Serial.println("IGNORED (Not my ID)");
      }
    } else {
      Serial.println("Invalid packet format");
    }
  }

  // -------- Timeout → Back to NORMAL --------
  if (currentMode == PRIORITY && millis() - lastReceiveTime > overrideTimeout) {
    Serial.println("Returning to NORMAL mode");

    currentMode = NORMAL;
    previousMillis = millis();
    currentSignal = 0;

    allRed();
  }

  // -------- Run Modes --------
  if (currentMode == NORMAL) {
    normalCycle();
  } else {
    priorityMode();
  }
}

// ================= NORMAL MODE =================
void normalCycle() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= greenTime) {
    previousMillis = currentMillis;
    currentSignal++;

    if (currentSignal > 3) {
      currentSignal = 0;
    }
  }

  allRed();
  setGreen(currentSignal);
}

// ================= PRIORITY MODE =================
void priorityMode() {
  allRed();

  // Map the physical GPS direction to the correct traffic signal
  if (priorityDirection == "N" || priorityDirection == "NE" || priorityDirection == "NW") {
    setGreen(0); // North
  } 
  else if (priorityDirection == "S" || priorityDirection == "SE" || priorityDirection == "SW") {
    setGreen(1); // South
  } 
  else if (priorityDirection == "E") {
    setGreen(2); // East
  } 
  else if (priorityDirection == "W") {
    setGreen(3); // West
  }
}

// ================= HELPER FUNCTIONS =================
void allRed() {
  digitalWrite(N_RED, HIGH); digitalWrite(N_GRN, LOW);
  digitalWrite(S_RED, HIGH); digitalWrite(S_GRN, LOW);
  digitalWrite(E_RED, HIGH); digitalWrite(E_GRN, LOW);
  digitalWrite(W_RED, HIGH); digitalWrite(W_GRN, LOW);
}

void setGreen(int dir) {
  if (dir == 0) {
    digitalWrite(N_RED, LOW);
    digitalWrite(N_GRN, HIGH);
  }
  if (dir == 1) {
    digitalWrite(S_RED, LOW);
    digitalWrite(S_GRN, HIGH);
  }
  if (dir == 2) {
    digitalWrite(E_RED, LOW);
    digitalWrite(E_GRN, HIGH);
  }
  if (dir == 3) {
    digitalWrite(W_RED, LOW);
    digitalWrite(W_GRN, HIGH);
  }
}