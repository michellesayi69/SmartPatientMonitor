/*
 * SmartPatientMonitor - ESP32-S3 Firmware
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

#include "secrets.h"  // WiFi credentials (not committed to GitHub)

// ================= MQTT =================
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

// ================= Pins =================
const int LED_PIN    = 2;
const int BUTTON_PIN = 4;

// ================= Vitals =================
int hr = 75;
int spo2 = 98;
float temp = 37.0;

// ================= Thresholds =================
const int CRITICAL_HR   = 120;
const int CRITICAL_SPO2 = 92;
const int NORMAL_HR_MIN = 60;
const int NORMAL_HR_MAX = 100;
const int NORMAL_SPO2_MIN = 95;
const float NORMAL_TEMP_MAX = 38.0;

// ================= Power =================
#define ENABLE_DEEP_SLEEP 0
const unsigned long STABLE_SLEEP_MS = 10000;

volatile bool criticalCondition = false;
bool spiffsAvailable = false;

// ================= FreeRTOS =================
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;
TaskHandle_t alertTaskHandle = NULL;

WiFiClient espClient;
PubSubClient client(espClient);

// ================= Helpers =================
void saveToBuffer(const char* payload) {
  if (!spiffsAvailable) return;
  File file = SPIFFS.open("/buffer.txt", FILE_APPEND);
  if (file) {
    file.println(payload);
    file.close();
  }
}

void replayBuffer() {
  if (!spiffsAvailable) return;
  File file = SPIFFS.open("/buffer.txt", FILE_READ);
  if (!file) return;
  Serial.println("Replaying buffered data...");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0 && client.connected()) {
      // Route messages to the correct topic based on prefix
      if (line.startsWith("ALERT:")) {
        String alertMsg = line.substring(6); // Remove "ALERT:"
        client.publish("patient/alert", alertMsg.c_str());
      } else {
        client.publish("patient/sensor", line.c_str());
      }
      delay(50);
    }
  }
  file.close();
  SPIFFS.remove("/buffer.txt");
}

void publishVitals() {
  StaticJsonDocument<128> doc;
  doc["hr"]   = hr;
  doc["spo2"] = spo2;
  doc["temp"] = temp;
  char payload[128];
  serializeJson(doc, payload);

  if (client.connected()) {
    client.publish("patient/sensor", payload);
  } else {
    saveToBuffer(payload);
  }
  Serial.println(payload);
}

void checkThresholds() {
  if (hr > CRITICAL_HR || spo2 < CRITICAL_SPO2) {
    if (!criticalCondition) {
      criticalCondition = true;
      if (client.connected()) {
        client.publish("patient/alert", "CRITICAL");
      } else {
        saveToBuffer("ALERT:CRITICAL"); // Saved with a prefix for routing later
      }
    }
  }
}

// ================= MQTT Callback =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String cmd = String((char*)payload, length);
  if (cmd == "ALARM_ON") {
    criticalCondition = true;
    Serial.println("Remote ALARM_ON received");
  } else if (cmd == "ALARM_OFF") {
    criticalCondition = false;
    Serial.println("Remote ALARM_OFF received");
  }
}

// ================= Task 1: Sensor =================
void sensorTask(void *pv) {
  while (1) {
    hr   += random(-4, 5);
    hr    = constrain(hr, 50, 140);
    spo2 += random(-2, 3);
    spo2  = constrain(spo2, 90, 100);
    temp += random(-5, 6) / 10.0;
    temp  = constrain(temp, 35.0, 39.5);

    publishVitals();
    checkThresholds();

#if ENABLE_DEEP_SLEEP
    if (!criticalCondition &&
        hr >= NORMAL_HR_MIN && hr <= NORMAL_HR_MAX &&
        spo2 >= NORMAL_SPO2_MIN && temp <= NORMAL_TEMP_MAX) {
      Serial.println("Stable vitals - entering deep sleep 10s");
      esp_sleep_enable_timer_wakeup(STABLE_SLEEP_MS * 1000ULL);
      esp_deep_sleep_start();
    }
#endif

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ================= Task 2: Network =================
void networkTask(void *pv) {
  while (1) {
    if (!client.connected()) {
      Serial.println("Reconnecting to MQTT...");
      if (client.connect("ESP32_SmartPatientMonitor")) {
        client.subscribe("patient/command");
        replayBuffer();
        Serial.println("MQTT connected");
      }
    }
    client.loop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// ================= Task 3: Alert =================
void alertTask(void *pv) {
  bool lastButton = HIGH;
  while (1) {
    digitalWrite(LED_PIN, criticalCondition ? HIGH : LOW);

    bool btn = digitalRead(BUTTON_PIN);
    if (btn == LOW && lastButton == HIGH) {
      criticalCondition = !criticalCondition;
      Serial.println(criticalCondition ? "Button: alarm ON" : "Button: alarm OFF");
      delay(200);
    }
    lastButton = btn;

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  if (!SPIFFS.begin(true)) {
    Serial.println("⚠️ SPIFFS mount failed. Offline buffering disabled.");
    spiffsAvailable = false;
  } else {
    Serial.println("SPIFFS mounted successfully.");
    spiffsAvailable = true;
  }

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  xTaskCreatePinnedToCore(sensorTask,  "SensorTask",  4096, NULL, 1, &sensorTaskHandle,  1);
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 4096, NULL, 2, &networkTaskHandle, 1);
  xTaskCreatePinnedToCore(alertTask,   "AlertTask",   2048, NULL, 1, &alertTaskHandle,   1);

  Serial.println("FreeRTOS tasks started");
}

// ================= Loop =================
void loop() {
  vTaskDelay(portMAX_DELAY);
}
