#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <BluetoothSerial.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <MPU6050.h>

// WiFi credentials
const char* ssid = "Lego my Wygo";
const char* password = "$westgrain867$";

// OpenWeatherMap API details
const char* weatherApiKey = "38650cce7da795410159cc78beab200d";
const char* weatherCityID = "4469146"; // Replace with your actual city ID
const char* cityName = "Greensboro"; // Replace with your actual city name

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// GPIO pins for buttons
const int button1Pin = 25;
const int button2Pin = 26;
const int button3Pin = 27;
const int button4Pin = 14;

// Bluetooth Serial object
BluetoothSerial SerialBT;

// MAX30102 sensor
MAX30105 particleSensor;

// MPU6050 sensor
MPU6050 accelGyro;

// Define bitmap sizes
#define HEART_WIDTH 63
#define HEART_HEIGHT 64
#define WEATHER_WIDTH 16
#define WEATHER_HEIGHT 16

// Heart bitmaps
const uint8_t heartBitmaps[][HEART_WIDTH * HEART_HEIGHT / 8] PROGMEM = {
    // Add your heart bitmaps here
};

// Weather bitmaps
const uint8_t clear_sky_bitmap[] PROGMEM = {
    // Add your clear sky bitmap here
};

const uint8_t clouds_bitmap[] PROGMEM = {
    // Add your clouds bitmap here
};

const uint8_t rain_bitmap[] PROGMEM = {
    // Add your rain bitmap here
};

const uint8_t snow_bitmap[] PROGMEM = {
    // Add your snow bitmap here
};

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

// Snake game variables
int snakeX[100], snakeY[100];
int snakeLength;
int foodX, foodY;
int direction;
bool gameOver = false;
bool gameStarted = false;
bool showGameOver = false;
int score = 0;
int highScore = 0;
unsigned long lastMoveTime = 0;
int moveDelay = 150; // Initial move delay in milliseconds (increased base speed)

// Sensor variables
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
long redValue, irValue;
float SpO2;
int16_t axRaw, ayRaw, azRaw;
float ax, ay, az;
float resultantForce;
int trauma = 0;
unsigned long lastBluetoothSend = 0;

// Connection statuses
bool wifiConnected = false;
bool bluetoothConnected = false;

// State definitions
enum State {
    SHUT_DOWN,
    START_UP,
    START_UP_ANIMATION,
    MODE_1,
    MODE_2,
    SETTINGS,
    SETTINGS_WIFI,
    SETTINGS_BLUETOOTH,
    SETTINGS_FONT_SIZE,
    SETTINGS_POWER_OFF,
    SETTINGS_RETURN_MODE_1,
    EASTER_EGG_GAME,
    STAND_BY_MODE
};

State currentState = SHUT_DOWN;
unsigned long startUpTime = 0;
const unsigned long startUpDuration = 3000;
int settingsIndex = 0;
int fontSizeIndex = 0;
const int numberOfSettings = 6;
const int numberOfFontSizes = 2;
enum FontSize {
    SMALL = 1,
    DEFAULT_SIZE = 2
};
FontSize currentFontSize = SMALL;
bool isDefaultFontSize = false;
bool button1State = HIGH;
bool button2State = HIGH;
bool button3State = HIGH;
bool button4State = HIGH;
bool lastButton1State = HIGH;
bool lastButton2State = HIGH;
bool lastButton3State = HIGH;
bool lastButton4State = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
const unsigned long debounceDelay = 50;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 1000;
String weatherDescription = "";
float tempC = 0.0;
int mode1ScrollOffset = 0;
int mode2ScrollOffset = 0;
int settingsScrollOffset = 0;
int fontSizeScrollOffset = 0;
const int maxScrollOffset = 130;

void setup() {
    Serial.begin(115200);
    SerialBT.begin("ESP32_WeatherMonitor");

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.display();
    delay(2000);
    display.clearDisplay();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    wifiConnected = true;

    timeClient.begin();

    pinMode(button1Pin, INPUT_PULLUP);
    pinMode(button2Pin, INPUT_PULLUP);
    pinMode(button3Pin, INPUT_PULLUP);
    pinMode(button4Pin, INPUT_PULLUP);

    fetchWeatherData();

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 was not found. Please check wiring/power.");
        while (1);
    }
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);
    particleSensor.setPulseAmplitudeGreen(0);

    accelGyro.initialize();
    if (!accelGyro.testConnection()) {
        Serial.println("MPU6050 connection failed");
        while (1);
    }
}

