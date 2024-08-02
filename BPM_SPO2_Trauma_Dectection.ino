#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <MPU6050.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BluetoothSerial.h"

MAX30105 particleSensor;
MPU6050 accelGyro;
BluetoothSerial SerialBT;

const byte RATE_SIZE = 4; // Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

// Variables for SpO2 calculation
long redValue, irValue;
float SpO2;
int validSPO2, validHeartRate;

// Variables for accelerometer
int16_t axRaw, ayRaw, azRaw;
float ax, ay, az;
float resultantForce;
int trauma = 0;

unsigned long lastBluetoothSend = 0;

// WiFi Connect Bitmap (16x16)
const uint8_t wifi_connect_bitmap[] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x0e, 0x30, 0x18, 0x18, 0x03, 0xc0, 
    0x04, 0x20, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// WiFi No Connect Bitmap (16x16)
const uint8_t wifi_no_connect_bitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x0e, 0x30, 0x18, 0x18, 0x03, 0xc0, 
    0x04, 0x20, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Bluetooth Connect Bitmap (16x16)
const uint8_t bt_connect_bitmap[] PROGMEM = {
    0x00, 0x00, 0x01, 0x00, 0x01, 0x80, 0x01, 0x40, 0x01, 0x20, 0x05, 0x20, 0x03, 0x40, 0x01, 0x80, 
    0x01, 0x80, 0x03, 0x40, 0x05, 0x20, 0x01, 0x20, 0x01, 0x40, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00
};

// Bluetooth No Connect Bitmap (16x16)
const uint8_t bt_no_connect_bitmap[] PROGMEM = {
    0x00, 0x00, 0x01, 0x00, 0x01, 0x80, 0x01, 0x40, 0x01, 0x20, 0x05, 0x20, 0x03, 0x40, 0x01, 0x80, 
    0x01, 0x80, 0x03, 0x40, 0x05, 0x20, 0x01, 0x20, 0x01, 0x40, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00
};

// Battery Icon Bitmap (16x16)
const uint8_t battery_bitmap[] PROGMEM = {
    0x1f, 0xf8, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08,
    0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0x1f, 0xf8, 0x1f, 0xf8, 0x00, 0x00
};

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button pins
const int button1Pin = 25;
const int button2Pin = 26;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Health Monitor"); // Bluetooth device name
  Serial.println("The device started, now you can pair it with Bluetooth!");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x1F); // Turn Red LED to higher power
  particleSensor.setPulseAmplitudeIR(0x1F); // Turn IR LED to higher power
  particleSensor.setPulseAmplitudeGreen(0); // Turn off Green LED

  // Initialize accelerometer
  accelGyro.initialize();
  if (!accelGyro.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }

  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Initialize buttons
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
}

