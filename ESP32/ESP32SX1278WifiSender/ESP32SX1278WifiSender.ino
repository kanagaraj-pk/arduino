#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>

// Define the pins used by the LoRa module
#define NSS_PIN    5
#define RST_PIN    14
#define DIO0_PIN   2

int packetCounter = 0;
char serverAddress[] = "98.70.127.247";
int port = 8000;

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  bool wifiConnected = connectWiFi();
  initLoRa(); 

  
}

void initLoRa()
{
    Serial.println("LoRa Transmitter Setup...");

    // Override the default SPI pins for the LoRa library
    LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);

    // Initialize SX1278 at 433 MHz (change to 868E6 or 915E6 if using different module)
    if (!LoRa.begin(433E6)) {
      Serial.println("Starting LoRa failed! Check your wiring.");
      while (1); // Halt execution
    }
     LoRa.receive();   // Start in RX mode
    Serial.println("LoRa SX1278 Initialized Successfully!");
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("F14", "9448894884");

  Serial.print("Connecting to WiFi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 10000) {  // 10 seconds timeout
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi not available. Continuing without WiFi.");
    return false;
  }
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

  sendToServer(String(packetCounter));
  packetCounter++;
  
  delay(10000); 

}


void sendToServer(String msg) {
    if (WiFi.status() == WL_CONNECTED) 
        {
          msg.replace(" ", "%20");
          msg.replace("#", "%23");
          msg.replace("&", "%26");

          String path = "/send/" + msg;

          Serial.print("Sending: ");
          Serial.println(path);

          client.get(path);

          int statusCode = client.responseStatusCode();

          String response = client.responseBody();

          Serial.print("Status: ");
          Serial.println(statusCode);

          Serial.print("Response: ");
          Serial.println(response);
        }
}
