# AYG Cool Bed - Firmware

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

You can build, upload and monitor by using the UI in PlatformIO's VSCode panel.
It is a common mistake to forget to upload the Filesystem Image. This firmware uses the file system for static files of the web server. Forgetting to upload those will break normal operation.

Command line usage (using PlatformIO CLI):

```bash
# Cool Bed Prototype V1 board
pio run -e cool-bed-pt-v1-usb

# Quad MOS board
pio run -e esp32-4mos-usb

# Upload firmware + filesystem
pio run -e cool-bed-pt-v1-usb -t upload
pio run -e cool-bed-pt-v1-usb -t uploadfs
```

## Over-The-Air (OTA) Upload

See `platformio.local.sample.ini` for details. Save as `platformio.local.ini` and complete your parameters, such as password.

The first flash of a board must be done over serial.

## Adding a board or customizing one

Use `platformio.local.ini` to add unversioned PlatformIO environments and define custom boards or make modifications to default boards.

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
