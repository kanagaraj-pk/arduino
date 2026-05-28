#include <DS3231.h>
#include <Wire.h>


DS3231 myRTC;
bool century = false;
bool h12Flag;
bool pmFlag;



#define RELAY_PIN 7


const int START_HOUR   = 13;   
const int START_MINUTE = 40;
const int RUN_MINUTES  = 20;

bool motorRunning = false;
bool motorDoneToday = false;
unsigned long motorStartMillis = 0;


void setup() {
	Wire.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   

	Serial.begin(9600);

}

void loop() {
  if (myRTC.getHour(h12Flag, pmFlag) == 0 && myRTC.getMinute() == 0) {
    motorDoneToday = false;
  }


  if (!motorRunning && !motorDoneToday &&
      myRTC.getHour(h12Flag, pmFlag) == START_HOUR &&
      myRTC.getMinute() == START_MINUTE) {

    motorRunning = true;
    motorStartMillis = millis();
    digitalWrite(RELAY_PIN, HIGH);  
  }


  if (motorRunning &&
      millis() - motorStartMillis >= (unsigned long)RUN_MINUTES * 60UL * 1000UL) {

    motorRunning = false;
    motorDoneToday = true;
    digitalWrite(RELAY_PIN, LOW);  
  }

  Serial.print("Running:DoneToday  ");
  Serial.print(motorRunning);  
  Serial.print(":");
  Serial.print(motorDoneToday);  
  Serial.print("  ");
  Serial.println();


  Serial.print("Current Time: ");
	Serial.print(myRTC.getHour(h12Flag, pmFlag), DEC);
	Serial.print(":");
	Serial.print(myRTC.getMinute(), DEC);
	Serial.print(":");
	Serial.print(myRTC.getSecond(), DEC);
  Serial.println();


  Serial.print("Start Time: ");
  Serial.print(START_HOUR);  
  Serial.print(":");  
  Serial.println(START_MINUTE);  
  Serial.print("Run Minutes :");  
  Serial.print(RUN_MINUTES);  
  Serial.println();


	Serial.print("T=");
	Serial.print(myRTC.getTemperature(), 2);
  Serial.print("  ");
  Serial.println();

	delay(10000);
}

