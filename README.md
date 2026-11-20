# SmartPatientMonitor — IoT Patient Vitals Monitor

[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Arduino-red.svg)](https://www.espressif.com/)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Multitasking-darkblue.svg)](https://www.freertos.org/)
[![MQTT](https://img.shields.io/badge/MQTT-PubSubClient-purple.svg)](https://mqtt.org/)

An embedded IoT healthcare monitoring system that generates physiological
vitals on an ESP32-S3, performs edge-based threshold detection, and streams
data over MQTT. The firmware includes fault-tolerant buffering and adaptive 
power management. Physical sensor integration is planned; the current build 
uses simulated vitals and is validated in Wokwi.

*Note: This is a pure embedded systems project. The companion Android app was 
deprecated to keep the scope focused on the hardware and firmware.*

---

## Project Status

| Component | Status |
|-----------|--------|
| ESP32-S3 firmware (FreeRTOS, MQTT, threshold alerts, SPIFFS buffering) | ✅ Complete — simulated sensor input, Wokwi-verified |
| Adaptive deep sleep | 🟡 Implemented — disabled in demo build (`ENABLE_DEEP_SLEEP 0`) |
| Physical hardware integration | ⏸ Deferred — board delivery delayed; Wokwi used as substitute |
| MQTT Data Dashboard (Web/Node-RED) | 🚧 Planned — for viewing the data without a custom app |

---

## Architecture
┌──────────────────┐ MQTT ┌────────────────┐ MQTT ┌─────────────────┐
│ ESP32-S3 │ ────────► │ MQTT Broker │ ◄──────── │ MQTT Client / │
│ (FreeRTOS) │ │ (HiveMQ) │ │ Web Dashboard │
└──────────────────┘ └────────────────┘ └─────────────────┘**MQTT topics:**

| Topic | Direction | Payload |
|-------|-----------|---------|
| `patient/sensor` | ESP32 → Broker | JSON: `{"hr":75,"spo2":98,"temp":36.5}` |
| `patient/alert` | ESP32 → Broker | Text: `CRITICAL` |
| `patient/command` | Client → Broker → ESP32 | Text: `ALARM_ON` / `ALARM_OFF` |

---

## Firmware features (implemented)

- **FreeRTOS multitasking** — Three independent tasks: sensor acquisition,
  network management, and alert handling.
- **Edge anomaly detection** — Critical thresholds (HR > 120 or SpO₂ < 92)
  are checked on the device and published immediately.
- **SPIFFS buffering** — If MQTT is unavailable, readings are appended to a
  flash file and replayed on reconnect.
- **Adaptive deep sleep** — Implemented but disabled in the demo build.
- **Two-way MQTT control** — The device subscribes to `patient/command` so a
  client can trigger or clear an alarm remotely.

---

## Tech stack

| Layer | Technology |
|-------|------------|
| Firmware language | C++ (Arduino framework) |
| Microcontroller | ESP32-S3-NANO |
| RTOS | FreeRTOS |
| Communication | MQTT (PubSubClient) |
| Serialization | ArduinoJson |
| Flash storage | SPIFFS |
| Simulation | Wokwi |

---

## How to run the firmware

**Wokwi simulator (no board required):** 
https://wokwi.com/projects/464887465868320769

**Physical hardware (when the board arrives):**
1. Install Arduino IDE and the ESP32 board manager.
2. Install libraries: PubSubClient, ArduinoJson.
3. Copy `firmware/secrets.h.example` to `firmware/secrets.h` and add WiFi credentials.
4. Select **Tools → Board → ESP32S3 Dev Module** and upload.

**Viewing the data:**
Use any MQTT client (like MQTTX or the HiveMQ web client) to subscribe to `patient/sensor` on `broker.hivemq.com:1883`.

---

## Known limitations

- **Simulated vitals** — constrained random numbers; real sensors planned.
- **Public MQTT broker** — unauthenticated and unencrypted; fine for a demo.
- **Deep sleep disabled** — `ENABLE_DEEP_SLEEP` is `0` in the demo build.
- **Buffered alerts replay to `patient/sensor`** — a known firmware bug; alerts
  saved during an outage appear as sensor readings on reconnect.

---

## Future work

1. Fix buffered-alert topic and unsafe MQTT callback string in firmware.
2. Enable adaptive deep sleep after power measurement on real hardware.
3. Connect real sensors (MAX30100, DS18B20).
4. Add TLS to MQTT (`mqtts://` on 8883).
5. Build a lightweight HTML/JS dashboard to subscribe to the MQTT topics.

---

## License

MIT — see the `LICENSE` file.

## Author

Michelle Sayi — [GitHub](https://github.com/michellesayi69)
