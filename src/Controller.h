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
	bool _readTemperatureOnce(DallasTemperature& sensor, float& outVal);
	bool _readTemperatureRetry(DallasTemperature& sensor, float& outVal);
	void _calculateDerivedMetrics(uint64_t rawNow, uint64_t filteredNow, unsigned long nowMs);
	void _checkIvLimitsAndFaults(unsigned long now);
};