void loop() {
  irValue = particleSensor.getIR();
  redValue = particleSensor.getRed();

  if (checkForBeat(irValue) == true) {
    // We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
      rateSpot %= RATE_SIZE; // Wrap variable

      // Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  // Calculate SpO2
  float ratio = ((float)redValue / irValue);
  float R = ratio * ratio;
  SpO2 = 104 - 17 * R; // Simplified formula for SpO2 calculation

  // Read accelerometer values
  accelGyro.getAcceleration(&axRaw, &ayRaw, &azRaw);
  ax = (float)axRaw / 16384.0;
  ay = (float)ayRaw / 16384.0;
  az = (float)azRaw / 16384.0;
  resultantForce = sqrt(ax * ax + ay * ay + az * az);

  if (resultantForce > 3.0 && trauma == 0) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Are you Okay?");
    display.display();
    Serial.println("Are you Okay?");

    unsigned long startTime = millis();
    bool responseReceived = false;

    while (millis() - startTime < 5000) { // Wait for 5 seconds for a response
      if (digitalRead(button1Pin) == LOW) { // Yes button pressed
        Serial.println("Continuing monitoring...");
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Continue monitoring");
        display.display();
        delay(2000); // Display message for 2 seconds
        responseReceived = true;
        break;
      } else if (digitalRead(button2Pin) == LOW) { // No button pressed
        trauma = 1;
        Serial.println("Trauma detected, sending alert...");
        sendTraumaAlert(2000);
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Calling for help!");
        display.display();
        delay(2000); // Display message for 2 seconds
        responseReceived = true;
        break;
      }
    }

    if (!responseReceived) {
      trauma = 1;
      Serial.println("No response received, sending alert...");
      sendTraumaAlert(2000);
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Calling for help!");
      display.display();
      delay(2000); // Display message for 2 seconds
    }

    sendTraumaAlert(10000); // Send trauma alert during the 10 seconds delay
    trauma = 0; // Reset trauma status
  }

  // Display information on OLED
  display.clearDisplay();
  display.setCursor(0,0);
  display.println();
  display.println();
  display.print("BPM: ");
  display.println(beatsPerMinute);
  display.print("Avg BPM: ");
  display.println(beatAvg);
  display.print("SpO2: ");
  display.println(SpO2);
  display.print("Force: ");
  display.println(resultantForce);

  // Display WiFi and Bluetooth status
  bool wifiConnected = false; // Replace with actual condition
  bool btConnected = true; // Replace with actual condition
  int batteryLevel = 75; // Replace with actual battery level percentage

  display.setCursor(0, 0);
  if (wifiConnected) {
    displayWifiConnect();
  } else {
    displayWifiNoConnect();
  }

  if (btConnected) {
    displayBtConnect();
  } else {
    displayBtNoConnect();
  }

  displayBatteryLevel(batteryLevel);

  display.display();

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", Red=");
  Serial.print(redValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);
  Serial.print(", SpO2=");
  Serial.print(SpO2);
  Serial.print(", Force=");
  Serial.print(resultantForce);

  if (irValue < 50000)
    Serial.print(" No finger?");

  Serial.println();

  // Send data to Bluetooth every 0.2 seconds
  if (millis() - lastBluetoothSend >= 200) {
    SerialBT.print("Avg BPM=");
    SerialBT.print(beatAvg);
    SerialBT.print(", SpO2=");
    SerialBT.print(SpO2);
    SerialBT.print(", Force=");
    SerialBT.println(resultantForce);
    lastBluetoothSend = millis();
  }
}

void displayWifiConnect() {
  display.drawBitmap(0, 0, wifi_connect_bitmap, 16, 16, SSD1306_WHITE);
}

void displayWifiNoConnect() {
  display.drawBitmap(0, 0, wifi_no_connect_bitmap, 16, 16, SSD1306_WHITE);
  display.drawLine(0, 0, 15, 15, SSD1306_WHITE); // Diagonal line to cross out the icon
  display.drawLine(0, 15, 15, 0, SSD1306_WHITE); // Diagonal line to cross out the icon
}

void displayBtConnect() {
  display.drawBitmap(18, 0, bt_connect_bitmap, 16, 16, SSD1306_WHITE);
}

void displayBtNoConnect() {
  display.drawBitmap(18, 0, bt_no_connect_bitmap, 16, 16, SSD1306_WHITE);
  display.drawLine(18, 0, 33, 15, SSD1306_WHITE); // Diagonal line to cross out the icon
  display.drawLine(18, 15, 33, 0, SSD1306_WHITE); // Diagonal line to cross out the icon
}

void displayBatteryLevel(int batteryLevel) {
  display.setCursor(98, 0);
  display.print(batteryLevel);
  display.print("%");
  display.drawBitmap(112, 0, battery_bitmap, 16, 16, SSD1306_WHITE);
}

void sendTraumaAlert(unsigned long duration) {
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    SerialBT.println("trauma=1");
    delay(200); // Send the trauma alert every 0.2 seconds
  }
}
