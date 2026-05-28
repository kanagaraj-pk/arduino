#include <SPI.h>
#include <LoRa.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Transmitter");

  // NSS, RESET, DIO0 (optional if wired)
  LoRa.setPins(10, 9, 2);

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("LoRa init success");
}

void loop() {
  Serial.println("Sending: Hello");

  LoRa.beginPacket();
  LoRa.print("Hello");
  LoRa.endPacket();

  delay(3000);   // send every 1 second
}