void loop() {
    int reading1 = digitalRead(button1Pin);
    int reading2 = digitalRead(button2Pin);
    int reading3 = digitalRead(button3Pin);
    int reading4 = digitalRead(button4Pin);

    if (reading1 != lastButton1State) {
        lastDebounceTime1 = millis();
    }
    if ((millis() - lastDebounceTime1) > debounceDelay) {
        if (reading1 != button1State) {
            button1State = reading1;
            if (button1State == LOW) {
                handleButton1Press();
            }
        }
    }
    lastButton1State = reading1;

    if (reading2 != lastButton2State) {
        lastDebounceTime2 = millis();
    }
    if ((millis() - lastDebounceTime2) > debounceDelay) {
        if (reading2 != button2State) {
            button2State = reading2;
            if (button2State == LOW) {
                handleButton2Press();
            }
        }
    }
    lastButton2State = reading2;

    if (reading3 != lastButton3State) {
        lastDebounceTime3 = millis();
    }
    if ((millis() - lastDebounceTime3) > debounceDelay) {
        if (reading3 != button3State) {
            button3State = reading3;
            if (button3State == LOW) {
                handleButton3Press();
            }
        }
    }
    lastButton3State = reading3;

    if (reading4 != lastButton4State) {
        lastDebounceTime4 = millis();
    }
    if ((millis() - lastDebounceTime4) > debounceDelay) {
        if (reading4 != button4State) {
            button4State = reading4;
            if (button4State == LOW) {
                handleButton4Press();
            }
        }
    }
    lastButton4State = reading4;

    if (SerialBT.available()) {
        String receivedData = SerialBT.readString();
        Serial.print("Received via Bluetooth: ");
        Serial.println(receivedData);
        // Process received data
        processBluetoothData(receivedData);
    }

    switch (currentState) {
        case SHUT_DOWN:
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor((SCREEN_WIDTH - 6 * 12) / 2, (SCREEN_HEIGHT - 8) / 2);
            display.println(F("Standby mode"));
            display.display();
            break;

        case START_UP:
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor((SCREEN_WIDTH - 6 * 11) / 2, (SCREEN_HEIGHT - 8) / 2);
            display.println(F("Booting up..."));
            display.display();
            delay(2000);
            startUpTime = millis();
            currentState = START_UP_ANIMATION;
            break;

        case START_UP_ANIMATION:
            if (millis() - startUpTime < startUpDuration) {
                displayAnimation();
            } else {
                currentState = MODE_1;
            }
            break;

        case MODE_1:
            if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
                updateModeDisplay();
                lastDisplayUpdate = millis();
            }

            if (millis() - lastWeatherUpdate >= weatherUpdateInterval) {
                fetchWeatherData();
                lastWeatherUpdate = millis();
            }
            break;

        case MODE_2:
            if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
                updateMode2Display();
                lastDisplayUpdate = millis();
            }
            break;

        case SETTINGS:
            display.clearDisplay();
            display.setTextSize(currentFontSize);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0 - settingsScrollOffset);
            display.println(F("Settings"));
            displayMenuItem(0, F("1. WiFi"));
            displayMenuItem(1, F("2. Bluetooth"));
            displayMenuItem(2, F("3. Font Size"));
            displayMenuItem(3, F("4. Easter Egg Game"));
            displayMenuItem(4, F("5. Return to Mode 1"));
            displayMenuItem(5, F("6. Stand by Mode"));
            display.display();
            break;

        case SETTINGS_WIFI:
            display.clearDisplay();
            display.setTextSize(currentFontSize);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println(F("WiFi Settings"));
            display.display();
            break;

        case SETTINGS_BLUETOOTH:
            display.clearDisplay();
            display.setTextSize(currentFontSize);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println(F("Bluetooth Settings"));
            display.display();
            handleBluetoothSettings();
            break;

        case SETTINGS_FONT_SIZE:
            updateFontSizeSettingsDisplay();
            break;

        case SETTINGS_RETURN_MODE_1:
            currentState = MODE_1;
            break;

        case STAND_BY_MODE:
            currentState = SHUT_DOWN;
            break;

        case EASTER_EGG_GAME:
            if (!gameStarted) {
                resetGame();
                displayStartScreen();
            }

            if (gameOver && !showGameOver) {
                showGameOver = true;
                if (score > highScore) {
                    highScore = score;
                }
                displayGameOverScreen();
                delay(2000); // Display "Game Over" for 2 seconds
                currentState = SETTINGS; // Return to settings after game over
                break;
            }

            // Handle button presses with debouncing
            static unsigned long lastButtonPressTime = 0;
            if (millis() - lastButtonPressTime > 150) { // 150ms debounce time
                if (digitalRead(button1Pin) == LOW && direction != 1) {
                    direction = 0;
                    lastButtonPressTime = millis();
                }
                if (digitalRead(button2Pin) == LOW && direction != 0) {
                    direction = 1;
                    lastButtonPressTime = millis();
                }
                if (digitalRead(button3Pin) == LOW && direction != 3) {
                    direction = 2;
                    lastButtonPressTime = millis();
                }
                if (digitalRead(button4Pin) == LOW && direction != 2) {
                    direction = 3;
                    lastButtonPressTime = millis();
                }
            }

            // Move the snake
            if (millis() - lastMoveTime > moveDelay) {
                lastMoveTime = millis();
                moveSnake();
                checkCollisions();
                updateDisplay();
            }
            break;
    }
}

