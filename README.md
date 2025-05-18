# ESP32 Power Theft Detection System

A smart IoT-based power theft detection system using ESP32 and current sensors with a real-time web dashboard.

## Overview

This system detects power theft by comparing current readings between input and load lines. When theft is detected (measured by a significant difference between input and load current), the system alerts users through a web dashboard. Users can then manually cut off power or restore it remotely from anywhere with internet access.

## Features

- **Real-time Current Monitoring**: Measures current from input and load using ACS712 sensors
- **Theft Detection Algorithm**: Compares input and output current to detect unauthorized taps
- **Manual Control**: Cut off or restore power remotely via web dashboard
- **Firebase Integration**: Data stored in Firebase Realtime Database for remote access
- **Web Dashboard**: Mobile-responsive interface with real-time updates
- **Location Tracking**: Shows device location on an integrated map
- **Visual & Audio Alerts**: Clear notifications when theft is detected
- **Backup Local Interface**: Fallback local web server if internet connection fails

## Hardware Requirements

- ESP32 Development Board (ESP32 DevKit V1 or similar)
- 2 × ACS712 Current Sensors (5A or appropriate for your application)
- Relay Module (to control power)
- Power Supply for ESP32 (5V)
- Jumper Wires
- Breadboard/PCB for circuit assembly

## Wiring Connections

- ACS712 Input Sensor → GPIO32 on ESP32
- ACS712 Load Sensor → GPIO33 on ESP32
- Relay Control → GPIO25 on ESP32
- Connect sensors in series with input and load lines

## Software Requirements

- PlatformIO IDE (recommended) or Arduino IDE
- Required Libraries:
  - Firebase ESP32 Client (version 3.17.1)
  - ArduinoJson (version 6.21.3)
  - ESP32 Arduino Core

## Installation

### Setting up the Hardware

1. Connect the ACS712 sensors to your power lines
2. Wire the ESP32 as per the connection guide above
3. Connect the relay module to control the power line

### Setting up Firebase

1. Create a Firebase project at [Firebase Console](https://console.firebase.google.com/)
2. Set up a Realtime Database
3. Update the Firebase credentials in `src/main.cpp`:
   ```cpp
   #define FIREBASE_HOST "your-project-id.firebasedatabase.app"
   #define FIREBASE_AUTH "your-database-secret-or-api-key"
   ```

### Uploading the Code

**Using PlatformIO (Recommended):**

1. Clone this repository
2. Open the project in PlatformIO
3. Update WiFi credentials in `src/main.cpp`:
   ```cpp
   const char* ssid = "your-wifi-ssid";
   const char* password = "your-wifi-password";
   ```
4. Update location data for your installation:
   ```cpp
   const float latitude = 12.938477;   // Your location
   const float longitude = 77.564919;  // Your location
   ```
5. Connect ESP32 via USB
6. Click Upload button or run:
   ```
   pio run --target upload
   ```

**Using Arduino IDE:**

1. Install required libraries through Library Manager
2. Set board to ESP32 Dev Module
3. Copy code from `src/main.cpp` to a new sketch
4. Update WiFi and Firebase credentials
5. Upload to your ESP32

## Usage

### Web Dashboard

1. Open `firebase-dashboard.html` in any web browser
2. The dashboard will connect to Firebase and show real-time data
3. Monitor current readings and theft status
4. If theft is detected, an alert will appear
5. Use the Emergency Cutoff and Restore Power buttons to control the system

### Local Dashboard (Backup)

If needed, you can also access a local dashboard:

1. Find your ESP32's IP address from the serial monitor
2. Open `dashboard.html` in a browser
3. Enter the ESP32's IP address when prompted

## Calibration

The system uses a default threshold of 0.1A for theft detection. You may need to adjust the following variables in `src/main.cpp` for your specific setup:

```cpp
const float sensitivity = 0.185;  // For ACS712-5A: 185 mV/A
const float theftThreshold = 0.1; // Theft detection threshold (0.1A)
```

## Troubleshooting

- **WiFi Connection Issues**: Check SSID and password. The ESP32 will still function without WiFi, but without remote capabilities.
- **Firebase Connection Failed**: Verify your API key and database URL.
- **Current Readings Inaccurate**: Calibrate the `offsetVoltage` values or adjust the sensitivity for your specific ACS712 model.
- **Theft Detection Too Sensitive/Insensitive**: Adjust `theftThreshold` value.

## License

This project is released under the MIT License.

## Acknowledgments

- ACS712 Current Sensor library
- Firebase ESP32 Client library by Mobizt
- ESP32 Arduino Core
- TailwindCSS for the dashboard UI
- Leaflet for map integration

## Project Structure

```
/
├── platformio.ini          # PlatformIO configuration
├── src/
│   └── main.cpp            # Main ESP32 code
├── firebase-dashboard.html # Remote web interface 
└── dashboard.html          # Local backup interface
``` 