#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Firebase ESP Client Library
#if defined(ESP32)
  #include <FirebaseESP32.h>
#elif defined(ESP8266)
  #include <FirebaseESP8266.h>
#endif

// Power state constants
#define POWER_ON 0
#define POWER_OFF 1

// New direct paths for power control
const char* POWER_STATE_PATH = "/theft_detection/power_state";
const char* POWER_COMMAND_ACK = "/theft_detection/power_command_ack";

// Function prototypes
float measureOffset(int sensorPin);
float measureCurrent(int sensorPin, float offsetVoltage);
void handleTheftDetected();
void handleNoTheft();
void sendDataToFirebase();
void checkFirebaseCommands();

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
const int debounceTime = 1000;    // Reduced from 2000ms to 1000ms (1 second)
const int restoreDelay = 1000;    // Reduced from 2000ms to 1000ms (1 second)

unsigned long theftStartTime = 0; // Time when theft was first detected
unsigned long theftRemovalTime = 0; // Time when theft was potentially removed
bool theftDetected = false;       // Theft detection status
bool powerCutoff = false;         // Power cutoff state

// WiFi and WebServer Configuration
const char* ssid = "unnathi 2.4G";        // Replace with your WiFi SSID
const char* password = "8217566789"; // Replace with your WiFi password

// Hardcoded location information
const float latitude = 12.938477;   // nandi aroma
const float longitude = 77.564919;  

// Create web server on port 80
WebServer server(80);

// Global variables for API access
float globalInputCurrent = 0;
float globalLoadCurrent = 0;
float globalCurrentDifference = 0;

// Firebase configuration
#define API_KEY "AIzaSyBc1-TjprtzvD0S_AcNNnO3xhs-c1pBYVU"
#define DATABASE_URL "https://esp32-theft-detection-default-rtdb.asia-southeast1.firebasedatabase.app"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize Firebase calls
#define FIREBASE_HOST "esp32-theft-detection-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyBc1-TjprtzvD0S_AcNNnO3xhs-c1pBYVU"  // Use API key as auth token

// Define Firebase database paths - using full paths instead of concatenation
const char* PATH_ROOT = "/theft_detection";
const char* PATH_INPUT_CURRENT = "/theft_detection/input_current";
const char* PATH_LOAD_CURRENT = "/theft_detection/load_current";
const char* PATH_DIFFERENCE = "/theft_detection/difference";
const char* PATH_THEFT_DETECTED = "/theft_detection/theft_detected";
const char* PATH_POWER_CUTOFF = "/theft_detection/power_cutoff";
const char* PATH_LATITUDE = "/theft_detection/location/latitude";
const char* PATH_LONGITUDE = "/theft_detection/location/longitude";
const char* PATH_LAST_UPDATE = "/theft_detection/last_update";
const char* PATH_CMD_CUTOFF = "/theft_detection/commands/cutoff";
const char* PATH_CMD_RESTORE = "/theft_detection/commands/restore";

// Last time data was sent to Firebase
unsigned long lastFirebaseUpdate = 0;
const int firebaseUpdateInterval = 2000; // Reduced from 5000ms to 2000ms (2 seconds)

// API Handlers
void handleRoot() {
  char html[1024]; // Buffer for HTML content
  char currentStr[10], loadStr[10], diffStr[10];
  
  // Convert float values to strings with 3 decimal places
  dtostrf(globalInputCurrent, 1, 3, currentStr);
  dtostrf(globalLoadCurrent, 1, 3, loadStr);
  dtostrf(globalCurrentDifference, 1, 3, diffStr);
  
  // Build HTML using sprintf
  sprintf(html,
    "<html><head>"
    "<title>ESP32 Theft Detection</title>"
    "<meta http-equiv='refresh' content='5'>"
    "</head><body>"
    "<h1>ESP32 Theft Detection System</h1>"
    "<p>Input Current: %s A</p>"
    "<p>Load Current: %s A</p>"
    "<p>Difference: %s A</p>"
    "<p>Status: %s</p>"
    "<p>Power: %s</p>"
    "</body></html>",
    currentStr, loadStr, diffStr,
    theftDetected ? "THEFT DETECTED!" : "Normal",
    powerCutoff ? "OFF" : "ON"
  );
  
  server.send(200, "text/html", html);
}