void updateModeDisplay() {
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setTextColor(SSD1306_WHITE);

    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    setTime(epochTime);

    int currentHour = hour();
    int currentMinute = minute();
    int currentSecond = second();
    int currentDay = day();
    int currentMonth = month();
    int currentYear = year();
    int currentWeekday = weekday();

    int timezoneOffset = -5;
    if (isDST(currentDay, currentMonth, currentHour, currentWeekday)) {
        timezoneOffset = -4;
    }

    currentHour += timezoneOffset;
    if (currentHour < 0) {
        currentHour += 24;
        currentDay -= 1;
    } else if (currentHour >= 24) {
        currentHour -= 24;
        currentDay += 1;
    }

    char timeString[6];
    snprintf(timeString, sizeof(timeString), "%02d:%02d", currentHour, currentMinute);

    char dateString[11];
    snprintf(dateString, sizeof(dateString), "%02d/%02d/%04d", currentDay, currentMonth, currentYear);

    display.setCursor(0, 0 - mode1ScrollOffset);
    display.print(timeString);

    display.setTextSize(currentFontSize);
    display.setCursor(68, 0 - mode1ScrollOffset);
    display.print(dateString);

    int lineHeight = isDefaultFontSize ? 16 : 10;

    display.setCursor(0, lineHeight * 2 - mode1ScrollOffset);
    display.print("Temp: ");
    display.print(tempC, 1);
    display.println(" C");

    if (isDefaultFontSize) {
        display.setCursor(0, lineHeight * 4 - mode1ScrollOffset);
        display.print("Weather:");
        display.setCursor(0, lineHeight * 6 - mode1ScrollOffset);
        display.println(weatherDescription);
        display.setCursor(0, lineHeight * 8 - mode1ScrollOffset);
        display.print(cityName);
    } else {
        display.setCursor(0, lineHeight * 3 - mode1ScrollOffset);
        display.print("Weather:");
        display.setCursor(0, lineHeight * 4 - mode1ScrollOffset);
        display.println(weatherDescription);
        display.setCursor(0, lineHeight * 5 - mode1ScrollOffset);
        display.print(cityName);
    }

    int bitmapY = lineHeight * (isDefaultFontSize ? 4 : 3) - mode1ScrollOffset;
    if (weatherDescription == "clear sky") {
        display.drawBitmap(100, bitmapY, clear_sky_bitmap, WEATHER_WIDTH, WEATHER_HEIGHT, SSD1306_WHITE);
    } else if (weatherDescription.indexOf("cloud") >= 0) {
        display.drawBitmap(100, bitmapY, clouds_bitmap, WEATHER_WIDTH, WEATHER_HEIGHT, SSD1306_WHITE);
    } else if (weatherDescription.indexOf("rain") >= 0) {
        display.drawBitmap(100, bitmapY, rain_bitmap, WEATHER_WIDTH, WEATHER_HEIGHT, SSD1306_WHITE);
    } else if (weatherDescription.indexOf("snow") >= 0) {
        display.drawBitmap(100, bitmapY, snow_bitmap, WEATHER_WIDTH, WEATHER_HEIGHT, SSD1306_WHITE);
    }

    display.display();
}

