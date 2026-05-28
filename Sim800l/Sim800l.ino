#include "SoftwareSerial.h"

SoftwareSerial sim800l(2, 3);

String cmd = "";

void setup()
{
  sim800l.begin(9600);
  Serial.begin(9600);
  Serial.println("Initializing...");
  delay(1000);

  sim800l.println("AT");                 // Sends an ATTENTION command, reply should be OK
  updateSerial();
  sim800l.println("AT+CMGF=1");          // Configuration for sending SMS
  updateSerial();
  sim800l.println("AT+CNMI=1,2,0,0,0");  // Configuration for receiving SMS
  updateSerial();
}

void loop()
{
  updateSerial();
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {

    cmd+=(char)Serial.read();
 
    if(cmd!=""){
      cmd.trim();  // Remove added LF in transmit
      if (cmd.equals("S")) {
        sendSMS();
      } else {
        sim800l.print(cmd);
        sim800l.println("");
      }
    }
  }
  
  while(sim800l.available()) 
  {
    Serial.write(sim800l.read());//Forward what Software Serial received to Serial Port
  }
}

void sendSMS(){
  sim800l.println("AT+CMGF=1");
  delay(500);
  sim800l.println("AT+CMGS=\"+919902580905\"");
  delay(500);
  sim800l.print("Hi! Success!");
  delay(500);
  sim800l.write((char)26);
}