// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Metric.h"
#include "ConfigVar.h"

// Namespace for preferences storage
#define PREFS_NAMESPACE "cool-bed"

// Operating mode values
#define MODE_STOP "stop"
#define MODE_TEMPERATURE "temperature"
#define MODE_TEMPERATURE_PREPARE "temperature_prepare"
#define MODE_MANUAL_CIRC "manual_circ"
#define MODE_MANUAL_COOL "manual_cool"
#define MODE_MANUAL_HEAT "manual_heat"
#define MODE_PRIME_CIRC "prime_circ"
#define MODE_FLOW_CALIBRATION "flow_calibration"
#define MODE_FLOW_TEST "flow_test"

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

#define BUILD_DATE_TIME __DATE__ " " __TIME__
#define BUILD_TIMESTAMP __TIMESTAMP__

class State {
public:
	State();
	void begin();

	// Take / give the mutex (use RAII via StateGuard below)
	void lock();
	void unlock();

	// Load all config from Preferences
	void loadConfig();

	// Save a single config variable (called when a value changes)
	template<typename T>
	void saveConfigVar(ConfigVar<T>& var) {
		Preferences prefs;
		prefs.begin(PREFS_NAMESPACE, false);
		var.save(prefs);
		prefs.end();
	}

	// Serialize entire state to JSON
	void toJson(JsonObject obj, bool includeConfig = true, bool includeTelemetry = true,
				bool excludeMqttConfig = false) const;

	// Serialize metric/config model metadata to JSON
	void toModelJson(JsonObject obj, bool includeConfig = true, bool includeTelemetry = true,
				     bool excludeMqttConfig = false) const;

	// Apply a JSON object of config values; returns false if nothing matched
	bool applyConfigJson(const JsonObjectConst& obj);
	bool isModeSupported(const String& mode) const;
	bool isValidMode(const String& mode) const;

	// Mark whether telemetry metrics contain some data beyond initial values. This is used to determine whether to include telemetry in JSON output.
	void setMetricsInitialized(bool valid);