void updateMode2Display() {
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setTextColor(SSD1306_WHITE);

    irValue = particleSensor.getIR();
    redValue = particleSensor.getRed();

    if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);

        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute;
            rateSpot %= RATE_SIZE;
            beatAvg = 0;
            for (byte x = 0 ; x < RATE_SIZE ; x++)
                beatAvg += rates[x];
            beatAvg /= RATE_SIZE;
        }
    }

    float ratio = ((float)redValue / irValue);
    float R = ratio * ratio;
    SpO2 = 104 - 17 * R;

    accelGyro.getAcceleration(&axRaw, &ayRaw, &azRaw);
    ax = (float)axRaw / 16384.0;
    ay = (float)ayRaw / 16384.0;
    az = (float)azRaw / 16384.0;
    resultantForce = sqrt(ax * ax + ay * ay + az * az);

    if (resultantForce > 3.0 && trauma == 0) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Are you Okay?");
        display.display();
        Serial.println("Are you Okay?");

        unsigned long startTime = millis();
        bool responseReceived = false;

        while (millis() - startTime < 5000) {
            if (digitalRead(button1Pin) == LOW) {
                Serial.println("Continuing monitoring...");
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("Continue monitoring");
                display.display();
                delay(2000);
                responseReceived = true;
                break;
            } else if (digitalRead(button2Pin) == LOW) {
                trauma = 1;
                Serial.println("Trauma detected, sending alert...");
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("Calling for help!");
                display.display();
                delay(2000);
                responseReceived = true;
                break;
            }
        }

        if (!responseReceived) {
            trauma = 1;
            Serial.println("No response received, sending alert...");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Calling for help!");
            display.display();
            delay(2000);
        }

        delay(10000);
        trauma = 0;
    }

    display.clearDisplay();
    display.setCursor(0, 16 - mode2ScrollOffset); // Move readings down by 16 pixels
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

    display.setCursor(0, 0);
    if (wifiConnected) {
        displayWifiConnect();
    } else {
        displayWifiNoConnect();
    }

    if (bluetoothConnected) {
        displayBtConnect();
    } else {
        displayBtNoConnect();
    }

    displayBatteryLevel(75); // Example battery level, replace with actual level

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

