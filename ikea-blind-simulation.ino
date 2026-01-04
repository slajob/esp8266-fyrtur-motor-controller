#include <SoftwareSerial.h>

// RX = D2 (GPIO4), TX = D1 (GPIO5)
SoftwareSerial zigbee(4, 5); 

// Simulation Variables
uint8_t currentPos = 0;    
uint8_t targetPos = 0;
uint8_t motorStatus = 0xEA; // 0xEA = Idle/Stopped
unsigned long lastUpdate = 0;

// Buffer Logic
uint8_t packetBuffer[12];
uint8_t bufIdx = 0;
uint8_t lastByte = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  zigbee.begin(2400); 
  Serial.println("\n--- IKEA Fyrtur Motor Simulator (v2) ---");
  Serial.println("Waiting for Zigbee Sync (00 FF)...");
}

void loop() {
  while (zigbee.available()) {
    uint8_t r = zigbee.read();
    
    // DEBUG: Print raw bytes in a single line for easier reading
    Serial.print(r < 0x10 ? "0" : "");
    Serial.print(r, HEX);
    Serial.print(" ");

    // HEADER SYNC: If we see 00 followed by FF, it's a new packet
    if (lastByte == 0x00 && r == 0xFF) {
      packetBuffer[0] = 0x00;
      packetBuffer[1] = 0xFF;
      bufIdx = 2;
    } else if (bufIdx > 0 && bufIdx < 12) {
      packetBuffer[bufIdx++] = r;
    }
    
    lastByte = r;

    // PROCESS PACKETS
    if (bufIdx >= 6) {
      // 1. HEARTBEAT/POLL (00 FF 9A CC CC 00)
      if (packetBuffer[2] == 0x9A && packetBuffer[3] == 0xCC) {
        Serial.println(" -> [POLL]");
        sendMotorStatus();
        bufIdx = 0; // Clear buffer after processing
      } 
      // 2. MOVE COMMAND (00 FF 9A DD [POS] [CS])
      // Note: Move commands are usually 6 or 7 bytes
      else if (bufIdx >= 6 && packetBuffer[2] == 0x9A && packetBuffer[3] == 0xDD) {
        targetPos = packetBuffer[4];
        Serial.print(" -> [GOTO ");
        Serial.print(targetPos);
        Serial.println("%]");
        sendMotorStatus();
        bufIdx = 0;
      }
    }
  }

  // SIMULATE MOVEMENT (1% every 800ms)
  if (millis() - lastUpdate > 800) {
    if (currentPos != targetPos) {
      if (currentPos < targetPos) currentPos++;
      else currentPos--;
      motorStatus = 0xD8; // Status: Moving
    } else {
      motorStatus = 0xEA; // Status: Stopped
    }
    lastUpdate = millis();
  }
}

void sendMotorStatus() {
  // We use 8 bytes as seen in your logs
  uint8_t res[8];
  res[0] = 0x00;
  res[1] = 0xFF;
  res[2] = 0xD8;       // Motor ID
  res[3] = 0x49;       // Status Command
  res[4] = motorStatus;
  res[5] = currentPos;
  res[6] = targetPos;
  
  // Custom IKEA Checksum calculation (Sum of payload)
  uint16_t sum = 0;
  for (int i = 2; i <= 6; i++) sum += res[i];
  
  // Based on your logs, the checksum is likely (0x12E - sum) or similar.
  // We will use a calculated one, but if it fails, we'll try the 'Inverse Sum'.
  res[7] = (uint8_t)(sum & 0xFF); 

  // Send to Zigbee board
  zigbee.write(res, 8);
}