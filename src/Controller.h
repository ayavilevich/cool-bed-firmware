#pragma once
#include <Arduino.h>
#include <functional>
#include "State.h"
#include "FlowSensor.h"

class Connectivity; // can't include Connectivity.h here due to circular dependency or conflict between WebServer.h and ESPAsyncWebServer.h, just forward declare

// INA226 library
#include <INA226_WE.h>
// Dallas temperature
#include <OneWire.h>
#include <DallasTemperature.h>

// Motor driver: conditionally include TB6612 library
#ifdef MOTOR_DRIVER_TB6612_AIN1_PIN
#include <SparkFun_TB6612.h>
#endif

// ---- Constants ----
#define SENSOR_SAMPLE_INTERVAL_MS		1000
#define TEMP_RETRY_COUNT				3
#define TEMP_MIN_CELSIUS				-5.0f
#define TEMP_MAX_CELSIUS				40.0f
#define TEMP_STEP						10
#define BUTTON_HOLD_MS					5000

// INA226 settings
#define INA226_I2C_ADDRESS				0x40
#define INA226_SHUNT_RESISTANCE			0.1f	// Ohm
#define INA226_MAX_CURRENT_A			1.3f	// A

// Wi-Fi built-in LED blink period when connected
#define BUILTIN_LED_BLINK_PERIOD_MS		1000

using EventCallback = std::function<void()>;

class Controller {
public:
	Controller(State& state, Connectivity& connectivity);

	void begin();
	void loop();

	// Register callbacks for telemetry/config change events (used by MQTT)
	void onTelemetryUpdated(EventCallback cb) { _onTelemetryUpdated = cb; }
	void onConfigUpdated(EventCallback cb) { _onConfigUpdated = cb; }

	// Called when mode is changed externally (web/MQTT)
	void setMode(const String& newMode);

	// Notify that config changed externally
	void notifyConfigChanged();

	// Check button state (call from main loop)
	void checkButton();

	// LED update (call from main loop)
	void updateLeds(bool wifiConnected);

private:
	State& _state;
	Connectivity& _connectivity;
	FlowSensor _flowSensor;

	OneWire _owOut;
	OneWire _owReturn;
	DallasTemperature _tempOut;
	DallasTemperature _tempReturn;

	INA226_WE _ina226;

#ifdef MOTOR_DRIVER_TB6612_AIN1_PIN
	Motor* _motor;
#endif

	// Sampling state
	unsigned long _lastSampleMs;
	uint64_t _prevRawPulses;
	uint64_t _prevFilteredPulses;
	unsigned long _prevPulsesMs;

	// Mode operation state
	unsigned long _modeStartMs;		// when current mode was entered
	uint8_t _currentSpeed;			// track current speed to avoid redundant writes to motor driver and adjust value relative to it in temperature mode
	uint64_t _calibStartPulses;		// flowPulsesFiltered at calibration start
	unsigned long _calibStartMs;	// millis at calibration start

	// Error state
	bool _inError;
	String _errorCause;

	EventCallback _onTelemetryUpdated;
	EventCallback _onConfigUpdated;

	// Button debounce
	unsigned long _buttonPressMs;
	bool _buttonIsPressed;

	// Built-in LED blink
	unsigned long _lastLedToggleMs;
	bool _builtinLedState;

	// Temperature mode timing
	unsigned long _lastTempAdjMs;

	// Serial state summary
	unsigned long _lastSerialPrintMs;

	void _sampleSensors();
	void _runMode();
	void _triggerError(const String& cause);
	void _clearError();
	void _setPumpSpeed(uint8_t speed);
	bool _readTemperature(DallasTemperature& sensor, float& outVal);
	void _calculateDerivedMetrics(uint64_t rawNow, uint64_t filteredNow, unsigned long nowMs);
};
