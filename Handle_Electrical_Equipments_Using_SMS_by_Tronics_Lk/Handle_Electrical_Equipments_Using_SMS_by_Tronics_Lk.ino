/* SUBSCRIBE 'Tronics Lk' YouTube Channel to get latest Electronics and Programming Tutorials

   Wiring Guide
   ============
   GSM Module >
              Tx Pin >  8 Pin
              Rx Pin >  9 Pin
              Gnd Pin > Gnd Pin
              Vcc Pin > 3.7V - 4.2V (Power supply should be 2A)


   Device 1 (LED 1) > 13
   Device 1 (LED 2) > 12

*/

#include <SoftwareSerial.h>

//GSM Module TX is connected to Arduino D8
#define SIM800_TX_PIN 8

//GSM Module RX is connected to Arduino D9
#define SIM800_RX_PIN 9

SoftwareSerial mySerial(SIM800_TX_PIN, SIM800_RX_PIN); //Create software serial object to communicate with GSM Module

int device_1 = 13; // attach pin D13 of Arduino to Device-1
int device_2 = 12; // attach pin D13 of Arduino to Device-2

// defines variables
int index = 0;
String number = "";
String message = "";

char incomingByte;
String incomingData;
bool atCommand = true;

void setup()
{
  Serial.begin(9600); // Serial Communication for Serial Monitor is starting with 9600 of baudrate speed
  mySerial.begin(9600); // Serial Communication for GSM Module is starting with 9600 of baudrate speed

  pinMode(device_1, OUTPUT); //Sets the device_1 as an OUTPUT
  pinMode(device_2, OUTPUT); //Sets the device_2 as an OUTPUT

  digitalWrite(device_1, LOW); //Sets the device_1 in to OFF state at the beginning
  digitalWrite(device_2, LOW); //Sets the device_2 in to OFF state at the beginning


  // Check if you're currently connected to SIM800L
  while (!mySerial.available()) {
    mySerial.println("AT");
    delay(1000);
    Serial.println("connecting....");
  }

  Serial.println("Connected..");

  mySerial.println("AT+CMGF=1");  //Set SMS Text Mode
  delay(1000);
  mySerial.println("AT+CNMI=1,2,0,0,0");  //procedure, how to receive messages from the network
  delay(1000);
  mySerial.println("AT+CMGL=\"REC UNREAD\""); // Read unread messages

  Serial.println("Ready to received Commands..");
}

void loop()
{
  if (mySerial.available()) {
    delay(100);

    // Serial buffer
    while (mySerial.available()) {
      incomingByte = mySerial.read();
      incomingData += incomingByte;
    }

    delay(10);
    if (atCommand == false) {
      receivedMessage(incomingData);
    } else {
      atCommand = false;
    }

    //delete messages to save memory
    if (incomingData.indexOf("OK") == -1) {
      mySerial.println("AT+CMGDA=\"DEL ALL\"");
      delay(1000);
      atCommand = true;
    }

    incomingData = "";
  }
}

void receivedMessage(String inputString) {

  //Get The number of the sender
  index = inputString.indexOf('"') + 1;
  inputString = inputString.substring(index);
  index = inputString.indexOf('"');
  number = inputString.substring(0, index);
  Serial.println("Number: " + number);

  //Get The Message of the sender
  index = inputString.indexOf("\n") + 1;
  message = inputString.substring(index);
  message.trim();
  Serial.println("Message: " + message);
  message.toUpperCase(); // uppercase the message received


  //turn Device 1 ON
  if (message.indexOf("D1 ON") > -1) {
    digitalWrite(device_1, HIGH);
    delay(1000);
    Serial.println("Command: Device 1 Turn On.");
  }

  //turn Device 1 OFF
  if (message.indexOf("D1 OFF") > -1) {
    digitalWrite(device_1, LOW);
    Serial.println("Command: Device 1 Turn Off.");
  }

  //turn Device 2 ON
  if (message.indexOf("D2 ON") > -1) {
    digitalWrite(device_2, HIGH);
    delay(1000);
    Serial.println("Command: Device 2 Turn On.");
  }

  //turn Device 2 OFF
  if (message.indexOf("D2 OFF") > -1) {
    digitalWrite(device_2, LOW);
    Serial.println("Command: Device 1 Turn Off.");
  }

  delay(50);// Added delay between two readings
}
