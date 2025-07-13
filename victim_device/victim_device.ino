// =======================================================================
// TRANSMITTER CODE V17: Final - Integrated Fall Detection & Cancellation
// =======================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <stdint.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- NRF, GPS, MPU objects & Pin Configurations are unchanged ---
const int CE_PIN = 16;
const int CSN_PIN = 0;
RF24 radio(CE_PIN, CSN_PIN);
const byte pipeAddress[2][6] = {"PIPE0", "PIPE1"};
const int GPS_RX_PIN = 2; const int GPS_TX_PIN = 15;
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;
Adafruit_MPU6050 mpu;
const float FALL_IMPACT_THRESHOLD = 3.5;
unsigned long lastFallDetectTime = 0;
const long FALL_COOLDOWN_MS = 5000;
const int EMERGENCY_BUTTON_PIN = 15; // D8
const int ALERT_PIN = 3;             // RX Pin

// --- State Variables ---
bool sosState = false;      // Is the user sending a manual SOS?
bool fallState = false;     // Has a fall been detected and not yet cancelled?
bool isLocalBeaconActive = false;
long lastButtonPressTime = 0;
const long DEBOUNCE_DELAY = 250;
unsigned long previousBeaconToggleTime = 0;
bool isBeaconOn = false;

// --- Data Structures are unchanged ---
struct VictimData {
  float latitude;
  float longitude;
  bool isSosSignal;
  bool isFallDetected;
};
VictimData victimDataPacket;
struct RescuerData {
  bool activateBeacon;
};
RescuerData rescuerDataPacket;


void setup() {
  // Setup is identical to V16. Serial is disabled.
  pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP); 
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);
  delay(1000); 
  Wire.begin(); 
  int mpu_retries = 5; while (mpu_retries > 0) { if (mpu.begin()) break; delay(1000); mpu_retries--; }
  if (mpu_retries == 0) { while(1) { digitalWrite(ALERT_PIN, !digitalRead(ALERT_PIN)); delay(100); } }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  if (!radio.begin()) { while (1); }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(pipeAddress[0]);
  radio.openReadingPipe(1, pipeAddress[1]);
  radio.startListening();
  gpsSerial.begin(9600);
}

// --- UPDATED LOGIC FUNCTIONS ---

// The button now has dual purpose: cancel a fall or toggle a manual SOS
void checkSosButton() {
  if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
      // A valid button press occurred.
      if (fallState) {
        // If a fall was detected, this press is a "cancel".
        // Clear both the fall state and the SOS state it triggered.
        fallState = false;
        sosState = false;
      } else {
        // If there was no active fall, this is a manual SOS toggle.
        sosState = !sosState;
      }
      lastButtonPressTime = millis();
    }
  }
}

// MPU check now directly triggers the SOS state
void processMpuData() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float totalG = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2)) / 9.81;
  bool isCooldown = millis() - lastFallDetectTime < FALL_COOLDOWN_MS;
  if (totalG > FALL_IMPACT_THRESHOLD && !isCooldown) {
    lastFallDetectTime = millis();
    // A fall was detected!
    fallState = true;
    // AUTOMATICALLY activate the SOS signal.
    sosState = true; 
  }
}

// Check for remote commands - this logic is unchanged
void checkForRemoteCommands() {
    if (radio.available()) {
        radio.read(&rescuerDataPacket, sizeof(RescuerData));
        isLocalBeaconActive = rescuerDataPacket.activateBeacon;
    }
}

// Handle local beacon - this logic is unchanged
void handleLocalBeacon() {
  if (isLocalBeaconActive) {
    if (millis() - previousBeaconToggleTime >= 500) {
      isBeaconOn = !isBeaconOn;
      digitalWrite(ALERT_PIN, isBeaconOn);
      previousBeaconToggleTime = millis();
    }
  } else {
    digitalWrite(ALERT_PIN, LOW);
    isBeaconOn = false;
  }
}

void loop() {
  // Run all state-management functions
  checkForRemoteCommands();
  checkSosButton();
  processMpuData();
  handleLocalBeacon();
  
  // Feed GPS data continuously
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Once a second, when GPS gets a new sentence, send our status
  if (gps.location.isUpdated()) {
    // Populate GPS data
    if (gps.location.isValid()) {
      victimDataPacket.latitude = gps.location.lat();
      victimDataPacket.longitude = gps.location.lng();
    } else {
      victimDataPacket.latitude = 0.0f;
      victimDataPacket.longitude = 0.0f;
    }
    // Populate the alert flags
    victimDataPacket.isFallDetected = fallState;
    victimDataPacket.isSosSignal = sosState;

    // --- Send the Data Packet ---
    radio.stopListening();
    radio.write(&victimDataPacket, sizeof(VictimData));
    radio.startListening();
  }
}