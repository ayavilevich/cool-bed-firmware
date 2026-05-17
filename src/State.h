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

	// Apply a JSON object of config values; returns false if nothing matched
	bool applyConfigJson(const JsonObjectConst& obj);

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
	ConfigVar<String>        mode            { "mode",            MODE_STOP,        "",     "Operating mode: stop/speed/temperature/calibration" };
	ConfigVar<uint8_t>       speedSetPoint   { "speedSetPoint",   255,              "",     "Pump speed set point",        1,    255           };
	ConfigVar<float>         temperatureSetPoint { "temperatureSetPoint", 20.0f,    "°C",   "Temperature set point",       10.0f, 40.0f, "tempSetPoint" };
	ConfigVar<unsigned int>  calibrationVolume { "calibrationVolume", 500,          "ml",   "Calibration volume",          1,    5000,  "calVolume"     };
	ConfigVar<float>         calibrationFlow { "calibrationFlow", 1.0f,             "L/min","Calibration end flow",        0.0f, 100.0f,"calFlow"       };
	ConfigVar<unsigned int>  calibrationFlowPulses { "calibrationFlowPulses", 500,  "",     "Calibration flow pulses",     1,    10000, "calFlowPulses" };
	ConfigVar<unsigned int>  systemTime      { "systemTime",      30,               "s",    "System response time",        0,    600          };
	ConfigVar<unsigned int>  minFlowPulsesPerSec { "minFlowPulsesPerSec", 10,       "p/s",  "Min flow pulses/sec",         1,    1000,  "minFlowPPS"    };
	ConfigVar<unsigned int>  maxCurrent      { "maxCurrent",      1000,             "mA",   "Max pump current",            1,    1200         };
	ConfigVar<unsigned int>  minVoltage      { "minVoltage",      4000,             "mV",   "Min pump voltage",            0,    40000        };

	// --------------- Telemetry ---------------
	Metric<String>           status          { "status",          "",               "",     "Device status"               };
	Metric<bool>             error           { "error",           false,            "",     "Error state"                 };
	Metric<uint8_t>          pumpSpeed       { "pumpSpeed",       0,                "",     "Current pump speed"          };
	Metric<uint64_t>         flowPulsesRaw   { "flowPulsesRaw",   0,                "",     "Raw flow pulse count"        };
	Metric<uint64_t>         flowPulsesFiltered { "flowPulsesFiltered", 0,          "",     "Filtered flow pulse count"   };
	Metric<unsigned int>     pumpVoltage     { "pumpVoltage",     0,                "mV",   "Pump voltage"                };
	Metric<unsigned int>     pumpCurrent     { "pumpCurrent",     0,                "mA",   "Pump current"                };
	Metric<float>            outTemperature  { "outTemperature",  20.0f,            "°C",   "Outgoing water temperature"  };
	Metric<float>            returnTemperature { "returnTemperature", 20.0f,        "°C",   "Returning water temperature" };

	// --------------- Calculated telemetry ---------------
	Metric<unsigned int>     flowPulsesRawPerSec     { "flowPulsesRawPerSec",      0, "p/s", "Raw flow pulses/sec"      };
	Metric<unsigned int>     flowPulsesFilteredPerSec { "flowPulsesFilteredPerSec", 0, "p/s","Filtered flow pulses/sec" };
	Metric<float>            flow            { "flow",            0.0f,             "L/min", "Water flow rate"          };

private:
	SemaphoreHandle_t _mutex;

	// Helper to write all config vars to JSON
	void _configToJson(JsonObject obj, bool excludeMqtt) const;
	// Helper to write all telemetry vars to JSON
	void _telemetryToJson(JsonObject obj) const;
};

// RAII guard for State mutex
class StateGuard {
public:
	explicit StateGuard(State& state) : _state(state) { _state.lock(); }
	~StateGuard() { _state.unlock(); }
private:
	State& _state;
};
