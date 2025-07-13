// =======================================================================
// RECEIVER CODE V17: Final - Two-Way Communication
// =======================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>

// --- OLED and NRF Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int CE_PIN = 9; const int CSN_PIN = 8;
RF24 radio(CE_PIN, CSN_PIN);
// Define two pipes for two-way communication. Must be the same on both devices.
const byte pipeAddress[2][6] = {"PIPE0", "PIPE1"};

// --- Pinout and State Variables ---
const int BUZZER_PIN = 3;
const int CONTROL_BUTTON_PIN = 2; // Rescuer's button on D2
bool remoteBeaconState = false;   // The state we WANT the victim's beacon to be
long lastButtonPressTime = 0;
const long DEBOUNCE_DELAY = 250;
unsigned long previousBeepTime = 0;
bool isBuzzerOn = false;

// --- Data Structures ---
// 1. Data this device RECEIVES from the victim
struct VictimData {
  float latitude;
  float longitude;
  bool isSosSignal;
  bool isFallDetected;
};
VictimData victimDataPacket;

// 2. Data this device SENDS to the victim
struct RescuerData {
  bool activateBeacon;
};
RescuerData rescuerDataPacket;


void setup() {
  Serial.begin(115200);
  Serial.println("Receiver V17 (Two-Way) Initialized");

  pinMode(CONTROL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while(1); }
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("Initializing..."); display.display(); delay(500);

  // NRF Initialization for Two-Way Communication
  if (!radio.begin()) { while (1); }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(pipeAddress[1]);    // We will SEND on PIPE1
  radio.openReadingPipe(1, pipeAddress[0]); // We will LISTEN for data on PIPE0
  radio.startListening();

  display.clearDisplay(); display.setCursor(0, 0);
  display.println("NRF OK. Waiting..."); display.display();
}

// Check if the rescuer has pressed their control button
void checkControlButton() {
  if (digitalRead(CONTROL_BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
      remoteBeaconState = !remoteBeaconState; // Toggle the desired remote state
      rescuerDataPacket.activateBeacon = remoteBeaconState;
      
      // --- Send the Command to the Victim ---
      radio.stopListening();
      radio.write(&rescuerDataPacket, sizeof(RescuerData));
      radio.startListening();
      
      lastButtonPressTime = millis();
    }
  }
}

// Handle the local buzzer alert based on incoming data
void handleLocalBuzzer() {
  // Buzzer beeps if an SOS is received from victim's button OR a fall is detected
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
        
        // As soon as we get data, update everything
        updateOledDisplay();
        sendToPhoneApp();
    }
}

void loop() {
  checkControlButton();
  checkForVictimData();
  handleLocalBuzzer();
}

// Update the OLED display with the latest data
void updateOledDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);

  // Display GPS info
  if (victimDataPacket.latitude != 0.0) {
    display.print("Lat: "); display.println(victimDataPacket.latitude, 4);
    display.print("Lng: "); display.println(victimDataPacket.longitude, 4);
  } else {
    display.println("Victim GPS: No Fix");
  }

  // Display Alert Status
  display.print("SOS: "); display.print(victimDataPacket.isSosSignal ? "ON " : "OFF");
  display.print(" / Fall: "); display.println(victimDataPacket.isFallDetected ? "YES" : "NO");

  // Display Remote Beacon status
  display.setCursor(0, 24);
  display.print("Victim Beacon: ");
  display.print(remoteBeaconState ? "ON" : "OFF");
  
  display.display();
}

// Send GPS data to the phone map app
void sendToPhoneApp() {
  if (victimDataPacket.latitude != 0.0) {
    String appData = String(victimDataPacket.latitude, 6);
    appData += ",";
    appData += String(victimDataPacket.longitude, 6);
    appData += "|";
    
    char charBuffer[50];
    appData.toCharArray(charBuffer, 50);
    Serial.write(charBuffer);
    Serial.flush();
  }
}