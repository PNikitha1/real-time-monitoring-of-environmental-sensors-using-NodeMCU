/*
  NodeMCU (ESP8266) with Temperature and Pressure Sensors
  --------------------------------------------------------
  Connections:
  
  Temperature Sensor:
    + (VCC)   -> 3V3
    G (GND)   -> GND
    SCL       -> D1
    SDA       -> D2
    
  Pressure Sensor:
    VCC       -> 3V3
    GND       -> GND
    SCK       -> D5
    OUT       -> D6
  Water Level Sensor:
    +         ->3V3
    -         ->GND
    S         ->A0
*/
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>  // Include WiFiClientSecure for HTTPS
#include <Wire.h>
#include <Adafruit_BMP085.h> // Include BMP180 library
#include <ThingSpeak.h> // Include ThingSpeak library

#define WIFI_SSID "GITAM"
#define WIFI_PASSWORD "Gitam$$123"

// Pushover credentials
const char* pushoverUserKey = "uxf7n97khb7fzeffnuybdof9k6nzce";  // Replace with your Pushover User Key
const char* pushoverApiToken = "a5biqgdnfv5zqp8jugi5n47qvkanzc";  // Replace with your Pushover API Token

// ThingSpeak credentials
unsigned long channelID = 2697558; // Replace with your ThingSpeak Channel ID
const char* apiKey = "GESMLLIN6X0MLX18"; // Replace with your ThingSpeak Write API Key

// Create an instance of the BMP180 sensor
Adafruit_BMP085 bmp;

unsigned long lastSensorCheck = 0;
const unsigned long sensorInterval = 15000;  // Check every 15 seconds

WiFiClientSecure clientSecure; // Use WiFiClientSecure for HTTPS
WiFiClient client;  // Regular WiFi client for ThingSpeak

#define PRESSURE_SCK_PIN  D5   // Serial Clock for pressure sensor
#define PRESSURE_OUT_PIN  D6   // Data Output from pressure sensor
#define WATER_LEVEL_PIN A0 

void setup() {
  Serial.begin(115200);

  // Initialize BMP180 sensor
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP180 sensor, check wiring!");
    while (1);
  }
  
  pinMode(PRESSURE_SCK_PIN, OUTPUT);
  digitalWrite(PRESSURE_SCK_PIN, LOW); // Ensure SCK starts LOW
  pinMode(PRESSURE_OUT_PIN, INPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");

  // Initialize ThingSpeak
  ThingSpeak.begin(client);
  
  // Set time for SSL/TLS connections (required for secure client)
  clientSecure.setInsecure();  // Bypass SSL certificate validation for simplicity, should use certificate in production
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle sensor checks and notifications
  if (currentMillis - lastSensorCheck >= sensorInterval) {
    lastSensorCheck = currentMillis;

    float temperature = bmp.readTemperature();
    float pressure = readPressureSensor();
    int waterLevelValue = analogRead(WATER_LEVEL_PIN);

    // Display sensor values in the serial monitor
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" hPa");

    Serial.print("Water Level: ");
    Serial.println(waterLevelValue);

    // Check conditions and send Pushover notifications
    handleNotifications(temperature, pressure, waterLevelValue);

    // Send data to ThingSpeak
    sendDataToThingSpeak(temperature, pressure, waterLevelValue);
  }
}

// Send data to ThingSpeak
void sendDataToThingSpeak(float temperature, float pressure, int waterLevel) {
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, pressure);
  ThingSpeak.setField(3, waterLevel);

  // Write to the ThingSpeak channel
  int responseCode = ThingSpeak.writeFields(channelID, apiKey);
  if (responseCode == 200) {
    Serial.println("Data sent to ThingSpeak successfully!");
  } else {
    Serial.print("Error sending data: ");
    Serial.println(responseCode);
  }
}

// Read pressure sensor (similar to how you've done it)
float readPressureSensor() {
  const int NUM_BITS = 16;
  unsigned long rawData = 0;

  for (int i = 0; i < NUM_BITS; i++) {
    digitalWrite(PRESSURE_SCK_PIN, HIGH);
    delayMicroseconds(10);
    int bit = digitalRead(PRESSURE_OUT_PIN);
    rawData = (rawData << 1) | bit;
    digitalWrite(PRESSURE_SCK_PIN, LOW);
    delayMicroseconds(10);
  }

  float pressure = convertRawToPressure(rawData);
  return pressure;
}

float convertRawToPressure(unsigned long raw) {
  float pressure = 300.0 + ((float)raw / 65535.0) * (1100.0 - 300.0);
  return pressure;
}

// Handle Pushover notifications based on sensor values
void handleNotifications(float temperature, float pressure, int waterLevel) {
  if (temperature > 33.0) {
    sendPushoverNotification("Temperature is too high - Kindly Turn ON AC", "Temperature Alert");
  } else if (temperature < 10.0) {
    sendPushoverNotification("Temperature is too low - Kindly Turn off AC", "Temperature Alert");
  }

  if (pressure > 1000) {
    sendPushoverNotification("Pressure is too high - Kindly shut down the geyser", "Pressure Alert");
  } else if (pressure < 400) {
    sendPushoverNotification("Pressure is too low - Kindly stop heating the water", "Pressure Alert");
  }

  if (waterLevel > 900) {
    sendPushoverNotification("Water Tank is Almost Full - Kindly Turn Off the Motor", "Water Level Alert");
  } else if (waterLevel < 200) {
    sendPushoverNotification("Water is Low - Kindly Turn ON the Motor", "Water Level Alert");
  }
}

// Function to send Pushover notifications using HTTPS
void sendPushoverNotification(String message, String title) {
  HTTPClient http;

  String postData = "token=" + String(pushoverApiToken) + 
                    "&user=" + String(pushoverUserKey) + 
                    "&title=" + title + 
                    "&message=" + message;

  http.begin(clientSecure, "https://api.pushover.net/1/messages.json");  // Use secure client for HTTPS
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Pushover Notification Sent Successfully: " + response);
  } else {
    Serial.println("Error Sending Pushover Notification");
  }

  http.end();
}
