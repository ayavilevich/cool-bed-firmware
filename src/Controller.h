// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

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
#ifdef CIRCULATION_TB6612_AIN1_PIN
#include <SparkFun_TB6612.h>
#endif

// ---- Constants ----
#define PUMP_MAX_SPEED					255 // max PWM value for pump speed
#define SENSOR_SAMPLE_INTERVAL_MS		1000
#define TEMP_RETRY_COUNT				3
#define TEMP_MIN_CELSIUS				-5.0f
#define TEMP_MAX_CELSIUS				40.0f
#define VARIABLE_SPEED_STEP				10
#define OUTPUT_RAMP_UP_MS				1000
#define FLOW_TEST_STEP					10
#define FLOW_TEST_STEP_INTERVAL_RATIO	0.5 // how long to test each speed in a flow test. ratio of the "system time".
#define FLOW_TEST_FIRST_STEP_INTERVAL_RATIO	1.0 // ratio of the "system time". first step needs more time to prime from stopped, subsequent steps can be faster.
#define FLOW_TEST_MAX_SPEED				PUMP_MAX_SPEED
#define BUTTON_HOLD_MS					5000
#define WATER_SPECIFIC_HEAT_J_PER_KG_C	4186.0f // how much energy (in joules) it takes to raise 1 kg of water by 1 degree Celsius
#define SECONDS_PER_MINUTE				60.0f
#define COOLING_HYSTERESIS_START		1.0f
#define COOLING_HYSTERESIS_STOP			0.0f
#define HEATING_HYSTERESIS_STOP			-1.0f
#define HEATING_HYSTERESIS_START		-2.0f

// Logic to check if temperature sensors might be uncalibrated. Check after some time of flow inactivity when sensors should have equilibrated.
#define TEMPERATURE_CALIBRATION_EQUILIBRIUM_SECONDS			(10UL * 60UL)
#define TEMPERATURE_CALIBRATION_DELTA_WARNING_THRESHOLD_C	0.2f
#define TEMPERATURE_CALIBRATION_WARNING_STATUS_TEXT			"Warning: temperature sensors might not be calibrated"

// INA226 settings
#ifndef INA226_CIRCULATION_ADDRESS
#define INA226_CIRCULATION_ADDRESS		0x40
#endif
#define INA226_SHUNT_RESISTANCE			0.1f	// Ohm
#define INA226_MAX_CURRENT_A			1.2f	// A

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

#ifdef FLOW_SENSOR_PIN
	FlowSensor _flowSensor;
#endif

#ifdef DALLAS_SENSOR_OUTGOING_PIN
	OneWire _owOut;
	DallasTemperature _tempOut;
#endif
#ifdef DALLAS_SENSOR_RETURNING_PIN
	OneWire _owReturn;
	DallasTemperature _tempReturn;
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	OneWire _owCooling;
	DallasTemperature _tempCooling;
#endif

#ifdef INA226_CIRCULATION_ADDRESS
	INA226_WE _inaCirc;
#endif
#ifdef INA226_COOLING_ADDRESS
	INA226_WE _inaCooling;
#endif
#ifdef INA226_HEATING_ADDRESS
	INA226_WE _inaHeating;
#endif

#ifdef CIRCULATION_TB6612_AIN1_PIN
	Motor* _circMotor;
#endif

	// Sampling state
	unsigned long _lastSampleMs;
	uint64_t _prevRawPulses;
	uint64_t _prevFilteredPulses;
	unsigned long _prevPulsesMs;

	// Mode operation state
	unsigned long _modeStartMs;		// when current mode was entered
	uint64_t _calibStartPulses;		// flowPulsesFiltered at calibration start
	unsigned long _calibStartMs;	// millis at calibration start
	unsigned long _lastValidFlowMs;	// last time we had valid flow reading (used to detect flow inactivity)

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
	// Flow test mode timing
	unsigned long _lastFlowTestStepMs;
	unsigned long _circActiveSinceMs;
	unsigned long _coolingActiveSinceMs;
	unsigned long _heatingActiveSinceMs;

	// Serial state summary
	unsigned long _lastSerialPrintMs;

	void _sampleSensors();
	void _runMode();
	void _triggerError(const String& cause);
	void _clearError();
	void _setCircSpeed(uint8_t speed);
	void _setCoolingSpeed(uint8_t speed);
	void _setHeatingSpeed(uint8_t speed);
	void _stopAllOutputs();
	void _updateOutputLedIndicators();
	bool readTemperatureOnce(DallasTemperature& sensor, float& outVal);
	bool readTemperatureRetry(DallasTemperature& sensor, float& outVal);
	void _calculateDerivedMetrics(uint64_t rawNow, uint64_t filteredNow, unsigned long nowMs);
	void _checkIvLimitsAndFaults(unsigned long now);
};