	// --------------- Configuration ---------------
	ConfigVar<String>        hostname        { "hostname",        "cool-bed",       "",     "Hostname / device ID",                           };
	ConfigVar<bool>          mqtt            { "mqtt",            false,            "",     "Enable MQTT"                                     };
	ConfigVar<String>        mqttServer      { "mqttServer",      "",               "",     "MQTT server hostname"                            };
	ConfigVar<int>           mqttPort        { "mqttPort",        1883,             "",     "MQTT server port",            1,    65535         };
	ConfigVar<String>        mqttUsername    { "mqttUsername",    "",               "",     "MQTT username"                                   };
	ConfigVar<String>        mqttPassword    { "mqttPassword",    "",               "",     "MQTT password"                                   };
	ConfigVar<String>        mqttRootTopic   { "mqttRootTopic",   "cool-bed",       "",     "MQTT root topic"                                 };
	ConfigVar<bool>          mqttHADiscovery { "mqttHADiscovery", true,             "",     "Enable HA MQTT discovery"                        };
	ConfigVar<String>        mqttHADiscoveryTopic { "mqttHADiscoveryTopic", "homeassistant", "", "HA discovery topic prefix",                  {}, {}, "mqttHADiscTopic" };
	ConfigVar<String>        mode            { "mode",            MODE_STOP,        "",     "Operating mode" };
	ConfigVar<uint8_t>       circSpeedSetPoint   { "circSpeedSetPoint",   255,              "counts",     "Circulation speed set point",        1,    255, "circSpSet"           };
	ConfigVar<uint8_t>       coolingSpeedSetPoint { "coolingSpeedSetPoint", 255,             "counts",     "Cooling speed set point",            0,    255, "coolSpSet"           };
	ConfigVar<uint8_t>       heatingSpeedSetPoint { "heatingSpeedSetPoint", 255,             "counts",     "Heating speed set point",            0,    255, "heatSpSet"           };
	ConfigVar<float>         temperatureSetPoint { "temperatureSetPoint", 26.0f,    "°C",   "Temperature set point",       10.0f, 40.0f, "tempSetPoint" };
	ConfigVar<float>         outTemperatureCalibrationOffset { "outTemperatureCalibrationOffset", 0.0f, "°C", "Outgoing temperature offset", -10.0f, 10.0f, "outTempCalOff" };
	ConfigVar<float>         returnTemperatureCalibrationOffset { "returnTemperatureCalibrationOffset", 0.0f, "°C", "Return temperature offset", -10.0f, 10.0f, "retTempCalOff" };
	ConfigVar<float>         coolingTemperatureCalibrationOffset { "coolingTemperatureCalibrationOffset", 0.0f, "°C", "Cooling temperature offset", -10.0f, 10.0f, "coolTempOff" };
	ConfigVar<unsigned int>  calibrationVolume { "calibrationVolume", 500,          "ml",   "Calibration volume",          1,    5000,  "calVolume"     };
	ConfigVar<unsigned int>  calibrationFlowPulses { "calibrationFlowPulses", 500,  "pulses","Calibration flow pulses",    1,    10000, "calFlowPulses" };
	ConfigVar<unsigned int>  systemTime      { "systemTime",      30,               "s",    "System response time",        0,    600          };
	ConfigVar<unsigned int>  minFlowPulsesPerSec { "minFlowPulsesPerSec", 5,        "p/s",  "Min flow",                    1,    1000,  "minFlowPPS"    };
	ConfigVar<unsigned int>  maxCircCurrent      { "maxCircCurrent",      700,             "mA",   "Max circulation current",            1,    1200, "maxCircCur"         };
	ConfigVar<unsigned int>  minCircCurrent      { "minCircCurrent",      200,             "mA",   "Min circulation current",            1,    1200, "minCircCur"         };
	ConfigVar<unsigned int>  minCircVoltage      { "minCircVoltage",      4000,            "mV",   "Min circulation voltage",            0,    40000, "minCircVolt"        };
	ConfigVar<unsigned int>  maxCoolingCurrent   { "maxCoolingCurrent",   700,             "mA",   "Max cooling current",                1,    1200, "maxCoolCur"         };
	ConfigVar<unsigned int>  minCoolingCurrent   { "minCoolingCurrent",   200,             "mA",   "Min cooling current",                1,    1200, "minCoolCur"         };
	ConfigVar<unsigned int>  minCoolingVoltage   { "minCoolingVoltage",   4000,            "mV",   "Min cooling voltage",                0,    40000, "minCoolVolt"        };
	ConfigVar<unsigned int>  maxHeatingCurrent   { "maxHeatingCurrent",   2500,            "mA",   "Max heating current",                1,    4000, "maxHeatCur"         };
	ConfigVar<unsigned int>  minHeatingCurrent   { "minHeatingCurrent",   200,             "mA",   "Min heating current",                1,    4000, "minHeatCur"         };
	ConfigVar<unsigned int>  minHeatingVoltage   { "minHeatingVoltage",   4000,            "mV",   "Min heating voltage",                0,    40000, "minHeatVolt"        };

