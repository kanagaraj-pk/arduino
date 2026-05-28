#include <SPI.h>
#include <LoRa.h>

// Define the pins used by the LoRa module
#define NSS_PIN    5
#define RST_PIN    14
#define DIO0_PIN   2

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
  
  Serial.println("LoRa SX1278 Initialized Successfully!");
}

void loop() {
  Serial.print("Sending packet: ");
  Serial.println(packetCounter);

  // Begin transmission packet
  LoRa.beginPacket();
  
  // Print payload data to the packet
  LoRa.print("Hello ESP32 LoRa! Count: ");
  LoRa.print(packetCounter);
  
  // End packet and transmit it over the air
  LoRa.endPacket();

  packetCounter++;
  delay(10000); // Wait 2 seconds before sending next packet
}
