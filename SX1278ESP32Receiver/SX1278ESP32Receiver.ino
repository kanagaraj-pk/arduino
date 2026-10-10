#include <SPI.h>
#include <LoRa.h>
#include <Preferences.h>


// ESP32 pin configuration - verify these against your board and wiring.
// Example for a classic ESP32 DevKit using VSPI with an SX127x LoRa module.
#define MOTOR_PIN 27
#define LORA_SCK_PIN 18
#define LORA_MISO_PIN 19
#define LORA_MOSI_PIN 23
#define LORA_SS_PIN 5
#define LORA_RST_PIN 14
#define LORA_DIO0_PIN 26

#define MAX_SCHEDULES 10
#define PREFS_NAMESPACE "motor-sched"
#define PREFS_COUNT_KEY "count"
#define PREFS_DATA_KEY "data"

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

String loraSignalInfo = "";

String GateStr = "";

bool loraReady = false;
unsigned long lastLoraRetry = 0;



void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW); // Motor  

  Serial.println("ESP32 LoRa Motor Scheduler Setup...");

  // Start SPI using the ESP32 pin assignments above.
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_SS_PIN);

  loraReady = initLoRa();

  // Override the default SPI pins for the LoRa library
  //LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

  // Initialize SX1278 at 433 MHz (change to 868E6 or 915E6 if using different module)
  //if (!LoRa.begin(433E6)) {
   // Serial.println("Starting LoRa failed! Check your wiring.");
   //while (1); // Halt execution
 // }
  // Apply settings after successful initialization
  //LoRa.setSpreadingFactor(12);     // Highest sensitivity
  //LoRa.setSignalBandwidth(125E3);  // 125 kHz
 // LoRa.setCodingRate4(8);          // Maximum error correction
 // LoRa.enableCrc();
 // LoRa.setTxPower(20);
 // LoRa.receive(); 
  //Serial.println("LoRa SX1278 Initialized Successfully!");


  // Load operating config from non-volatile memory
  loadScheduleFromPreferences();

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
  //Serial.println("LoRa Ready. Checking packets...");
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

    loraSignalInfo = "RSSI=" + String(rssi) + "," + " SNR=" + String(snr) + "," + " FERR=" + String(ferr);

    incomingMsg.trim();
    Serial.print(loraSignalInfo);

    Serial.print("Received: ");
    Serial.println(incomingMsg);

    processIncomingMessage(incomingMsg);
  }
  // 2. Keep track of current system runtime clocks
  updateInternalClock();

  // 3. Monitor schedules and manage active relays
  manageMotorState();
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
        clearSchedulePreferences();
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
      GateStr = msg.substring(0, firstPipe);
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
          Serial.println("New schedule configuration detected. Saving to ESP32 flash...");
    
            totalSchedules = parsedCount;
            for (int i = 0; i < totalSchedules; i++) {
              activeSchedules[i] = parsedSchedules[i];
            }
          
            saveScheduleToPreferences();
            sendStatusToSender(); // Transmit confirmation back with new data
        } else {
            // SAFETY RETREAT: Runs if data matches baseline exactly
            Serial.println("Schedules match existing baseline exactly. Preferences flash storage bypassed.");
            
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


// Clear saved schedules in ESP32 NVS flash.
// Preferences stores key/value data and handles flash management internally.
void clearSchedulePreferences() {
  if (totalSchedules == 0) {
    Serial.println("System is already clear. Skipping redundant flash write.");
  } else {
    totalSchedules = 0;

    Preferences prefs;
    if (prefs.begin(PREFS_NAMESPACE, false)) {
      prefs.putUChar(PREFS_COUNT_KEY, 0);
      prefs.remove(PREFS_DATA_KEY);
      prefs.end();
    } else {
      Serial.println("ERROR: Could not open Preferences to clear schedules.");
    }
  }

  if (motorIsRunning) {
    digitalWrite(MOTOR_PIN, LOW);
    motorIsRunning = false;
    Serial.println("Emergency Schedule Kill: Motor stopped.");
  }

  Serial.println("Non-volatile schedule states cleared.");
}

// Save the current schedule list to ESP32 NVS flash.
void saveScheduleToPreferences() {
  Preferences prefs;

  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    Serial.println("ERROR: Could not open Preferences to save schedules.");
    return;
  }

  const size_t dataSize = totalSchedules * sizeof(Schedule);
  size_t countWritten = prefs.putUChar(PREFS_COUNT_KEY, totalSchedules);
  size_t dataWritten = 0;

  if (dataSize > 0) {
    dataWritten = prefs.putBytes(PREFS_DATA_KEY, activeSchedules, dataSize);
  } else {
    prefs.remove(PREFS_DATA_KEY);
  }

  prefs.end();

  if (countWritten == 1 && (dataSize == 0 || dataWritten == dataSize)) {
    Serial.println("Schedule parameters successfully saved to ESP32 flash.");
  } else {
    Serial.println("ERROR: Schedule save may be incomplete.");
  }
}

// Load schedules from ESP32 NVS flash at startup.
void loadScheduleFromPreferences() {
  Preferences prefs;

  if (!prefs.begin(PREFS_NAMESPACE, true)) {
    totalSchedules = 0;
    Serial.println("No saved schedule storage available; starting empty.");
    return;
  }

  uint8_t savedCount = prefs.getUChar(PREFS_COUNT_KEY, 0);
  const size_t expectedSize = savedCount * sizeof(Schedule);
  size_t storedSize = prefs.getBytesLength(PREFS_DATA_KEY);

  if (savedCount > MAX_SCHEDULES ||
      (savedCount > 0 && storedSize != expectedSize)) {
    totalSchedules = 0;
    prefs.end();
    Serial.println("Saved schedules invalid or incomplete; starting empty.");
    return;
  }

  totalSchedules = savedCount;

  if (totalSchedules > 0) {
    size_t bytesRead = prefs.getBytes(PREFS_DATA_KEY, activeSchedules,
                                      expectedSize);
    if (bytesRead != expectedSize) {
      totalSchedules = 0;
      prefs.end();
      Serial.println("Could not read saved schedules; starting empty.");
      return;
    }
  }

  prefs.end();

  Serial.print("Loaded ");
  Serial.print(totalSchedules);
  Serial.println(" active operational sequences from ESP32 flash:");

  for (int i = 0; i < totalSchedules; i++) {
    Serial.print(" -> ");
    printTime(activeSchedules[i].startHour, activeSchedules[i].startMinute);
    Serial.print(" for ");
    Serial.print(activeSchedules[i].runMinutes);
    Serial.println(" mins");
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
  String response = String(motorIsRunning) + "|" + loraSignalInfo + "|SCH:" + String(totalSchedules) + "|";

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

