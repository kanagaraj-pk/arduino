#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>



// Define the pins used by the LoRa module
#define ServoPIN_Gate1 3
#define ServoPIN_Gate2 5
#define ServoPIN_Gate3 6
#define LORA_SS_PIN 10
#define LORA_RST_PIN 9
#define LORA_DIO0_PIN 2

Servo servo1;
Servo servo2;
Servo servo3;

bool loraReady = false;
unsigned long lastLoraRetry = 0;
const int CLOSED_ANGLE = 90;
const int OPEN_ANGLE = 0;
const int ServoDelay = 3000;

void setup() {
  Serial.begin(9600);
  Serial.println("servo attach.");
  servo1.attach(ServoPIN_Gate1);
  servo2.attach(ServoPIN_Gate2);
  servo3.attach(ServoPIN_Gate3);

  //closeAllGates();
  Serial.println("LoRa Transmitter Setup...");
  loraReady = initLoRa();
  Serial.println("LoRa ready. Waiting for gate commands...");

}

void loop() {
   if (!loraReady) {
        if (millis() - lastLoraRetry > 10000UL) {   // retry every 10 seconds
            lastLoraRetry = millis();
            Serial.println("Retrying LoRa...");
           loraReady = initLoRa();
        }
        return;   // don't use LoRa until initialized
     }
    
    
    int packetSize = LoRa.parsePacket();
    if (packetSize) { 


    String incomingMsg = "";   
    while (LoRa.available()) {
      incomingMsg += (char)LoRa.read();
    }

    incomingMsg.trim();

    Serial.print("Received: ");
    Serial.println(incomingMsg);
      if (incomingMsg.indexOf("Gate:1") >= 0) {
          openGate(1);
      }
      else if (incomingMsg.indexOf("Gate:2") >= 0) {
          openGate(2);
      }
      else if (incomingMsg.indexOf("Gate:3") >= 0) {
          openGate(3);
      }
    }

}


void closeAllGates() {
  Serial.println("Closing All Gates.");
  servo1.write(CLOSED_ANGLE);
  delay(5000);
  servo2.write(CLOSED_ANGLE);
  delay(5000);
  servo3.write(CLOSED_ANGLE);
  delay(5000);
  Serial.println("All Gates Closed.");
}

bool initLoRa() {

    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    //Sometimes the radio itself gets stuck during a brownout. Before calling LoRa.begin(), reset it:
    pinMode(LORA_RST_PIN, OUTPUT);
    digitalWrite(LORA_RST_PIN, LOW);
    delay(20);
    digitalWrite(LORA_RST_PIN, HIGH);
    delay(20);

    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa init failed");
        return false;
    }

    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(8);
    LoRa.enableCrc();
    LoRa.setTxPower(20);
    LoRa.receive();
    //Serial.println("LoRa Ready");
    return true; 
}

void openGate(int gate) {
  // Close all gates first
 

  // Open only the selected gate
  switch (gate) {
    case 1:
      servo1.write(OPEN_ANGLE);
      delay(ServoDelay);
      servo2.write(CLOSED_ANGLE);
      delay(ServoDelay);
      servo3.write(CLOSED_ANGLE);
      delay(ServoDelay);
      break;

    case 2:
      servo2.write(OPEN_ANGLE);
      delay(ServoDelay);
      servo1.write(CLOSED_ANGLE);
      delay(ServoDelay);
      servo3.write(CLOSED_ANGLE);
      delay(ServoDelay);
      break;

    case 3:
      servo3.write(OPEN_ANGLE);
      delay(ServoDelay);
      servo1.write(CLOSED_ANGLE);
      delay(ServoDelay);
      servo2.write(CLOSED_ANGLE);
      delay(ServoDelay);
      break;

    default:
      Serial.println("Invalid gate number");
      return;
  }

  Serial.print("Gate ");
  Serial.print(gate);
  Serial.println(" opened");
}