void handleData() {
  DynamicJsonDocument doc(1024);
  doc["inputCurrent"] = globalInputCurrent;
  doc["loadCurrent"] = globalLoadCurrent;
  doc["difference"] = globalCurrentDifference;
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  
  server.send(200, "application/json", jsonResponse);
}

void handleStatus() {
  DynamicJsonDocument doc(1024);
  doc["theftDetected"] = theftDetected;
  doc["powerCutoff"] = powerCutoff;
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  
  server.send(200, "application/json", jsonResponse);
}

void handleLocation() {
  DynamicJsonDocument doc(1024);
  doc["latitude"] = latitude;
  doc["longitude"] = longitude;
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  
  server.send(200, "application/json", jsonResponse);
}

void handleCutoff() {
  // Check if this is a POST request
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  digitalWrite(relayPin, HIGH);  // Cut off power
  powerCutoff = true;
  
  server.send(200, "text/plain", "Power cutoff activated");
  Serial.println("Manual power cutoff activated via API");
}

void handleRestore() {
  // Check if this is a POST request
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  digitalWrite(relayPin, LOW);  // Restore power
  powerCutoff = false;
  theftDetected = false;
  
  server.send(200, "text/plain", "Power restored");
  Serial.println("Manual power restore activated via API");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// WiFi diagnostic functions
void printWiFiStatus() {
  Serial.println("");
  Serial.print("WiFi Status: ");
  
  switch (WiFi.status()) {
    case WL_CONNECTED:
      Serial.println("WL_CONNECTED");
      break;
    case WL_NO_SHIELD:
      Serial.println("WL_NO_SHIELD");
      break;
    case WL_IDLE_STATUS:
      Serial.println("WL_IDLE_STATUS");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("WL_NO_SSID_AVAIL - Cannot find the SSID");
      break;
    case WL_SCAN_COMPLETED:
      Serial.println("WL_SCAN_COMPLETED");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("WL_CONNECT_FAILED - Wrong password");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("WL_CONNECTION_LOST");
      break;
    case WL_DISCONNECTED:
      Serial.println("WL_DISCONNECTED");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
  }
}

void scanWiFiNetworks() {
  Serial.println("Scanning for WiFi networks...");
  
  int networksFound = WiFi.scanNetworks();
  
  if (networksFound == 0) {
    Serial.println("No WiFi networks found");
  } else {
    Serial.print(networksFound);
    Serial.println(" networks found:");
    
    for (int i = 0; i < networksFound; ++i) {
      // Print SSID and RSSI for each network
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm) ");
      
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:
          Serial.println("Open");
          break;
        case WIFI_AUTH_WEP:
          Serial.println("WEP");
          break;
        case WIFI_AUTH_WPA_PSK:
          Serial.println("WPA");
          break;
        case WIFI_AUTH_WPA2_PSK:
          Serial.println("WPA2");
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          Serial.println("WPA+WPA2");
          break;
        case WIFI_AUTH_WPA3_PSK:
          Serial.println("WPA3");
          break;
        default:
          Serial.println("Unknown");
          break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);  // Start Serial Monitor at 115200 baud rate

  // Initialize relay pin
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // LOW keeps relay normally ON (NC connected)

  // Calibrate sensors (measure 0A voltage)
  inputOffsetVoltage = measureOffset(inputSensorPin);
  loadOffsetVoltage = measureOffset(loadSensorPin);

  // Connect to WiFi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  // Scan for available networks to check if the SSID is visible
  scanWiFiNetworks();
  
  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  WiFi.begin(ssid, password);

  // Try to connect with timeout
  int timeout_counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    timeout_counter++;
    
    // Try for 20 seconds (40 * 500ms)
    if (timeout_counter >= 40) {
      Serial.println("\nWiFi connection failed!");
      printWiFiStatus(); // Print the reason for failure
      Serial.println("System will continue without WiFi functionality.");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Set up server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/data", HTTP_GET, handleData);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/location", HTTP_GET, handleLocation);
    server.on("/api/cutoff", HTTP_POST, handleCutoff);
    server.on("/api/restore", HTTP_POST, handleRestore);
    server.onNotFound(handleNotFound);
  
    // Add CORS headers
    server.enableCORS(true);
  
    // Start server
    server.begin();
    Serial.println("HTTP server started");
    
    // Initialize Firebase
    Serial.println("Initializing Firebase...");
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);
    
    // Check Firebase connection
    delay(1000);
    if (Firebase.ready()) {
      Serial.println("Firebase connected successfully");
      
      // Set up initial command values (legacy support)
      Firebase.setBool(fbdo, PATH_CMD_CUTOFF, false);
      Firebase.setBool(fbdo, PATH_CMD_RESTORE, false);
      
      // Set up new power command system with separate paths
      Firebase.setInt(fbdo, "/theft_detection/commands/cutoff_time", 0);
      Firebase.setInt(fbdo, "/theft_detection/commands/restore_time", 0);
      Firebase.setInt(fbdo, "/theft_detection/status/cutoff_executed", 0);
      Firebase.setInt(fbdo, "/theft_detection/status/restore_executed", 0);
      
      // Update current state in Firebase
      Firebase.setBool(fbdo, PATH_POWER_CUTOFF, powerCutoff);
      Firebase.setBool(fbdo, PATH_THEFT_DETECTED, theftDetected);
      
      // Add initial sensor data
      Firebase.setFloat(fbdo, PATH_INPUT_CURRENT, 0);
      Firebase.setFloat(fbdo, PATH_LOAD_CURRENT, 0);
      Firebase.setFloat(fbdo, PATH_DIFFERENCE, 0);
      
      Serial.println("Initial Firebase data set");
      
      // Initial check for commands to ensure static variables are initialized
      checkFirebaseCommands();
    } else {
      Serial.println("Firebase initialization failed");
      Serial.print("Reason: ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }

  Serial.println("System Ready");
  delay(1000);  // Initial delay to settle the system
}

void loop() {
  // Handle client requests
  server.handleClient();
  
  // Check for Firebase commands first - high priority
  if (WiFi.status() == WL_CONNECTED) {
    checkFirebaseCommands();
  }
  
  // Measure current values
  float inputCurrent = measureCurrent(inputSensorPin, inputOffsetVoltage);
  float loadCurrent = measureCurrent(loadSensorPin, loadOffsetVoltage);
  float currentDifference = abs(inputCurrent - loadCurrent);
  
  // Update global variables for API access
  globalInputCurrent = inputCurrent;
  globalLoadCurrent = loadCurrent;
  globalCurrentDifference = currentDifference;

  // Send data to Firebase regularly
  if (WiFi.status() == WL_CONNECTED) {
    sendDataToFirebase();
    
    // Update power status regularly to ensure consistency
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 2000) {
      if (Firebase.ready()) {
        Firebase.setBool(fbdo, PATH_POWER_CUTOFF, powerCutoff);
      }
      lastStatusUpdate = millis();
    }
  }

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
        handleTheftDetected();
        // No automatic power cutoff - this will now be done manually via dashboard
      }
    }
    // Removed automatic cutoff code
  } else {
    // NO THEFT or THEFT REMOVED
    
    if (theftDetected) {
      // Start counting time since potential theft removal
      if (theftRemovalTime == 0) {
        theftRemovalTime = millis();
        // Immediately send a preliminary update to Firebase
        if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
          Firebase.setFloat(fbdo, "/theft_detection/difference", currentDifference);
          Firebase.setInt(fbdo, "/theft_detection/theft_removal_start", millis());
        }
      }
      
      // Verify theft remained removed for the restore delay period
      if (millis() - theftRemovalTime >= restoreDelay) {
        bool previousTheftState = theftDetected;
        theftDetected = false;
        theftStartTime = 0;
        Serial.println("Theft condition resolved - but power remains in current state");
        
        // Immediately update Firebase when theft is no longer detected
        if (previousTheftState && WiFi.status() == WL_CONNECTED && Firebase.ready()) {
          Firebase.setBool(fbdo, PATH_THEFT_DETECTED, false);
          Firebase.setInt(fbdo, "/theft_detection/alert/timestamp", millis());
          Firebase.setBool(fbdo, "/theft_detection/alert/new_theft", false);
          Firebase.setInt(fbdo, "/theft_detection/alert/resolved_time", millis());
        }
      }
    } else {
      // Normal operation (no theft)
      theftStartTime = 0;
      theftRemovalTime = 0;
      handleNoTheft();
    }
  }

  delay(50);  // Reduced from 100ms to 50ms for better responsiveness
}

