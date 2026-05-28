#include <Servo.h> // Include the Servo library
#include <DS3231.h>


Servo myservo;  // Create servo object to control a servo

DS3231 myRTC;
bool century = false;
bool h12Flag;
bool pmFlag;

void setup() {
  Serial.begin(57600);
  myservo.attach(5);  // Attaches the servo on pin 9 to the servo object
  myservo.write(0);    // Tell servo to go to 0 degrees
}

void loop() {
 myservo.write(65); 
 delay(10000); 
  myservo.write(0); 
 delay(10000); 
}
