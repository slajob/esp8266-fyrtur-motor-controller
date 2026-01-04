#include <SoftwareSerial.h>

SoftwareSerial zigbee(4, 5); // RX = D2, TX = D1

// Simulation State
uint8_t currentPos = 0;    
uint8_t targetPos = 0;
uint8_t motorStatus = 0xEA; 
unsigned long lastUpdate = 0;

uint8_t packetBuffer[16];
uint8_t bufIdx = 0;
uint8_t lastByte = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  zigbee.begin(2400); 
  Serial.println("\n--- IKEA Fyrtur Motor Simulator (v3 - Robust) ---");
}

void loop() {
  while (zigbee.available()) {
    uint8_t r = zigbee.read();
    
    // Header Sync: Reset on 00 FF
    if (lastByte == 0x00 && r == 0xFF) {
      bufIdx = 0;
      packetBuffer[bufIdx++] = 0x00;
      packetBuffer[bufIdx++] = 0xFF;
    } else if (bufIdx > 0 && bufIdx < 16) {
      packetBuffer[bufIdx++] = r;
    }
    lastByte = r;

    // Process when we have a likely packet (at least 6 bytes)
    if (bufIdx >= 6) {
      bool processed = false;
      
      // Look for Command Byte in the packet
      for (int i = 2; i < bufIdx; i++) {
        // HEARTBEAT (CC)
        if (packetBuffer[i] == 0xCC) {
          Serial.println(" -> [POLL]");
          sendMotorStatus();
          bufIdx = 0; processed = true; break;
        }
        // MOVE COMMAND (DD)
        if (packetBuffer[i] == 0xDD && i + 1 < bufIdx) {
          targetPos = packetBuffer[i+1];
          Serial.print(" -> [GOTO "); Serial.print(targetPos); Serial.println("%]");
          sendMotorStatus();
          bufIdx = 0; processed = true; break;
        }
      }
      // Safety: If buffer gets too long without a command, clear it
      if (!processed && bufIdx >= 12) bufIdx = 0;
    }
  }

  // SIMULATION: Movement & "Overcurrent" Stop
  if (millis() - lastUpdate > 600) { // Moving speed
    if (currentPos != targetPos) {
      motorStatus = 0xD8; // Moving status
      if (currentPos < targetPos) currentPos++;
      else currentPos--;
      
      Serial.print("Moving... Current: "); Serial.print(currentPos); Serial.println("%");
    } else {
      // We reached the target (or the top 0% limit)
      motorStatus = 0xEA; // Stopped status (mimics overcurrent stop)
    }
    lastUpdate = millis();
  }
}

void sendMotorStatus() {
  uint8_t res[8];
  res[0] = 0x00;
  res[1] = 0xFF;
  res[2] = 0xD8;       
  res[3] = 0x49;       
  res[4] = motorStatus;
  res[5] = currentPos;
  res[6] = targetPos;
  
  // Checksum: Sum of ID through Target Position
  uint16_t sum = 0;
  for (int i = 2; i <= 6; i++) sum += res[i];
  res[7] = (uint8_t)(sum & 0xFF); 

  zigbee.write(res, 8);
}