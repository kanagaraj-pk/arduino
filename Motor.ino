#define RELAY_PIN 7   // Relay control pin

void setup() {
  pinMode(RELAY_PIN, OUTPUT);

  // Turn relay OFF initially
  digitalWrite(RELAY_PIN, HIGH); // change to LOW if your relay is active-HIGH
}

void loop() {
  // Motor ON
  digitalWrite(RELAY_PIN, LOW);   // relay ON (most modules are active LOW)
  delay(600000);                    // motor runs for 5 seconds

  // Motor OFF
  digitalWrite(RELAY_PIN, HIGH);  // relay OFF
  delay(600000);                    // motor stops for 5 seconds
}