void handleTheftDetected() {
  Serial.println("Theft Detected! Use dashboard to control power.");
  
  // Force immediate update to Firebase to alert users
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    Firebase.setBool(fbdo, PATH_THEFT_DETECTED, true);
    // Optional: You could add a notification path specifically for alerting
    Firebase.setBool(fbdo, "/theft_detection/alert/new_theft", true);
    Firebase.setInt(fbdo, "/theft_detection/alert/timestamp", millis());
  }
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
  int actualSamples = 250; // Reduced from 500 to 250 samples for faster measurement
  
  for (int i = 0; i < actualSamples; i++) {
    sumSensorValue += analogRead(sensorPin);
  }

  float avgSensorValue = sumSensorValue / (float)actualSamples;
  float voltage = (avgSensorValue / 4095.0) * voltageRef;

  float current = (voltage - offsetVoltage) / sensitivity;

  if (abs(current) < 0.02) current = 0;

  return current;
}

// Firebase functions
void sendDataToFirebase() {
  if (millis() - lastFirebaseUpdate < firebaseUpdateInterval) {
    return; // Not time to update yet
  }
  
  // Force update immediately when theft status changes
  static bool lastTheftState = false;
  
  if (Firebase.ready() && (lastTheftState != theftDetected || millis() - lastFirebaseUpdate >= firebaseUpdateInterval)) {
    // Update sensor data
    Firebase.setFloat(fbdo, PATH_INPUT_CURRENT, globalInputCurrent);
    Firebase.setFloat(fbdo, PATH_LOAD_CURRENT, globalLoadCurrent);
    Firebase.setFloat(fbdo, PATH_DIFFERENCE, globalCurrentDifference);
    
    // Update status
    Firebase.setBool(fbdo, PATH_THEFT_DETECTED, theftDetected);
    Firebase.setBool(fbdo, PATH_POWER_CUTOFF, powerCutoff);
    
    // Update location
    Firebase.setFloat(fbdo, PATH_LATITUDE, latitude);
    Firebase.setFloat(fbdo, PATH_LONGITUDE, longitude);
    
    // Update timestamp
    Firebase.setInt(fbdo, PATH_LAST_UPDATE, millis());
    
    lastFirebaseUpdate = millis();
    lastTheftState = theftDetected;
  }
}

