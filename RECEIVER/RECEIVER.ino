#include <SPI.h>
#include <LoRa.h>

// -------- LoRa Pins --------
#define SS 27
#define RST 25
#define DIO0 26

String unitID = "N02";   // 👈 CHANGE to N02 for second signal unit

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

String lastDirection = "";   // store last valid direction

void setup() {
  Serial.begin(115200);

  pinMode(N_RED, OUTPUT); pinMode(N_GRN, OUTPUT);
  pinMode(S_RED, OUTPUT); pinMode(S_GRN, OUTPUT);
  pinMode(E_RED, OUTPUT); pinMode(E_GRN, OUTPUT);
  pinMode(W_RED, OUTPUT); pinMode(W_GRN, OUTPUT);

  allRed();

  SPI.begin(18, 19, 23, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Failed!");
    while (1);
  }
  Serial.println("Receiver Ready");
}

void loop() {
  // -------- Check for LoRa Packet --------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      received += c;
    }
    received.trim();

    Serial.print("Received: ");
    Serial.println(received);

    // Expected format: "N02,LAT=9.48,LON=77.51,DIR=N"
    // Extract targetUnit (before first comma)
    int commaIdx = received.indexOf(',');
    if (commaIdx != -1) {
      String target = received.substring(0, commaIdx);
      target.trim();

      // Check if this packet is for us
      if (target == unitID) {
        // Find DIR= field
        int dirIdx = received.indexOf("DIR=");
        if (dirIdx != -1) {
          int start = dirIdx + 4;
          int end = received.indexOf(',', start);
          if (end == -1) end = received.length();
          String dir = received.substring(start, end);
          dir.trim();

          if (dir.length() == 1 && (dir[0] == 'N' || dir[0] == 'S' || dir[0] == 'E' || dir[0] == 'W')) {
            lastDirection = dir;
            currentMode = PRIORITY;
            lastReceiveTime = millis();
            Serial.print("Priority activated for direction: ");
            Serial.println(lastDirection);
          } else {
            Serial.println("Invalid direction");
          }
        } else {
          Serial.println("No DIR field found");
        }
      } else {
        Serial.println("Ignored (not my ID)");
      }
    } else {
      Serial.println("Invalid packet format");
    }
  }

  // -------- Timeout → Back to NORMAL --------
  if (currentMode == PRIORITY && millis() - lastReceiveTime > overrideTimeout) {
    Serial.println("Returning to NORMAL");
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
    currentSignal = (currentSignal + 1) % 4;
  }
  allRed();
  setGreen(currentSignal);
}

// ================= PRIORITY MODE =================
void priorityMode() {
  allRed();
  // Map direction to signal index:
  // N -> 0, S -> 1, E -> 2, W -> 3
  if (lastDirection == "N") setGreen(0);
  else if (lastDirection == "S") setGreen(1);
  else if (lastDirection == "E") setGreen(2);
  else if (lastDirection == "W") setGreen(3);
}

// ================= HELPER FUNCTIONS =================
void allRed() {
  digitalWrite(N_RED, HIGH); digitalWrite(N_GRN, LOW);
  digitalWrite(S_RED, HIGH); digitalWrite(S_GRN, LOW);
  digitalWrite(E_RED, HIGH); digitalWrite(E_GRN, LOW);
  digitalWrite(W_RED, HIGH); digitalWrite(W_GRN, LOW);
}

void setGreen(int dir) {
  if (dir == 0) { digitalWrite(N_RED, LOW); digitalWrite(N_GRN, HIGH); }
  if (dir == 1) { digitalWrite(S_RED, LOW); digitalWrite(S_GRN, HIGH); }
  if (dir == 2) { digitalWrite(E_RED, LOW); digitalWrite(E_GRN, HIGH); }
  if (dir == 3) { digitalWrite(W_RED, LOW); digitalWrite(W_GRN, HIGH); }
}