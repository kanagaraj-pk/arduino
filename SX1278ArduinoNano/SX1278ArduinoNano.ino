#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>


// Define the pins used by the LoRa module
#define MOTOR_PIN 4
#define LORA_SS_PIN 10
#define LORA_RST_PIN 9
#define LORA_DIO0_PIN 2

#define EEPROM_START_ADDR 0
#define MAX_SCHEDULES 10

struct Schedule {
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t runMinutes;
};

// Global variables to manage active operational states
Schedule activeSchedules[MAX_SCHEDULES];
uint8_t totalSchedules = 0;
bool motorIsRunning = false;
unsigned long motorEndTime = 0;

// Track parsed execution time
uint8_t currentHour = 0;
uint8_t currentMinute = 0;
unsigned long lastMinuteUpdateTick = 0;

String signal = "";

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW); // Motor 

  Serial.println("LoRa Transmitter Setup...");

  // Override the default SPI pins for the LoRa library
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

  // Initialize SX1278 at 433 MHz (change to 868E6 or 915E6 if using different module)
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed! Check your wiring.");
    while (1); // Halt execution
  }
  // Apply settings after successful initialization
  LoRa.setSpreadingFactor(12);     // Highest sensitivity
  LoRa.setSignalBandwidth(125E3);  // 125 kHz
  LoRa.setCodingRate4(8);          // Maximum error correction
  LoRa.enableCrc();
  LoRa.setTxPower(20);
  LoRa.receive(); 
  Serial.println("LoRa SX1278 Initialized Successfully!");


  // Load operating config from non-volatile memory
  loadScheduleFromEEPROM();

}

void loop() {

  // Check for incoming LoRa packet
  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String incomingMsg = "";

    while (LoRa.available()) {
      incomingMsg += (char)LoRa.read();
    }

    long rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();
    long ferr = LoRa.packetFrequencyError();

    signal = "RSSI=" + String(rssi) + "," + " SNR=" + String(snr) + "," + " FERR=" + String(ferr);

    incomingMsg.trim();
    Serial.print(signal);

    Serial.print("Received: ");
    Serial.println(incomingMsg);

    processIncomingMessage(incomingMsg);
  }
  // 2. Keep track of current system runtime clocks
  updateInternalClock();

  // 3. Monitor schedules and manage active relays
  manageMotorState();
}


void processIncomingMessage(String msg) {
      // 1. Clean up formatting issues
      msg.trim(); // Removes leading/trailing spaces, newlines (\n), and carriage returns (\r)
      
      // 2. Strip surrounding literal quotation marks if they exist
      if (msg.startsWith("\"") && msg.endsWith("\"")) {
        msg = msg.substring(1, msg.length() - 1);
        msg.trim(); // Trim again just in case there were spaces inside quotes
      }
      if (msg == "CLEAR") {
        Serial.println("Clear command triggered. Wiping memory arrays...");
        clearScheduleEEPROM();
        sendStatusToSender(); // Echo status back confirming wipe
        return;
      }

      if (msg == "STATUS") {
        Serial.println("Manual status request received.");
        sendStatusToSender(); // Query data structures and transmit back
        return;
      }

      // Parsing indices configuration
      int firstPipe = msg.indexOf('|');
      int secondPipe = msg.indexOf('|', firstPipe + 1);
      
       if (firstPipe == -1 || secondPipe == -1) {
        Serial.print("Invalid payload structural syntax. Raw msg: [");
        Serial.print(msg);
        Serial.println("]");
        return;
        }

      // Extract Temporal Variables (Dxxx and Txx:xx)
      String dayStr = msg.substring(0, firstPipe);
      String timeStr = msg.substring(firstPipe + 1, secondPipe);

      // Parse Live Current Clocks 
      int colonIndex = timeStr.indexOf(':');
      if (colonIndex != -1) {
        currentHour = timeStr.substring(1, colonIndex).toInt(); // Skip 'T' character
        currentMinute = timeStr.substring(colonIndex + 1).toInt();
        lastMinuteUpdateTick = millis();
        Serial.print("Internal Clock Synced to: ");
        printTime(currentHour, currentMinute);
      }

      // Temporary container to evaluate dynamic schedule shifts
        Schedule parsedSchedules[MAX_SCHEDULES];
        uint8_t parsedCount = 0;

        int currentPipeIndex = secondPipe;
        while (currentPipeIndex != -1 && parsedCount < MAX_SCHEDULES) {
          int nextPipeIndex = msg.indexOf('|', currentPipeIndex + 1);
          String scheduleToken = "";
          
          if (nextPipeIndex == -1) {
            scheduleToken = msg.substring(currentPipeIndex + 1);
          } else {
            scheduleToken = msg.substring(currentPipeIndex + 1, nextPipeIndex);
          }
          
          scheduleToken.trim();
          if (scheduleToken.length() > 0) {
            int commaIndex = scheduleToken.indexOf(',');
            if (commaIndex != -1) {
              int schedColon = scheduleToken.indexOf(':');
              if (schedColon != -1 && schedColon < commaIndex) {
                parsedSchedules[parsedCount].startHour = scheduleToken.substring(0, schedColon).toInt();
                parsedSchedules[parsedCount].startMinute = scheduleToken.substring(schedColon + 1, commaIndex).toInt();
                parsedSchedules[parsedCount].runMinutes = scheduleToken.substring(commaIndex + 1).toInt();
                parsedCount++;
              }
            }
          }
          currentPipeIndex = nextPipeIndex;
        }

        // Evaluate if changes happened to prevent structural burn
       if (hasScheduleChanged(parsedSchedules, parsedCount)) {
          Serial.println("New schedule configuration detected. Overwriting EEPROM...");
    
            totalSchedules = parsedCount;
            for (int i = 0; i < totalSchedules; i++) {
              activeSchedules[i] = parsedSchedules[i];
            }
          
            saveScheduleToEEPROM();
            sendStatusToSender(); // Transmit confirmation back with new data
        } else {
            // SAFETY RETREAT: Runs if data matches baseline exactly
            Serial.println("Schedules match existing baseline exactly. EEPROM flash storage bypassed.");
            
            // Still reply back to the sender so it knows the packet arrived safely
            sendStatusToSender(); 
        }
}