void fetchWeatherData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?id=" + String(weatherCityID) + "&appid=" + String(weatherApiKey);
        http.begin(weatherApiUrl);
        int httpCode = http.GET();
        if (httpCode > 0) {
            String payload = http.getString();
            Serial.print("Weather API Response: ");
            Serial.println(payload);

            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.f_str());
                return;
            }

            if (doc.containsKey("main") && doc["main"].containsKey("temp")) {
                float tempK = doc["main"]["temp"];
                tempC = tempK - 273.15;
            } else {
                Serial.println("Temperature data not found in response.");
            }

            if (doc.containsKey("weather") && doc["weather"][0].containsKey("description")) {
                weatherDescription = String(doc["weather"][0]["description"].as<const char*>());
            } else {
                Serial.println("Weather description not found in response.");
            }

            Serial.println("Weather data fetched successfully");
            Serial.print("Temperature: ");
            Serial.println(tempC);
            Serial.print("Description: ");
            Serial.println(weatherDescription);
        } else {
            Serial.print("Failed to fetch weather data. HTTP code: ");
            Serial.println(httpCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}

void handleButton1Press() {
    if (currentState == MODE_1) {
        mode1ScrollOffset = max(0, mode1ScrollOffset - 10);
        updateModeDisplay();
        Serial.println("Button 1 pressed, scrolling up in mode 1");
    } else if (currentState == MODE_2) {
        mode2ScrollOffset = max(0, mode2ScrollOffset - 10);
        updateMode2Display();
        Serial.println("Button 1 pressed, scrolling up in mode 2");
    } else if (currentState == SETTINGS) {
        if (settingsIndex == 0) {
            currentState = SETTINGS_WIFI;
        } else if (settingsIndex == 1) {
            currentState = SETTINGS_BLUETOOTH;
        } else if (settingsIndex == 2) {
            currentState = SETTINGS_FONT_SIZE;
        } else if (settingsIndex == 3) {
            currentState = EASTER_EGG_GAME;
        } else if (settingsIndex == 4) {
            currentState = SETTINGS_RETURN_MODE_1;
        } else if (settingsIndex == 5) {
            currentState = STAND_BY_MODE;
        }
        Serial.print("Button 1 Pressed, current state: ");
        Serial.println(currentState);
    } else if (currentState == SETTINGS_FONT_SIZE) {
        if (fontSizeIndex == 0) {
            currentFontSize = SMALL;
            isDefaultFontSize = false;
        } else if (fontSizeIndex == 1) {
            currentFontSize = DEFAULT_SIZE;
            isDefaultFontSize = true;
        }
        currentState = SETTINGS;
    } else if (currentState == SHUT_DOWN) {
        currentState = START_UP;
        Serial.println("Button 1 long press detected, transitioning to START_UP");
    }
}

void handleButton2Press() {
    if (currentState == MODE_1) {
        mode1ScrollOffset = min(mode1ScrollOffset + 10, maxScrollOffset);
        updateModeDisplay();
        Serial.println("Button 2 pressed, scrolling down in mode 1");
    } else if (currentState == MODE_2) {
        mode2ScrollOffset = min(mode2ScrollOffset + 10, maxScrollOffset);
        updateMode2Display();
        Serial.println("Button 2 pressed, scrolling down in mode 2");
    }
}

void handleButton3Press() {
    switch (currentState) {
        case MODE_1:
            currentState = MODE_2;
            break;
        case MODE_2:
            currentState = MODE_1;
            break;
        case SETTINGS:
            settingsIndex = (settingsIndex - 1 + numberOfSettings) % numberOfSettings;
            settingsScrollOffset = settingsIndex * (isDefaultFontSize ? 20 : 10) * currentFontSize;
            updateSettingsDisplay();
            break;
        case SETTINGS_FONT_SIZE:
            fontSizeIndex = (fontSizeIndex - 1 + numberOfFontSizes) % numberOfFontSizes;
            fontSizeScrollOffset = fontSizeIndex * (isDefaultFontSize ? 20 : 10) * currentFontSize;
            updateFontSizeSettingsDisplay();
            break;
        case EASTER_EGG_GAME:
            break;
    }
    Serial.print("Button 3 Pressed, current state: ");
    Serial.println(currentState);
}

void handleButton4Press() {
    if (currentState == MODE_1 || currentState == MODE_2) {
        currentState = SETTINGS;
        Serial.println("Button 4 pressed, transitioning to SETTINGS");
    } else if (currentState == SETTINGS) {
        settingsIndex = (settingsIndex + 1) % numberOfSettings;
        settingsScrollOffset = settingsIndex * (isDefaultFontSize ? 20 : 10) * currentFontSize;
        updateSettingsDisplay();
        Serial.print("Button 4 pressed, settingsIndex: ");
        Serial.println(settingsIndex);
    } else if (currentState == SETTINGS_FONT_SIZE) {
        fontSizeIndex = (fontSizeIndex + 1) % numberOfFontSizes;
        fontSizeScrollOffset = fontSizeIndex * (isDefaultFontSize ? 20 : 10) * currentFontSize;
        updateFontSizeSettingsDisplay();
    } else if (currentState == EASTER_EGG_GAME) {
        Serial.println("Button 4 pressed, in Easter Egg Game");
    }
}

void displayMenuItem(int index, const __FlashStringHelper* text) {
    int cursorY = 10 * currentFontSize + index * (isDefaultFontSize ? 20 : 10) * currentFontSize - settingsScrollOffset;
    display.setCursor(0, cursorY);
    display.print(text);
    if (settingsIndex == index) {
        display.print(F(" <"));
    }
}

void updateSettingsDisplay() {
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0 - settingsScrollOffset);
    display.println(F("Settings"));
    displayMenuItem(0, F("1. WiFi"));
    displayMenuItem(1, F("2. Bluetooth"));
    displayMenuItem(2, F("3. Font Size"));
    displayMenuItem(3, F("4. Easter Egg Game"));
    displayMenuItem(4, F("5. Return to Mode 1"));
    displayMenuItem(5, F("6. Stand by Mode"));
    display.display();
}

void updateFontSizeSettingsDisplay() {
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0 - fontSizeScrollOffset);
    display.println(F("Font Size Settings"));
    display.setCursor(0, 20 * currentFontSize - fontSizeScrollOffset);
    display.print(F("1. Small"));
    if (fontSizeIndex == 0) display.print(F(" <"));

    display.setCursor(0, 40 * currentFontSize - fontSizeScrollOffset);
    display.print(F("2. Default"));
    if (fontSizeIndex == 1) display.print(F(" <"));

    display.display();
}

