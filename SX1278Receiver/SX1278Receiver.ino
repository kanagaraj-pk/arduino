#include <WiFiS3.h>
#include <SPI.h>
#include <LoRa.h>


#include <ArduinoHttpClient.h>

char ssid[] = "Vi";
char password[] = "9448894884";
char serverAddress[] = "98.70.127.247";
int port = 8000;

/* LoRa Pins */

#define SS    10
#define RST   9
#define DIO0  2

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);

void setup() {

  Serial.begin(115200);

  Serial.println("Starting");

  int status = WL_IDLE_STATUS;

  while (status != WL_CONNECTED) {

    Serial.println("Connecting...");

    status = WiFi.begin(ssid, password);

    delay(5000);
  }

  Serial.println("Connected");

  IPAddress ip = WiFi.localIP();

  Serial.print("IP: ");

  Serial.println(ip);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {

    Serial.println("LoRa init failed");

    while (1);
  }

  Serial.println("LoRa Ready");
}


void loop() {

  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String msg = "";

    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }

    long rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();
    long ferr = LoRa.packetFrequencyError();

    String finalMsg =
        "MSG=" + msg +
        " | RSSI=" + String(rssi) +
        " dBm | SNR=" + String(snr) +
        " | FERR=" + String(ferr);

    Serial.println(finalMsg);

    sendToServer(finalMsg);
  }
}

void sendToServer(String msg) {

  /* Basic URL Encoding */

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