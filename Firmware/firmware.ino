#include <WiFi.h>                   // WiFi Library
#include <time.h>                   // RTC time Library
#include <MAX30105.h>               // MAX30100 library
#include <OneWire.h>                // OneWire Library
#include <DallasTemperature.h>      // Temperature sensor Library
#include <MPU6050.h>                // MPU6050 Library
#include <SPIFFS.h>                 // Data storage Library
#include <heartRate.h>              // Heart rate functions
#include <spo2_algorithm.h>         // Calculation algorithm
#include <WiFiManager.h>            // Makes adaptable WiFi setup

// Pin Definitions
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 20
#define ONE_WIRE_BUS 10
#define VIBRATION_MOTOR_PIN 3

// MAX30105, MPU6050, and DS18B20 setup
MAX30105 pox;                          // Initialize MAX30105 object
OneWire oneWire(ONE_WIRE_BUS);         // Setup OneWire connection
DallasTemperature sensors(&oneWire);   // Initialize DS18B20 object
MPU6050 mpu;                           // Initialize MPU6050 object

// Sleep time setup
const uint64_t SLEEP_INTERVAL = 20 * 1000000LL;  // Sleep for 20 seconds

// Variables
bool isMorning = false;
bool wifiInitialized = false;
const int MORNING_HOUR = 6;  // Set the hour for morning (e.g., 6:00 AM)

// Function prototypes
void initializeSensors();           // Initialize sensors
bool initializeWiFi();              // Sets up WiFi, returns true on success
void configureTime();               // Syncs time from NTP servers
void sendData();                    // Sends the data to the server
void collectAndLogData();           // Collect and log data from sensors
void correctPosture();              // Posture correction based on MPU-6050
void prepareForDeepSleep();         // Prepares for deep sleep

void setup() {
    Serial.begin(115200);

    // Initialize file system for data logging
    if (!SPIFFS.begin(true)) {
        Serial.println(F("Failed to mount file system"));
        return;
    }

    // Initialize I2C and sensors
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    initializeSensors();

    // Initialize WiFi only once for time sync
    if (!wifiInitialized) {
        if (initializeWiFi()) {
            configureTime();   // Sync time with NTP server
            WiFi.disconnect(); // Disconnect WiFi to save power
            wifiInitialized = true;
        } else {
            Serial.println(F("WiFi initialization failed"));
        }
    }

    // Check if it's morning and send data
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        // Check if it's morning (after 6:00 AM)
        if (timeinfo.tm_hour >= MORNING_HOUR) {
            isMorning = true;
        }

        if (isMorning) {
            if (initializeWiFi()) {
                sendData();
                isMorning = false;  // Reset the flag after sending data
            }
        }
    } else {
        Serial.println(F("Failed to get time from RTC"));
    }

    // Collect sensor data and correct posture
    collectAndLogData();
    correctPosture();

    // Prepare the ESP for deep sleep
    prepareForDeepSleep();
}

void initializeSensors() {
    // Initialize MAX30105 sensor
    if (!pox.begin()) {
        Serial.println(F("Failed to initialize MAX30105"));
        return;
    }
    pox.setPulseAmplitudeRed(0x0F);  // 7.6 mA for Red LED
    pox.setPulseAmplitudeIR(0x0F);   // 7.6 mA for IR LED
    pox.setSampleRate(MAX30105_SAMPLERATE_50);
    pox.setPulseWidth(MAX30105_PULSEWIDTH_118);

    // Initialize DS18B20 temperature sensor
    sensors.begin();

    // Initialize MPU6050 for posture correction
    mpu.initialize();
    if (!mpu.testConnection()) {
        Serial.println(F("Failed to initialize MPU6050"));
    }
}

bool initializeWiFi() {
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    bool connected = wm.autoConnect("Sleep Portal");
    if (!connected) {
        Serial.println(F("WiFi connection failed"));
        return false;
    }
    Serial.println(F("WiFi connected"));
    return true;
}

void configureTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println(F("Failed to sync time with NTP"));
    } else {
        Serial.println(&timeinfo, "Time synchronized: %A, %B %d %Y %H:%M:%S");
    }
}

void sendData() {
    Serial.println(F("Sending data..."));

    // Open the log file and send data
    File logFile = SPIFFS.open("/data_log.txt", "r");
    if (!logFile) {
        Serial.println(F("Failed to open log file"));
        return;
    }

    while (logFile.available()) {
        String line = logFile.readStringUntil('\n');
        Serial.println(line);  // Replace with sending data to server
    }

    logFile.close();
    SPIFFS.remove("/data_log.txt");  // Clear log after sending
}

void collectAndLogData() {
    const int BUFFER_SIZE = 100;
    uint32_t irBuffer[BUFFER_SIZE], redBuffer[BUFFER_SIZE];
    int32_t heartRate = 0, spo2 = 0;
    int8_t validHeartRate = 0, validSpO2 = 0;

    // Collect data from MAX30105 sensor
    for (int i = 0; i < BUFFER_SIZE; i++) {
        irBuffer[i] = pox.getIR();
        redBuffer[i] = pox.getRed();
        delay(10);
    }
    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSpO2, &heartRate, &validHeartRate);

    // Collect temperature data
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);

    // Log data to SPIFFS (append mode)
    File logFile = SPIFFS.open("/data_log.txt", "a");
    if (!logFile) {
        Serial.println(F("Failed to open log file for writing"));
        return;
    }

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        logFile.printf("%02d:%02d:%02d,%.2f,%.2f,%.2f\n",
                       timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                       heartRate, spo2, temperature);
        Serial.printf("Logged: %02d:%02d:%02d, HR: %.2f, SpO2: %.2f, Temp: %.2f\n",
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                      heartRate, spo2, temperature);
    }
    logFile.close();
}

void correctPosture() {
    // Check posture using MPU6050
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    // Basic tilt detection for posture correction
    if (abs(ax) > 15000 || abs(ay) > 15000) {
        // Trigger vibration motor for correction
        digitalWrite(VIBRATION_MOTOR_PIN, HIGH);
        delay(500);
        digitalWrite(VIBRATION_MOTOR_PIN, LOW);
    }
}

void prepareForDeepSleep() {
    Serial.println(F("Entering deep sleep..."));
    esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL);  // Sleep for 20 seconds
    esp_deep_sleep_start();  // Enter deep sleep
}

void loop() {
    // Not used; ESP will go into deep sleep after setup
}