void handleBluetoothSettings() {
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setCursor(0, 0);
    if (bluetoothConnected) {
        display.println(F("Disconnect BT?"));
    } else {
        display.println(F("Connect to BT?"));
    }
    display.display();

    bool connectBT = promptYesNo();
    if (connectBT) {
        if (bluetoothConnected) {
            SerialBT.end();
            bluetoothConnected = false;
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println(F("BT Disconnected"));
            display.display();
            delay(2000);
        } else {
            connectToBluetooth();
        }
    }
    currentState = SETTINGS;
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setCursor(0, 0);
    display.println(F("Bluetooth Settings"));
    display.display();
}

void connectToBluetooth() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Connecting to BT..."));
    display.display();

    bluetoothConnected = SerialBT.hasClient();

    if (bluetoothConnected) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print(F("BT Connected"));
        display.display();
        delay(5000);
    } else {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print(F("BT Connect Failed"));
        display.display();
        delay(2000);
        displayTryAgainBT();
    }
}

void displayTryAgainBT() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Try BT again?"));
    display.display();

    bool tryAgain = promptYesNo();
    if (tryAgain) {
        connectToBluetooth();
    }
    currentState = SETTINGS;
    display.clearDisplay();
    display.setTextSize(currentFontSize);
    display.setCursor(0, 0);
    display.println(F("Bluetooth Settings"));
    display.display();
}

bool promptYesNo() {
    while (true) {
        bool button1State = digitalRead(button1Pin);
        bool button2State = digitalRead(button2Pin);

        if (button1State == LOW && lastButton1State == HIGH) {
            lastButton1State = button1State;
            lastButton2State = button2State;
            return true;
        }
        if (button2State == LOW && lastButton2State == HIGH) {
            lastButton1State = button1State;
            lastButton2State = button2State;
            return false;
        }

        lastButton1State = button1State;
        lastButton2State = button2State;
        delay(10);
    }
}

bool isDST(int day, int month, int hour, int wday) {
    if (month > 3 && month < 11) return true;
    if (month < 3 || month > 11) return false;

    int previousSunday = day - wday;
    if (month == 3) return previousSunday >= 8;
    return previousSunday < 1;
}

void displayAnimation() {
    int x = (SCREEN_WIDTH - 63) / 2;
    int y = (SCREEN_HEIGHT - 64) / 2;

    for (int i = 0; i < sizeof(heartBitmaps) / sizeof(heartBitmaps[0]); i++) {
        display.clearDisplay();
        display.drawBitmap(x, y, heartBitmaps[i], HEART_WIDTH, HEART_HEIGHT, SSD1306_WHITE);
        display.display();
        delay(200);
    }
}

void displayWifiConnect() {
    display.drawBitmap(0, 0, wifi_connect_bitmap, 16, 16, SSD1306_WHITE);
}

void displayWifiNoConnect() {
    display.drawBitmap(0, 0, wifi_no_connect_bitmap, 16, 16, SSD1306_WHITE);
    display.drawLine(0, 0, 15, 15, SSD1306_WHITE);
    display.drawLine(0, 15, 15, 0, SSD1306_WHITE);
}

void displayBtConnect() {
    display.drawBitmap(18, 0, bt_connect_bitmap, 16, 16, SSD1306_WHITE);
}

void displayBtNoConnect() {
    display.drawBitmap(18, 0, bt_no_connect_bitmap, 16, 16, SSD1306_WHITE);
    display.drawLine(18, 0, 33, 15, SSD1306_WHITE);
    display.drawLine(18, 15, 33, 0, SSD1306_WHITE);
}

void displayBatteryLevel(int batteryLevel) {
    display.setCursor(98, 0);
    display.print(batteryLevel);
    display.print("%");
    display.drawBitmap(112, 0, battery_bitmap, 16, 16, SSD1306_WHITE);
}

// Snake Game Functions
void spawnFood() {
    foodX = random(2, (SCREEN_WIDTH - 2) / 2) * 2;
    foodY = random(2, (SCREEN_HEIGHT - 2) / 2) * 2;
}

