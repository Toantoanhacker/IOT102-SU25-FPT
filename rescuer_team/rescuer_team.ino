// =======================================================================
// RECEIVER CODE V18: Final - Advanced Serial Messaging for Full GUI
// =======================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>

// --- OLED and NRF Configuration (Unchanged) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int CE_PIN = 9; const int CSN_PIN = 8;
RF24 radio(CE_PIN, CSN_PIN);
const byte pipeAddress[2][6] = {"PIPE0", "PIPE1"};

// --- Pinout and State Variables (Unchanged) ---
const int BUZZER_PIN = 3;
const int CONTROL_BUTTON_PIN = 2;
bool remoteBeaconState = false;
long lastButtonPressTime = 0;
const long DEBOUNCE_DELAY = 250;
unsigned long previousBeepTime = 0;
bool isBuzzerOn = false;

// --- Data Structures (Unchanged) ---
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
  delay(2000);
  Serial.begin(115200); // Corrected baud rate typo from 115220 to 115200
  Serial.println("Receiver V18 (Advanced Messaging) Initialized");

  pinMode(CONTROL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while(1); }
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("Initializing..."); display.display(); delay(500);

  if (!radio.begin()) { while (1); }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(pipeAddress[1]);
  radio.openReadingPipe(1, pipeAddress[0]);
  radio.startListening();

  display.clearDisplay(); display.setCursor(0, 0);
  display.println("NRF OK. Waiting..."); display.display();
}

// Check if the rescuer has pressed their control button
void checkControlButton() {
  if (digitalRead(CONTROL_BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
      remoteBeaconState = !remoteBeaconState;
      rescuerDataPacket.activateBeacon = remoteBeaconState;
      
      // Send the command to the Victim
      radio.stopListening();
      radio.write(&rescuerDataPacket, sizeof(RescuerData));
      radio.startListening();
      
      // --- NEW: IMMEDIATELY send the beacon command status to the Node.js app ---
      String beaconCmdMsg = "BEACON_CMD:";
      beaconCmdMsg += remoteBeaconState ? "1" : "0";
      beaconCmdMsg += "|";
      Serial.print(beaconCmdMsg); // Use print() to send the raw string
      Serial.flush();
      
      lastButtonPressTime = millis();
    }
  }
}

// Handle the local buzzer alert - LOGIC UNCHANGED
void handleLocalBuzzer() {
  if (victimDataPacket.isSosSignal || victimDataPacket.isFallDetected) {
    if (millis() - previousBeepTime >= 500) {
      isBuzzerOn = !isBuzzerOn;
      digitalWrite(BUZZER_PIN, isBuzzerOn);
      previousBeepTime = millis();
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    isBuzzerOn = false;
  }
}

// Check for any incoming data packets from the victim
void checkForVictimData() {
    if (radio.available()) {
        radio.read(&victimDataPacket, sizeof(VictimData));
        updateOledDisplay();
        sendVictimDataToNodeApp(); // Changed function name for clarity
    }
}

void loop() {
  checkControlButton();
  checkForVictimData();
  handleLocalBuzzer();
}

// Update the OLED display - LOGIC UNCHANGED
void updateOledDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (victimDataPacket.latitude != 0.0) {
    display.print("Lat: "); display.println(victimDataPacket.latitude, 4);
    display.print("Lng: "); display.println(victimDataPacket.longitude, 4);
  } else {
    display.println("Victim GPS: No Fix");
  }
  display.print("SOS: "); display.print(victimDataPacket.isSosSignal ? "ON " : "OFF");
  display.print(" / Fall: "); display.println(victimDataPacket.isFallDetected ? "Yes" : "No");
  display.setCursor(0, 24);
  display.print("Beacon CMD Sent: ");
  display.print(remoteBeaconState ? "ON" : "OFF");
  display.display();
}

// --- UPDATED: This function now sends the full victim data packet ---
void sendVictimDataToNodeApp() {
  if (victimDataPacket.latitude != 0.0) {
    // New Format: "GPS_DATA:lat,lng,sos_state,fall_state|"
    String appData = "GPS_DATA:";
    appData += String(victimDataPacket.latitude, 6);
    appData += ",";
    appData += String(victimDataPacket.longitude, 6);
    appData += ",";
    appData += victimDataPacket.isSosSignal ? "1" : "0";
    appData += ",";
    appData += victimDataPacket.isFallDetected ? "1" : "0";
    appData += "|";
    
    // Send the raw string using print(), not println()
    Serial.print(appData);
    Serial.flush();
  }
}