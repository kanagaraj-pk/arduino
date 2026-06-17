#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include <HTTPClient.h>



char ssid[] = "Vi";
char password[] = "9448894884";

String apiUrl = "https://api.tekfocusminds.com/motor";
//String apiUrl = "https://xtech.cx/motor";
String scheduleurl = apiUrl + "/getschedule";
String sendToServerurl = apiUrl + "/send/";
String fetchIntervalurl = apiUrl + "/fetchInterval";

unsigned long lastFetch = 0;
unsigned long fetchInterval = 600000; // 1 minute

/* LoRa Pins */

#define NSS_PIN   5
#define RST_PIN   14
#define DIO0_PIN  2


void setup() {

  Serial.begin(9600);

  Serial.println("Starting");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting...");
  }


  Serial.println("Connected");

  IPAddress ip = WiFi.localIP();

  Serial.print("IP: ");

  Serial.println(ip);
  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    while (1);
  }
  LoRa.setSpreadingFactor(12);     // Highest sensitivity
  LoRa.setSignalBandwidth(125E3);  // 125 kHz
  LoRa.setCodingRate4(8);          // Maximum error correction
  LoRa.enableCrc();
  LoRa.setTxPower(20);
  LoRa.receive();   // Start in RX mode
  Serial.println("LoRa Ready");

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(fetchIntervalurl);
  Serial.println("HTTP begin fetchInterval");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    unsigned long value = payload.toInt();

    if (value > 0) {
      fetchInterval = value;
    }
    Serial.print("fetchInterval = ");
    Serial.println(fetchInterval);
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpCode);
  }

  http.end();

}


void loop() {
  //static unsigned long lastDebug = 0;

  //if (millis() - lastDebug > 1000) {
     // lastDebug = millis();
     // Serial.println("Loop running");
  //}

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
 
 
  if (WiFi.status() == WL_CONNECTED &&
      (lastFetch == 0 || millis() - lastFetch >= fetchInterval)) {
      Serial.println("Fetching schedule...");

      LoRa.idle();      // Leave RX mode
      lastFetch = millis();

      HTTPClient http;
      
      http.setTimeout(5000);

      http.begin(scheduleurl);
      Serial.println("HTTP begin done");
      int httpCode = http.GET();
      Serial.print("HTTP code=");
      Serial.println(httpCode);


      if (httpCode <= 0) {
          Serial.println("API connection failed");
          http.end();
          return; // send nothing
      }
      if (httpCode == HTTP_CODE_OK) {

        String schedule = http.getString();

        // Example received:
        // T15:42|14:00,20

        Serial.println("API: " + schedule);

        LoRa.beginPacket();
        LoRa.print(schedule);
        LoRa.endPacket();
        LoRa.receive(); 
        Serial.println("LoRa Sent");
      }
      http.end();
      LoRa.receive();
    }
    else
    {
          if(WiFi.status() != WL_CONNECTED) {
          Serial.println("WIFI Connecting...");
          WiFi.begin(ssid, password);
          delay(10000);
          
        }
    }  
}



void sendToServer(String msg) {

  /* Basic URL Encoding */

  msg.replace(" ", "%20");
  msg.replace("#", "%23");
  msg.replace("&", "%26");

  String path = sendToServerurl + msg;

  Serial.print("Sending: ");
  Serial.println(path);

   HTTPClient http;
    
  http.setTimeout(5000);

  http.begin(path);

  int statusCode = http.GET();

  Serial.print("Status: ");
  Serial.println(statusCode);

}