void moveSnake() {
    for (int i = snakeLength - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }

    if (direction == 0) snakeY[0] -= 2;
    if (direction == 1) snakeY[0] += 2;
    if (direction == 2) snakeX[0] -= 2;
    if (direction == 3) snakeX[0] += 2;

    // Check for food collision
    if (snakeX[0] == foodX && snakeY[0] == foodY) {
        snakeLength++;
        score++;
        moveDelay = max(50, moveDelay - 5); // Increase speed with a minimum delay of 50ms
        spawnFood();
    }
}

void checkCollisions() {
    // Check wall collisions
    if (snakeX[0] < 0 || snakeX[0] >= SCREEN_WIDTH || snakeY[0] < 0 || snakeY[0] >= SCREEN_HEIGHT) {
        gameOver = true;
    }

    // Check self collisions
    for (int i = 1; i < snakeLength; i++) {
        if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
            gameOver = true;
        }
    }
}

void updateDisplay() {
    display.clearDisplay();

    // Draw borders
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

    // Draw snake
    for (int i = 0; i < snakeLength; i++) {
        if (i == 0) {
            // Draw the snake head with eyes
            display.fillRect(snakeX[i], snakeY[i], 4, 4, SSD1306_WHITE);
            // Draw eyes
            if (direction == 0) { // Up
                display.drawPixel(snakeX[i] + 1, snakeY[i] + 1, SSD1306_BLACK);
                display.drawPixel(snakeX[i] + 2, snakeY[i] + 1, SSD1306_BLACK);
            } else if (direction == 1) { // Down
                display.drawPixel(snakeX[i] + 1, snakeY[i] + 2, SSD1306_BLACK);
                display.drawPixel(snakeX[i] + 2, snakeY[i] + 2, SSD1306_BLACK);
            } else if (direction == 2) { // Left
                display.drawPixel(snakeX[i] + 1, snakeY[i] + 1, SSD1306_BLACK);
                display.drawPixel(snakeX[i] + 1, snakeY[i] + 2, SSD1306_BLACK);
            } else if (direction == 3) { // Right
                display.drawPixel(snakeX[i] + 2, snakeY[i] + 1, SSD1306_BLACK);
                display.drawPixel(snakeX[i] + 2, snakeY[i] + 2, SSD1306_BLACK);
            }
        } else {
            // Draw the snake body
            display.fillRect(snakeX[i], snakeY[i], 4, 4, SSD1306_WHITE);
        }
    }

    // Draw food
    display.fillRect(foodX, foodY, 4, 4, SSD1306_WHITE);

    // Display score with border
    display.drawRect(0, 0, 50, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, 2);
    display.print("Score: ");
    display.print(score);

    display.display();
}

void displayStartScreen() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int textWidth = 12 * 6; // Approximate width of "Snake" in pixels
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 20); // Centered position and lowered to prevent bleeding
    display.print("Snake");

    display.setTextSize(1);
    const char* startMessage1 = "Press UP + DOWN";
    textWidth = strlen(startMessage1) * 6; // Approximate width of the text
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 40);
    display.print(startMessage1);

    const char* startMessage2 = "to start";
    textWidth = strlen(startMessage2) * 6; // Approximate width of the text
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 50);
    display.print(startMessage2);

    const char* highScoreMessage = "High Score: ";
    textWidth = strlen(highScoreMessage) * 6 + 6 * 2; // Approximate width of the text + space for the score
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 0);
    display.print(highScoreMessage);
    display.print(highScore);

    display.display();
}

void displayGameOverScreen() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    const char* gameOverMessage = "Game Over";
    int textWidth = strlen(gameOverMessage) * 12; // Approximate width of "Game Over" in pixels
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 20); // Centered position
    display.print(gameOverMessage);
    display.display();
}

void processBluetoothData(String data) {
    // Add your logic to process received Bluetooth data
}

void resetGame() {
    gameStarted = true;
    score = 0;
    gameOver = false;
    showGameOver = false;
    snakeLength = 3;
    // Start snake in the middle of the screen
    int startX = SCREEN_WIDTH / 2;
    int startY = SCREEN_HEIGHT / 2;
    snakeX[0] = startX; snakeY[0] = startY;
    snakeX[1] = startX - 2; snakeY[1] = startY;
    snakeX[2] = startX - 4; snakeY[2] = startY;
    direction = 3; // Start direction to the right
    moveDelay = 150; // Reset move delay
    spawnFood();
}
