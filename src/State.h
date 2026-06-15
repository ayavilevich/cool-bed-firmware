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
#define MODE_SPEED "speed"
#define MODE_TEMPERATURE "temperature"
#define MODE_CALIBRATION "calibration"
#define MODE_FLOW_TEST "flowtest"

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

	// Mark whether telemetry metrics contain at least one valid sensor sample set
	void setMetricsValid(bool valid);

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
	ConfigVar<String>        mode            { "mode",            MODE_STOP,        "",     "Operating mode: stop/speed/temperature/calibration/flowtest" };
	ConfigVar<uint8_t>       speedSetPoint   { "speedSetPoint",   255,              "counts",     "Pump speed set point",        1,    255           };
	ConfigVar<float>         temperatureSetPoint { "temperatureSetPoint", 26.0f,    "°C",   "Temperature set point",       10.0f, 40.0f, "tempSetPoint" };
	ConfigVar<float>         outTemperatureCalibrationOffset { "outTemperatureCalibrationOffset", 0.0f, "°C", "Outgoing temperature offset", -10.0f, 10.0f, "outTempCalOff" };
	ConfigVar<float>         returnTemperatureCalibrationOffset { "returnTemperatureCalibrationOffset", 0.0f, "°C", "Return temperature offset", -10.0f, 10.0f, "retTempCalOff" };
	ConfigVar<float>         coolingTemperatureCalibrationOffset { "coolingTemperatureCalibrationOffset", 0.0f, "°C", "Cooling temperature offset", -10.0f, 10.0f, "coolTempOff" };
	ConfigVar<unsigned int>  calibrationVolume { "calibrationVolume", 500,          "ml",   "Calibration volume",          1,    5000,  "calVolume"     };
	ConfigVar<unsigned int>  calibrationFlowPulses { "calibrationFlowPulses", 500,  "pulses","Calibration flow pulses",    1,    10000, "calFlowPulses" };
	ConfigVar<unsigned int>  systemTime      { "systemTime",      30,               "s",    "System response time",        0,    600          };
	ConfigVar<unsigned int>  minFlowPulsesPerSec { "minFlowPulsesPerSec", 5,        "p/s",  "Min flow",                    1,    1000,  "minFlowPPS"    };
	ConfigVar<unsigned int>  maxCurrent      { "maxCurrent",      1000,             "mA",   "Max pump current",            1,    1200         };
	ConfigVar<unsigned int>  minVoltage      { "minVoltage",      4000,             "mV",   "Min pump voltage",            0,    40000        };

	// --------------- Telemetry ---------------
	Metric<String>           status          { "status",          "",               "",     "Device status"               };
	Metric<bool>             error           { "error",           false,            "",     "Error state"                 };
	Metric<String>           fwVersion       { "fwVersion",       FW_VERSION,       "",     "Firmware version"            };
	Metric<int>              rssi            { "rssi",            -127,             "dBm",  "Wi-Fi RSSI"                 };
	Metric<String>           buildDateTime   { "buildDateTime",   BUILD_DATE_TIME,  "",     "Build date and time"         };
	Metric<String>           buildTimestamp  { "buildTimestamp",  BUILD_TIMESTAMP,  "",     "Build timestamp"             };
	Metric<uint8_t>          pumpSpeed       { "pumpSpeed",       0,                "counts","Current pump speed"          };
	Metric<uint64_t>         flowPulsesRaw   { "flowPulsesRaw",   0,                "pulses", "Raw flow pulse count"        };
	Metric<uint64_t>         flowPulsesFiltered { "flowPulsesFiltered", 0,          "pulses", "Filtered flow pulse count"   };
	Metric<unsigned int>     pumpVoltage     { "pumpVoltage",     0,                "mV",   "Pump voltage"                };
	Metric<int>              pumpCurrent     { "pumpCurrent",     0,                "mA",   "Pump current"                };
	Metric<float>            outTemperature  { "outTemperature",  20.0f,            "°C",   "Outgoing water temperature"  };
	Metric<float>            returnTemperature { "returnTemperature", 20.0f,        "°C",   "Returning water temperature" };
	Metric<float>            coolingTemperature { "coolingTemperature", 20.0f,  "°C",   "Cooling water temperature" };

	// --------------- Calculated telemetry ---------------
	Metric<unsigned int>     flowPulsesRawPerSec     { "flowPulsesRawPerSec",      0, "p/s", "Raw flow"      };
	Metric<unsigned int>     flowPulsesFilteredPerSec { "flowPulsesFilteredPerSec", 0, "p/s","Filtered flow" };
	Metric<float>            flow            { "flow",            0.0f,             "L/min", "Water flow rate"          };
	Metric<float>            temperatureDelta { "temperatureDelta", 0.0f,          "°C",    "Return-out temperature"    };
	Metric<float>            coolingPower    { "coolingPower",    0.0f,             "W",     "Cooling power"             };

private:
	SemaphoreHandle_t _mutex;
	bool _metricsValid;

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