// Check Firebase for direct power state command
void checkFirebaseCommands() {
  if (!Firebase.ready()) {
    return;
  }
  
  // Read the desired power state directly
  if (Firebase.getInt(fbdo, POWER_STATE_PATH)) {
    int desiredState = fbdo.intData();
    Serial.print("Read power_state from Firebase: ");
    Serial.println(desiredState);
    
    // Current actual state
    int currentState = powerCutoff ? POWER_OFF : POWER_ON;
    
    // Only take action if there's a mismatch between desired and actual state
    if (desiredState != currentState) {
      Serial.print("State mismatch - Desired: ");
      Serial.print(desiredState);
      Serial.print(", Current: ");
      Serial.println(currentState);
      
      if (desiredState == POWER_OFF) {
        // Turn power OFF
        Serial.println("Turning power OFF");
        digitalWrite(relayPin, HIGH);  // Cut off power
        powerCutoff = true;
        
      } else if (desiredState == POWER_ON) {
        // Turn power ON
        Serial.println("Turning power ON");
        digitalWrite(relayPin, LOW);  // Restore power
        powerCutoff = false;
        
        // Don't automatically reset theft detection
        // Let the actual current measurements determine if theft is still happening
        // theftDetected remains unchanged until current measurements confirm
        
        // Only reset the timers to allow faster re-detection if needed
        theftStartTime = 0;
        theftRemovalTime = 0;
      }
      
      // Update actual state in Firebase
      Firebase.setBool(fbdo, PATH_POWER_CUTOFF, powerCutoff);
      
      // We're not changing theft status in Firebase here anymore
      // Only update it when the current measurements change it
      
      // Acknowledge command execution with timestamp
      Firebase.setInt(fbdo, POWER_COMMAND_ACK, millis());
      
      Serial.println("Power state updated and acknowledged");
    }
  } else {
    Serial.print("Failed to read power_state: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}