	// --------------- Telemetry ---------------
	// telemetry min/max range is an expected range for the metric, not a hard limit. Can use this to scale UI, etc. The actual value can exceed the min/max range.
	Metric<String>           status          { "status",          "",               "",     "Device status"               };
	Metric<bool>             error           { "error",           false,            "",     "Error state",               false, true };
	Metric<String>           fwVersion       { "fwVersion",       FW_VERSION,       "",     "Firmware version"            };
	Metric<int>              rssi            { "rssi",            -127,             "dBm",  "Wi-Fi RSSI",               -127, 0 };
	Metric<String>           buildDateTime   { "buildDateTime",   BUILD_DATE_TIME,  "",     "Build date and time"         };
	Metric<String>           buildTimestamp  { "buildTimestamp",  BUILD_TIMESTAMP,  "",     "Build timestamp"             };
	Metric<uint8_t>          circSpeed       { "circSpeed",       0,                "counts","Current circulation speed",        0, 255 };
	Metric<uint8_t>          coolingSpeed    { "coolingSpeed",    0,                "counts","Current cooling speed",            0, 255 };
	Metric<uint8_t>          heatingSpeed    { "heatingSpeed",    0,                "counts","Current heating speed",            0, 255 };
	Metric<uint64_t>         flowPulsesRaw   { "flowPulsesRaw",   0,                "pulses", "Raw flow pulse count",        0, uint64_t(-1) };
	Metric<uint64_t>         flowPulsesFiltered { "flowPulsesFiltered", 0,          "pulses", "Filtered flow pulse count",   0, uint64_t(-1) };
	Metric<unsigned int>     circVoltage     { "circVoltage",     0,                "mV",   "Circulation voltage", 0, 40000 };
	Metric<int>              circCurrent     { "circCurrent",     0,                "mA",   "Circulation current", 0, 4000 };
	Metric<unsigned int>     coolingVoltage  { "coolingVoltage",  0,                "mV",   "Cooling voltage", 0, 40000 };
	Metric<int>              coolingCurrent  { "coolingCurrent",  0,                "mA",   "Cooling current", 0, 4000 };
	Metric<unsigned int>     heatingVoltage  { "heatingVoltage",  0,                "mV",   "Heating voltage", 0, 40000 };
	Metric<int>              heatingCurrent  { "heatingCurrent",  0,                "mA",   "Heating current", 0, 4000 };
	Metric<float>            outTemperature  { "outTemperature",  20.0f,            "°C",   "Outgoing water temperature",  -5.0f, 40.0f };
	Metric<float>            returnTemperature { "returnTemperature", 20.0f,        "°C",   "Returning water temperature", -5.0f, 40.0f };
	Metric<float>            coolingTemperature { "coolingTemperature", 20.0f,      "°C",   "Cooling water temperature",   -5.0f, 40.0f };
	Metric<bool>             flowSensorPresent { "flowSensorPresent", false,         "",      "Flow sensor is present",       false, true };
	Metric<bool>             coolingPresent { "coolingPresent", false,                "",      "Cooling output is present",     false, true };
	Metric<bool>             heatingPresent { "heatingPresent", false,                "",      "Heating output is present",     false, true };
	Metric<bool>             coolingTemperaturePresent { "coolingTemperaturePresent", false, "", "Cooling temp sensor is present", false, true };
	Metric<bool>             returnTemperaturePresent { "returnTemperaturePresent", false, "", "Return temp sensor is present",  false, true };
	Metric<bool>             circIVPresent   { "circIVPresent", false,                "",      "Circulation IV sensing present", false, true };
	Metric<bool>             coolingIVPresent { "coolingIVPresent", false,            "",      "Cooling IV sensing present",     false, true };
	Metric<bool>             heatingIVPresent { "heatingIVPresent", false,            "",      "Heating IV sensing present",     false, true };

	// --------------- Calculated telemetry ---------------
	Metric<unsigned int>     flowPulsesRawPerSec     { "flowPulsesRawPerSec",      0, "p/s", "Raw flow"     , 0, 10000 };
	Metric<unsigned int>     flowPulsesFilteredPerSec { "flowPulsesFilteredPerSec", 0, "p/s","Filtered flow", 0, 10000 };
	Metric<float>            flow            { "flow",            0.0f,             "L/min", "Water flow rate",               0.0f, 100.0f };
	Metric<float>            temperatureDelta { "temperatureDelta", 0.0f,          "°C",    "Return-out temperature delta",  -40.0f, 40.0f };
	Metric<float>            coolingTransfer    { "coolingTransfer",    0.0f,             "W",     "Cooling transfer", 0.0f, 10000.0f };

private:
	SemaphoreHandle_t _mutex;
	bool _metricsInitialized;

	// Helper to write all config vars to JSON
	void _configToJson(JsonObject obj, bool excludeMqtt) const;
	// Helper to write all telemetry vars to JSON
	void _telemetryToJson(JsonObject obj) const;
	// Helper to write config metadata model to JSON
	void _configModelToJson(JsonObject obj, bool excludeMqtt) const;
	// Helper to write telemetry metadata model to JSON
	void _telemetryModelToJson(JsonObject obj) const;
};

// RAII guard for State mutex
class StateGuard {
public:
	explicit StateGuard(State& state) : _state(state) { _state.lock(); }
	~StateGuard() { _state.unlock(); }
private:
	State& _state;
};