// Compare current configurations block-by-block
bool hasScheduleChanged(Schedule newSched[], uint8_t newCount) {
  if (newCount != totalSchedules) return true;
  for (int i = 0; i < newCount; i++) {
    if (newSched[i].startHour != activeSchedules[i].startHour ||
        newSched[i].startMinute != activeSchedules[i].startMinute ||
        newSched[i].runMinutes != activeSchedules[i].runMinutes) {
      return true;
    }
  }
  return false;
}


// Erase dynamic structural pointers
void clearScheduleEEPROM() {
   // SAFETY CHECK: If it's already cleared, do nothing to protect EEPROM
  if (totalSchedules == 0) {
    Serial.println("System is already clear. Skipping redundant EEPROM write.");
    return; 
  }

  totalSchedules = 0;
  EEPROM.update(EEPROM_START_ADDR, 0); // Override header count index back to 0
  
  if (motorIsRunning) {
    digitalWrite(MOTOR_PIN, LOW);
    motorIsRunning = false;
    Serial.println("Emergency Schedule Kill: Engine offline.");
  }
  Serial.println("Non-volatile schedule states cleared.");
}

// Commit payload arrays natively down to flash blocks
void saveScheduleToEEPROM() {
  int addr = EEPROM_START_ADDR;
  
  // Header Write: Save counts to know bounds on reboot
  EEPROM.update(addr, totalSchedules);
  addr += 1;

  // Multi-byte struct arrays streaming out
  for (int i = 0; i < totalSchedules; i++) {
    EEPROM.put(addr, activeSchedules[i]);
    addr += sizeof(Schedule);
  }
  Serial.println("EEPROM parameters successfully modified.");
}


void loadScheduleFromEEPROM() {
  int addr = EEPROM_START_ADDR;
  totalSchedules = EEPROM.read(addr);
  addr += 1;

  // Catch empty or unprogrammed memory segments
  if (totalSchedules == 255 || totalSchedules > MAX_SCHEDULES) {
    totalSchedules = 0;
    Serial.println("No valid configurations localized inside flash.");
    return;
  }

  for (int i = 0; i < totalSchedules; i++) {
    EEPROM.get(addr, activeSchedules[i]);
    addr += sizeof(Schedule);
  }
  
  Serial.print("Loaded "); 
  Serial.print(totalSchedules); 
  Serial.println(" active operational sequences from flash storage:");
  for(int i=0; i < totalSchedules; i++) {
    Serial.print(" -> ");
    printTime(activeSchedules[i].startHour, activeSchedules[i].startMinute);
    Serial.print(" for "); Serial.print(activeSchedules[i].runMinutes); Serial.println(" mins");
  }
}

