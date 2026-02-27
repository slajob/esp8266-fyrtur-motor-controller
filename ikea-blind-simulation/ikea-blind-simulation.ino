#include <SoftwareSerial.h>

SoftwareSerial zigbee(4, 5); // RX, TX

// ---------------- DRV8833 PINS ----------------
#define AIN1   12   // D6
#define AIN2   13   // D7
#define SLP    14   // D5

// ---------------- PROTOCOL SETTINGS ----------------
#define CMD_HEADER_0 0x00
#define CMD_HEADER_1 0xFF
#define CMD_HEADER_2 0x9A
#define RES_HEADER_2 0xD8

uint8_t packetBuffer[6];
uint8_t bufIdx = 0;
bool isMoving = false;

void setup() {
  Serial.begin(115200);
  zigbee.begin(2400);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(SLP, OUTPUT);

  stopMotor(); // Ensure motor is off and driver is sleeping
  Serial.println("--- IKEA Manual Motor Controller (DRV8833) ---");
  Serial.println("Mode: Manual Only | Position: Locked at 50%");
}

void loop() {
  // 1. PROTOCOL PARSING
  while (zigbee.available()) {
    uint8_t r = zigbee.read();

    if (bufIdx == 0 && r == CMD_HEADER_0) {
      packetBuffer[bufIdx++] = r;
    } else if (bufIdx == 1 && r == CMD_HEADER_1) {
      packetBuffer[bufIdx++] = r;
    } else if (bufIdx == 2 && r == CMD_HEADER_2) {
      packetBuffer[bufIdx++] = r;
    } else if (bufIdx >= 3 && bufIdx < 6) {
      packetBuffer[bufIdx++] = r;
      if (bufIdx == 6) {
        processCommand();
        bufIdx = 0; 
      }
    } else {
      bufIdx = 0; 
    }
  }
}

// ---------------- MOTOR HARDWARE CONTROL ----------------
void moveUp() {
  digitalWrite(SLP, HIGH);  // Wake driver
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  isMoving = true;
  Serial.println("Motor: SPINNING UP");
}

void moveDown() {
  digitalWrite(SLP, HIGH);  // Wake driver
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  isMoving = true;
  Serial.println("Motor: SPINNING DOWN");
}

void stopMotor() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(SLP, LOW);   // Sleep driver to save power
  isMoving = false;
  Serial.println("Motor: STOPPED");
}

// ---------------- COMMAND LOGIC ----------------
void processCommand() {
  uint8_t data1 = packetBuffer[3];
  uint8_t data2 = packetBuffer[4];
  uint8_t checksum = packetBuffer[5];

  // Verify XOR Checksum
  if ((data1 ^ data2) != checksum) {
    Serial.println("Err: Checksum Mismatch");
    return;
  }

  // Handle Manual Movement (0x0A Group)
  if (data1 == 0x0A) {
    if (data2 == 0xDD) {      // CMD_UP
      moveUp();
    } 
    else if (data2 == 0xEE) { // CMD_DOWN
      moveDown();
    } 
    else if (data2 == 0xCC) { // CMD_STOP
      stopMotor();
    }
    sendMotorStatus();
  }
  // Ignore Direct Position Commands (0xDD)
  else if (data1 == 0xDD) {
    Serial.println("CMD: GoTo Pos ignored (Manual Mode)");
    sendMotorStatus();
  }
  // Handle Status Polls
  else if (data1 == 0xCC && data2 == 0xCC) {
    sendMotorStatus();
  }
}

// ---------------- ZIGBEE RESPONSE ----------------
void sendMotorStatus() {
  uint8_t res[8];
  
  uint8_t battery = 0x12; // Static 7.0V
  uint8_t voltage = 0xD8; 
  uint8_t speed   = isMoving ? 0x19 : 0x00; 
  uint8_t pos     = 0x32; // ALWAYS report 50% (32 hex) to disable limits

  res[0] = 0x00;
  res[1] = 0xFF;
  res[2] = RES_HEADER_2;
  res[3] = battery;
  res[4] = voltage;
  res[5] = speed;
  res[6] = pos;
  res[7] = battery ^ voltage ^ speed ^ pos; // XOR Checksum

  zigbee.write(res, 8);
}