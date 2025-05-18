#include <Arduino.h>

// Function prototypes
float measureOffset(int sensorPin);
float measureCurrent(int sensorPin, float offsetVoltage);
void handleTheftDetected();
void handleNoTheft();

// For ESP32 DevKit V1 - Type-C
const int inputSensorPin = 32;    // GPIO32 for input ACS712
const int loadSensorPin = 33;     // GPIO33 for load ACS712
const int relayPin = 25;          // GPIO25 for relay control

const float sensitivity = 0.185;  // For ACS712-5A: 185 mV/A
const float voltageRef = 3.3;     // ESP32 uses 3.3V ADC reference
float inputOffsetVoltage = 1.65;  // Midpoint for 3.3V system
float loadOffsetVoltage = 1.65;

const int samples = 500;
const float theftThreshold = 0.1; // Theft detection threshold (0.1A)
const int debounceTime = 2000;    // Debounce time (2 seconds)
const int cutoffDelay = 3000;     // 3 seconds delay before cutting off relay
const int restoreDelay = 5000;    // 5 seconds to verify theft is removed before restoring

unsigned long theftStartTime = 0; // Time when theft was first detected
unsigned long theftConfirmTime = 0;
unsigned long theftRemovalTime = 0; // Time when theft was potentially removed
bool theftDetected = false;       // Theft detection status
bool powerCutoff = false;         // Power cutoff state

void setup() {
  Serial.begin(115200);  // Start Serial Monitor at 115200 baud rate

  // Initialize relay pin
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // LOW keeps relay normally ON (NC connected)

  // Calibrate sensors (measure 0A voltage)
  inputOffsetVoltage = measureOffset(inputSensorPin);
  loadOffsetVoltage = measureOffset(loadSensorPin);

  Serial.println("System Ready");
  delay(1000);  // Initial delay to settle the system
}

void loop() {
  float inputCurrent = measureCurrent(inputSensorPin, inputOffsetVoltage);
  float loadCurrent = measureCurrent(loadSensorPin, loadOffsetVoltage);
  float currentDifference = abs(inputCurrent - loadCurrent);

  // THEFT DETECTION LOGIC
  if (currentDifference > theftThreshold) {
    // Reset removal timer if theft is still happening
    theftRemovalTime = 0;
    
    if (!theftDetected) {
      // Initial theft detection
      if (theftStartTime == 0) {
        theftStartTime = millis();
      }
      
      // Confirm theft after debounce period
      if (millis() - theftStartTime >= debounceTime) {
        theftDetected = true;
        theftConfirmTime = millis(); // Start delay timer for cutoff
        handleTheftDetected();
      }
    } else if (!powerCutoff) {
      // Wait for cutoff delay before cutting power
      if (millis() - theftConfirmTime >= cutoffDelay) {
        digitalWrite(relayPin, HIGH);  // Relay OFF (cut off theft load)
        powerCutoff = true;
        Serial.println("Theft Load Cutoff - Power will restore when theft is removed");
      }
    }
  } else {
    // NO THEFT or THEFT REMOVED
    
    if (powerCutoff) {
      // Start counting time since potential theft removal
      if (theftRemovalTime == 0) {
        theftRemovalTime = millis();
      }
      
      // Verify theft remained removed for the restore delay period
      if (millis() - theftRemovalTime >= restoreDelay) {
        digitalWrite(relayPin, LOW);  // Restore power
        powerCutoff = false;
        theftDetected = false;
        theftStartTime = 0;
        theftConfirmTime = 0;
        Serial.println("Theft load removed - Power restored");
      }
    } else {
      // Normal operation (no theft)
      theftStartTime = 0;
      theftDetected = false;
      theftConfirmTime = 0;
      handleNoTheft();
    }
  }

  delay(500);
}

void handleTheftDetected() {
  Serial.println("Theft Detected!");
}

void handleNoTheft() {
  Serial.println("No Theft");
}

float measureOffset(int sensorPin) {
  long sumSensorValue = 0;
  for (int i = 0; i < samples; i++) {
    sumSensorValue += analogRead(sensorPin);
  }

  float avg = sumSensorValue / (float)samples;
  return (avg / 4095.0) * voltageRef;
}

float measureCurrent(int sensorPin, float offsetVoltage) {
  long sumSensorValue = 0;
  for (int i = 0; i < samples; i++) {
    sumSensorValue += analogRead(sensorPin);
  }

  float avgSensorValue = sumSensorValue / (float)samples;
  float voltage = (avgSensorValue / 4095.0) * voltageRef;

  float current = (voltage - offsetVoltage) / sensitivity;

  if (abs(current) < 0.02) current = 0;

  return current;
}
