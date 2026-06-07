#include <SPI.h>
#include <LoRa.h>

// Define the pins used by the LoRa module
#define NSS_PIN    10
#define RST_PIN    9
#define DIO0_PIN   2

#define RELAY_PIN 5

bool motorRunning = false;
unsigned long motorStartMillis = 0;
unsigned long runMinutes = 0;
unsigned long lastStatusMinute = 0;


int packetCounter = 0;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Transmitter Setup...");

  // Override the default SPI pins for the LoRa library
  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  
  // Initialize SX1278 at 433 MHz (change to 868E6 or 915E6 if using different module)
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed! Check your wiring.");
    while (1); // Halt execution
  }
  // Apply settings after successful initialization
  LoRa.setSpreadingFactor(12);     // Highest sensitivity
  LoRa.setSignalBandwidth(125E3);  // 125 kHz
  LoRa.setCodingRate4(8);          // Maximum error correction
  LoRa.enableCrc();
  LoRa.setTxPower(20);
  LoRa.receive(); 
  Serial.println("LoRa SX1278 Initialized Successfully!");
}

void loop() {

  // Check for incoming LoRa packet
  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String receivedMsg = "";

    while (LoRa.available()) {
      receivedMsg += (char)LoRa.read();
    }

    receivedMsg.trim();

    Serial.print("Received: ");
    Serial.println(receivedMsg);

    // START command
    if (receivedMsg.startsWith("START:"))  {

      runMinutes = receivedMsg.substring(6).toInt();

      if (runMinutes > 0) {

        digitalWrite(RELAY_PIN, HIGH);

        motorRunning = true;
        motorStartMillis = millis();
        lastStatusMinute = 0;

        Serial.println("Motor Started");

        LoRa.beginPacket();
        LoRa.print("RUNNING:");
        LoRa.print(runMinutes);
        LoRa.endPacket();
      }
    }

    // STOP command
    else if (receivedMsg == "STOP") {

      digitalWrite(RELAY_PIN, LOW);

      motorRunning = false;

      Serial.println("Motor Stopped");

      LoRa.beginPacket();
      LoRa.print("STOPPED");
      LoRa.endPacket();
    }
  }

  // Auto stop timer
  if (motorRunning) {

  unsigned long elapsedMinutes =
      (millis() - motorStartMillis) / 60000UL;

  // Send status every minute
  if (elapsedMinutes > lastStatusMinute) {

    lastStatusMinute = elapsedMinutes;

    unsigned long remainingMinutes =
        (runMinutes > elapsedMinutes)
        ? (runMinutes - elapsedMinutes)
        : 0;

    LoRa.beginPacket();
    LoRa.print("RUNNING:");
    LoRa.print(remainingMinutes);
    LoRa.endPacket();

    Serial.print("Status Sent: RUNNING:");
    Serial.println(remainingMinutes);
  }

  // Stop when timer expires
  if (elapsedMinutes >= runMinutes) {

    digitalWrite(RELAY_PIN, LOW);
    motorRunning = false;

    LoRa.beginPacket();
    LoRa.print("COMPLETED");
    LoRa.endPacket();

    Serial.println("COMPLETED");
  }
}
