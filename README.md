# Cool Bed Firmware

ESP32 firmware for a water-cooled bed device. Pumps water from a tank through a mattress, with speed and temperature control, flow/current/voltage monitoring, and a web UI.

## Hardware

- **MCU**: ESP32
- **Motor driver**: TB6612FNG (SparkFun) or generic PWM (e.g. DRV8871) — selected at build time
- **Current/Voltage**: INA226 via I2C (SDA=21, SCL=22)
- **Temperature**: Two DS18B20 Dallas sensors (One Wire) — outgoing and return
- **Flow sensor**: Digital pulse output on GPIO 15
- **LEDs**: Built-in (Wi-Fi status), green (pump running), red (error)
- **Button**: IO0 — hold 5 s after boot to reset Wi-Fi credentials

## Features

- **Modes**: Stop, Speed (fixed PWM), Temperature (PID-like auto-adjust), Calibration
- **Web UI**: Alpine.js SPA served from SPIFFS — telemetry, mode control, 10-min history chart
- **MQTT**: Optional; publishes full state, supports Home Assistant MQTT discovery
- **mDNS**: Reachable at `<hostname>.local`
- **Wi-Fi provisioning**: WiFiManager captive portal on first boot

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
# TB6612 motor driver
pio run -e cool-bed-tb6612

# Generic PWM motor driver
pio run -e cool-bed-pwm

# Upload firmware + filesystem
pio run -e cool-bed-tb6612 -t upload
pio run -e cool-bed-tb6612 -t uploadfs
```

## First Boot

1. Device starts a Wi-Fi AP named after the configured hostname (default: `cool-bed`).
2. Connect to the AP and select your Wi-Fi network.
3. Access the UI at `http://cool-bed.local` (or the device's IP).

## Configuration

All settings are available via the web UI at `/config.html`, including MQTT credentials, operating thresholds, and calibration values. Settings are persisted in ESP32 NVS (`Preferences`).

## Project Structure

```
src/
  main.cpp          — Entry point, global objects, setup/loop
  State.h/cpp       — All config vars and telemetry metrics (thread-safe)
  Controller.h/cpp  — Motor, sensors, mode logic, LED/button handling
  FlowSensor.h/cpp  — Dual raw/filtered pulse counting
  Connectivity.h/cpp — WiFiManager, reconnect, mDNS
  WebInterface.h/cpp — HTTP server, REST API (/api/state, /api/config, /api/mode)
  MqttInterface.h/cpp — MQTT client, HA discovery
  Metric.h          — Templated metric/config base class
  ConfigVar.h       — Extends Metric with NVS persistence

data/               — SPIFFS web files
  index.html        — Main SPA (telemetry, mode controls, chart)
  config.html       — Configuration page
  app.js / config.js — Alpine.js components
  style.css         — Dark-theme styles
  alpine.min.js     — Alpine.js v3 (local)
  chart.min.js      — Chart.js v4 (local)
```