void printTime(uint8_t h, uint8_t m) {
  if(h < 10) Serial.print("0"); Serial.print(h);
  Serial.print(":");
  if(m < 10) Serial.print("0"); Serial.println(m);
}

// Returns the index of the next upcoming schedule. Returns -1 if no schedules exist.
int getNextScheduleIndex() {
  if (totalSchedules == 0) return -1;

  int currentTotalMinutes = (currentHour * 60) + currentMinute;
  int nextIndex = 0;
  int minTimeDifference = 1440; // Max minutes in a day (24 * 60)

  for (int i = 0; i < totalSchedules; i++) {
    int schedTotalMinutes = (activeSchedules[i].startHour * 60) + activeSchedules[i].startMinute;
    int timeDifference = schedTotalMinutes - currentTotalMinutes;

    // If the schedule is later today
    if (timeDifference > 0) {
      if (timeDifference < minTimeDifference) {
        minTimeDifference = timeDifference;
        nextIndex = i;
      }
    } 
    // If the schedule has already passed today, calculate time for tomorrow
    else {
      int tomorrowDifference = timeDifference + 1440; 
      if (tomorrowDifference < minTimeDifference) {
        minTimeDifference = tomorrowDifference;
        nextIndex = i;
      }
    }
  }
  return nextIndex;
}

void sendStatusToSender() {
  
  // If motor is running, do not block radio transmissions long
  String response = String(motorIsRunning) + "|" + signal + "|SCH:" + String(totalSchedules) + "|";

  // 1. Append all active schedules currently loaded in memory
  if (totalSchedules == 0) {
    response += "NONE";
  } else {
    for (int i = 0; i < totalSchedules; i++) {
      response += String(activeSchedules[i].startHour) + ":" + 
                  String(activeSchedules[i].startMinute) + "," + 
                  String(activeSchedules[i].runMinutes);
      if (i < totalSchedules - 1) response += ";"; // Separate items with a semicolon
    }
  }

  // 2. Determine and append the next schedule
  response += "|NEXT:";
  int nextIdx = getNextScheduleIndex();
  if (nextIdx != -1) {
    response += String(activeSchedules[nextIdx].startHour) + ":" + 
                String(activeSchedules[nextIdx].startMinute) + " (Run " + 
                String(activeSchedules[nextIdx].runMinutes) + "m)";
  } else {
    response += "NONE";
  }

  // 3. Transmit the packet out over the air
  Serial.print("Sending telemetry packet: ");
  Serial.println(response);

  LoRa.beginPacket();
  LoRa.print(response);
  LoRa.endPacket();
  
  // Crucial: Put the LoRa module back into continuous receive mode
  LoRa.receive(); 
}


// Pseudo-internal clock progression to bridge transmission gaps
void updateInternalClock() {
  if (millis() - lastMinuteUpdateTick >= 60000) {
    lastMinuteUpdateTick += 60000;
    currentMinute++;
    if (currentMinute >= 60) {
      currentMinute = 0;
      currentHour++;
      if (currentHour >= 24) {
        currentHour = 0;
      }
    }
    Serial.print("Time Step: ");
    printTime(currentHour, currentMinute);
  }
}

// Scan current pointers to engage hardware
void manageMotorState() {
  if (motorIsRunning) {
    if (millis() >= motorEndTime) {
      digitalWrite(MOTOR_PIN, LOW);
      motorIsRunning = false;
      Serial.println("Motor cycle completed. Shutting down system.");
    }
    return; // Don't interrupt while cycle is currently operating
  }

  // Don't fire engine if no configuration structures loaded 
  if (totalSchedules == 0) return;

  // Evaluate matching timelines to close relay channels
  for (int i = 0; i < totalSchedules; i++) {
    if (currentHour == activeSchedules[i].startHour && currentMinute == activeSchedules[i].startMinute) {
      // Basic check ensures safety variables map above zero bounds
      if (activeSchedules[i].runMinutes > 0) { 
        motorIsRunning = true;
        motorEndTime = millis() + ((unsigned long)activeSchedules[i].runMinutes * 60000);
        digitalWrite(MOTOR_PIN, HIGH);
        Serial.print("Schedule Match! Starting motor for ");
        Serial.print(activeSchedules[i].runMinutes);
        Serial.println(" minutes.");
        break; 
      }
    }
  }
}

