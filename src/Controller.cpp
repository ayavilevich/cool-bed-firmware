// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "Controller.h"
#include "Connectivity.h"
#include "Utils.h"
#include <Wire.h>
#include <math.h>

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
#define INEFFECTIVE_COOLING_WARNING_DELTA_C 8.0f

// Logic to check if temperature sensors might be uncalibrated. Check after some time of flow inactivity when sensors should have equilibrated.
#define TEMPERATURE_CALIBRATION_EQUILIBRIUM_SECONDS			(10UL * 60UL)
#define TEMPERATURE_CALIBRATION_DELTA_WARNING_THRESHOLD_C	0.2f
#define TEMPERATURE_CALIBRATION_WARNING_STATUS_TEXT			"Warning: temperature sensors might not be calibrated"

// INA226 settings
#define INA_CIRC_SHUNT_RESISTANCE			0.1f	// Ohm
#define INA_CIRC_MAX_CURRENT_A				1.2f	// A
#define INA_COOLING_SHUNT_RESISTANCE		0.1f	// Ohm
#define INA_COOLING_MAX_CURRENT_A			1.2f	// A
#define INA_HEATING_SHUNT_RESISTANCE		0.02f	// Ohm, have a smaller shunt for the heater output to support higher currents
#define INA_HEATING_MAX_CURRENT_A			6f		// A

#define SERIAL_PRINT_INTERVAL_MS		5000
#define WIFI_LED_TOGGLE_INTERVAL_MS		500 // // Wi-Fi built-in LED blink period when connected
#define LED_TOGGLE_INTERVAL_MS			350

Controller::Controller(State& state, Connectivity& connectivity)
	: _state(state),
	  _connectivity(connectivity),
#ifdef FLOW_SENSOR_PIN
	  _flowSensor(FLOW_SENSOR_PIN),
#endif
#ifdef DALLAS_SENSOR_OUTGOING_PIN
	  _owOut(DALLAS_SENSOR_OUTGOING_PIN),
	  _tempOut(&_owOut),
#endif
#ifdef DALLAS_SENSOR_RETURNING_PIN
	  _owReturn(DALLAS_SENSOR_RETURNING_PIN),
	  _tempReturn(&_owReturn),
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	  _owCooling(DALLAS_SENSOR_COOLING_PIN),
	  _tempCooling(&_owCooling),
#endif
#ifdef INA226_CIRCULATION_ADDRESS
	  _inaCirc(INA226_CIRCULATION_ADDRESS),
#endif
#ifdef INA226_COOLING_ADDRESS
	  _inaCooling(INA226_COOLING_ADDRESS),
#endif
#ifdef INA226_HEATING_ADDRESS
	  _inaHeating(INA226_HEATING_ADDRESS),
#endif
	  _lastSampleMs(0),
	  _prevRawPulses(0),
	  _prevFilteredPulses(0),
	  _prevPulsesMs(0),
	  _modeStartMs(0),
	  _calibStartPulses(0),
	  _calibStartMs(0),
	  _lastValidFlowMs(0),
	  _inError(false),
	  _buttonPressMs(0),
	  _buttonIsPressed(false),
	  _lastLedToggleMs(0),
	  _builtinLedState(false),
	  _lastTempAdjMs(0),
	  _lastFlowTestStepMs(0),
	  _circActiveSinceMs(0),
	  _coolingActiveSinceMs(0),
	  _heatingActiveSinceMs(0),
	  _lastSerialPrintMs(0) {
#ifdef CIRCULATION_TB6612_AIN1_PIN
	_circMotor = nullptr;
#endif
}

void Controller::begin() {
#ifdef RED_LED_PIN
	pinMode(RED_LED_PIN, OUTPUT);
	digitalWrite(RED_LED_PIN, LOW);
#endif
#ifdef GREEN_LED_PIN
	pinMode(GREEN_LED_PIN, OUTPUT);
	digitalWrite(GREEN_LED_PIN, LOW);
#endif
#ifdef BUILTIN_LED_PIN
	pinMode(BUILTIN_LED_PIN, OUTPUT);
	digitalWrite(BUILTIN_LED_PIN, LOW);
#endif

	pinMode(BUTTON_PIN, INPUT_PULLUP);

#ifdef CIRCULATION_TB6612_AIN1_PIN
	_circMotor = new Motor(CIRCULATION_TB6612_AIN1_PIN, CIRCULATION_TB6612_AIN2_PIN,
	                   CIRCULATION_PWM_PIN, 1, CIRCULATION_TB6612_STBY_PIN);
#else
	pinMode(CIRCULATION_PWM_PIN, OUTPUT);
	analogWrite(CIRCULATION_PWM_PIN, 0);
#endif

#ifdef COOLING_PWM_PIN
	pinMode(COOLING_PWM_PIN, OUTPUT);
	analogWrite(COOLING_PWM_PIN, 0);
#endif
#ifdef HEATING_PWM_PIN
	pinMode(HEATING_PWM_PIN, OUTPUT);
	analogWrite(HEATING_PWM_PIN, 0);
#endif

#ifdef FLOW_SENSOR_PIN
	_flowSensor.begin();
#endif
#ifdef DALLAS_SENSOR_OUTGOING_PIN
	_tempOut.begin();
#endif
#ifdef DALLAS_SENSOR_RETURNING_PIN
	_tempReturn.begin();
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	_tempCooling.begin();
#endif

	Wire.begin();
#ifdef INA226_CIRCULATION_ADDRESS
	_inaCirc.init();
	_inaCirc.setResistorRange(INA_CIRC_SHUNT_RESISTANCE, INA_CIRC_MAX_CURRENT_A);
	_inaCirc.setAverage(INA226_AVERAGE_128);
	_inaCirc.setConversionTime(INA226_CONV_TIME_204);
	_inaCirc.waitUntilConversionCompleted();
#endif
#ifdef INA226_COOLING_ADDRESS
	_inaCooling.init();
	_inaCooling.setResistorRange(INA_COOLING_SHUNT_RESISTANCE, INA_COOLING_MAX_CURRENT_A);
	_inaCooling.setAverage(INA226_AVERAGE_128);
	_inaCooling.setConversionTime(INA226_CONV_TIME_204);
	_inaCooling.waitUntilConversionCompleted();
#endif
#ifdef INA226_HEATING_ADDRESS
	_inaHeating.init();
	_inaHeating.setResistorRange(INA_HEATING_SHUNT_RESISTANCE, INA_HEATING_MAX_CURRENT_A);
	_inaHeating.setAverage(INA226_AVERAGE_128);
	_inaHeating.setConversionTime(INA226_CONV_TIME_204);
	_inaHeating.waitUntilConversionCompleted();
#endif

	{
		StateGuard guard(_state);
		_state.mode.set(MODE_STOP);
		_state.flowSensorPresent.set(
#ifdef FLOW_SENSOR_PIN
			true
#else
			false
#endif
		);
		_state.coolingPresent.set(
#ifdef COOLING_PWM_PIN
			true
#else
			false
#endif
		);
		_state.heatingPresent.set(
#ifdef HEATING_PWM_PIN
			true
#else
			false
#endif
		);
		_state.returnTemperaturePresent.set(
#ifdef DALLAS_SENSOR_RETURNING_PIN
			true
#else
			false
#endif
		);
		_state.coolingTemperaturePresent.set(
#ifdef DALLAS_SENSOR_COOLING_PIN
			true
#else
			false
#endif
		);
		_state.circIVPresent.set(
#ifdef INA226_CIRCULATION_ADDRESS
			true
#else
			false
#endif
		);
		_state.coolingIVPresent.set(
#ifdef INA226_COOLING_ADDRESS
			true
#else
			false
#endif
		);
		_state.heatingIVPresent.set(
#ifdef INA226_HEATING_ADDRESS
			true
#else
			false
#endif
		);
		_state.status.set(String("Start: ") + resetReasonToString(esp_reset_reason()));
	}

	_stopAllOutputs();
}

void Controller::loop() {
	unsigned long now = millis();

	if (now - _lastSampleMs >= SENSOR_SAMPLE_INTERVAL_MS) {
		_lastSampleMs = now;
		_sampleSensors();
	}

	_runMode();
	_updateOutputLedIndicators();

	if (now - _lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
		_lastSerialPrintMs = now;
		StateGuard guard(_state);
		Serial.printf("[State] mode=%-16s status=%-14s circ=%3u",
			_state.mode.get().c_str(),
			_state.status.get().c_str(),
			_state.circSpeed.get());

#ifdef COOLING_PWM_PIN
		Serial.printf(" cool=%3u", _state.coolingSpeed.get());
#endif

#ifdef HEATING_PWM_PIN
		Serial.printf(" heat=%3u", _state.heatingSpeed.get());
#endif

		Serial.printf(" outT=%5.1fC", _state.outTemperature.get());

#ifdef DALLAS_SENSOR_RETURNING_PIN
		Serial.printf(" retT=%5.1fC", _state.returnTemperature.get());
#endif

#ifdef FLOW_SENSOR_PIN
		Serial.printf(" flow=%5.2fL/min RSSI=%d", _state.flow.get(), _state.rssi.get());
#endif

		Serial.println();
	}
}

void Controller::checkButton() {
	bool pressed = (digitalRead(BUTTON_PIN) == LOW);
	if (pressed && !_buttonIsPressed) {
		_buttonPressMs = millis();
		_buttonIsPressed = true;
	} else if (!pressed) {
		_buttonIsPressed = false;
		_buttonPressMs = 0;
	} else if (pressed && _buttonIsPressed && _buttonPressMs > 0) {
		if (millis() - _buttonPressMs >= BUTTON_HOLD_MS) {
			Serial.println("[Controller] Button held 5s - requesting WiFi reset");
			_buttonPressMs = 0;
			_connectivity.resetWifi();
		}
	}
}

void Controller::updateLeds(bool wifiConnected) {
#ifdef BUILTIN_LED_PIN
	unsigned long now = millis();
	if (!wifiConnected) {
		if (now - _lastLedToggleMs >= WIFI_LED_TOGGLE_INTERVAL_MS) {
			_lastLedToggleMs = now;
			_builtinLedState = !_builtinLedState;
			digitalWrite(BUILTIN_LED_PIN, _builtinLedState ? HIGH : LOW);
		}
	} else {
		_builtinLedState = true;
		digitalWrite(BUILTIN_LED_PIN, HIGH);
	}
#endif
}

void Controller::setMode(const String& newMode) {
	if (!_state.isValidMode(newMode)) {
		_triggerError("Invalid mode: " + newMode);
		return;
	}

	bool calibrationError = false;
	{
		StateGuard guard(_state);
		if (_state.mode.get() == MODE_FLOW_CALIBRATION && newMode != MODE_FLOW_CALIBRATION) {
			uint64_t pulsesDelta = _state.flowPulsesFiltered.get() - _calibStartPulses;
			if (pulsesDelta == 0 || _state.flowPulsesFilteredPerSec.get() < _state.minFlowPulsesPerSec.get()) {
				calibrationError = true;
			} else {
				_state.calibrationFlowPulses.set((unsigned int)pulsesDelta);
				_state.status.set("Calibrated");
				Preferences prefs;
				prefs.begin(PREFS_NAMESPACE, false);
				_state.calibrationFlowPulses.save(prefs);
				prefs.end();
			}
		}
	}
	if (calibrationError) {
		_triggerError("Calibration failed: no flow");
		return;
	}

	{
		StateGuard guard(_state);
		_state.mode.set(newMode);
	}
	_clearError();
	_modeStartMs = millis();
	_lastTempAdjMs = 0;
	_lastFlowTestStepMs = 0;

	if (newMode == MODE_STOP) {
		_stopAllOutputs();
		StateGuard guard(_state);
		_state.status.set("Stopped");
	} else if (newMode == MODE_MANUAL_CIRC) {
		uint8_t sp;
		{
			StateGuard guard(_state);
			sp = _state.circSpeedSetPoint.get();
			_state.status.set("Circulating");
		}
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		_setCircSpeed(sp);
		_lastValidFlowMs = millis();
	} else if (newMode == MODE_MANUAL_COOL) {
		uint8_t circSp, coolSp;
		{
			StateGuard guard(_state);
			circSp = _state.circSpeedSetPoint.get();
			coolSp = _state.coolingSpeedSetPoint.get();
			_state.status.set("Cooling");
		}
		_setHeatingSpeed(0);
		_setCircSpeed(circSp);
		_setCoolingSpeed(coolSp);
		_lastValidFlowMs = millis();
	} else if (newMode == MODE_MANUAL_HEAT) {
		uint8_t circSp, heatSp;
		{
			StateGuard guard(_state);
			circSp = _state.circSpeedSetPoint.get();
			heatSp = _state.heatingSpeedSetPoint.get();
			_state.status.set("Heating");
		}
		_setCoolingSpeed(0);
		_setCircSpeed(circSp);
		_setHeatingSpeed(heatSp);
		_lastValidFlowMs = millis();
	} else if (newMode == MODE_TEMPERATURE) {
#ifdef METHOD_VARIABLE_CIRCULATION_SPEED
		_setCircSpeed(PUMP_MAX_SPEED);
#else
		uint8_t circSp;
		{
			StateGuard guard(_state);
			circSp = _state.circSpeedSetPoint.get();
		}
		_setCircSpeed(circSp);
#endif
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		{
			StateGuard guard(_state);
			_state.status.set("Circulating");
		}
		_lastTempAdjMs = millis();
		_lastValidFlowMs = millis();
	} else if (newMode == MODE_TEMPERATURE_PREPARE) {
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		{
			StateGuard guard(_state);
			_state.status.set("Preparing");
		}
	} else if (newMode == MODE_FLOW_CALIBRATION) {
		uint8_t circSp;
		{
			StateGuard guard(_state);
			_calibStartPulses = _state.flowPulsesFiltered.get();
			circSp = _state.circSpeedSetPoint.get();
			_state.status.set("Calibrating");
		}
		_calibStartMs = millis();
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		_setCircSpeed(circSp);
	} else if (newMode == MODE_FLOW_TEST) {
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		_setCircSpeed(FLOW_TEST_MAX_SPEED);
		_lastFlowTestStepMs = millis();
		StateGuard guard(_state);
		_state.status.set("Flow Test");
	}

	Serial.printf("[Controller] Mode changed to: %s\n", newMode.c_str());
}

void Controller::notifyConfigChanged() {
	if (_onConfigUpdated) _onConfigUpdated();
}

void Controller::_triggerError(const String& cause) {
	{
		StateGuard guard(_state);
		_state.error.set(true);
		_state.status.set("Error: " + cause);
		_state.mode.set(MODE_STOP);
	}
	_inError = true;
	_errorCause = cause;
	_stopAllOutputs();
#ifdef RED_LED_PIN
	digitalWrite(RED_LED_PIN, HIGH);
#endif
	Serial.printf("[Controller] ERROR: %s\n", cause.c_str());
	if (_onTelemetryUpdated) _onTelemetryUpdated();
}

void Controller::_clearError() {
	_inError = false;
	_errorCause = "";
	{
		StateGuard guard(_state);
		_state.error.set(false);
	}
#ifdef RED_LED_PIN
	digitalWrite(RED_LED_PIN, LOW);
#endif
	if (_onTelemetryUpdated) _onTelemetryUpdated();
}

void Controller::_setCircSpeed(uint8_t speed) {
	{
		StateGuard guard(_state);
		_state.circSpeed.set(speed);
	}
#ifdef CIRCULATION_TB6612_AIN1_PIN
	if (_circMotor) {
		_circMotor->drive(speed);
	}
#else
	analogWrite(CIRCULATION_PWM_PIN, speed);
#endif
	if (speed > 0 && _circActiveSinceMs == 0) _circActiveSinceMs = millis();
	if (speed == 0) _circActiveSinceMs = 0;
}

void Controller::_setCoolingSpeed(uint8_t speed) {
#ifdef COOLING_PWM_PIN
	analogWrite(COOLING_PWM_PIN, speed);
#else
	speed = 0;
#endif
	{
		StateGuard guard(_state);
		_state.coolingSpeed.set(speed);
	}
	if (speed > 0 && _coolingActiveSinceMs == 0) _coolingActiveSinceMs = millis();
	if (speed == 0) _coolingActiveSinceMs = 0;
}

void Controller::_setHeatingSpeed(uint8_t speed) {
#ifdef HEATING_PWM_PIN
	analogWrite(HEATING_PWM_PIN, speed);
#else
	speed = 0;
#endif
	{
		StateGuard guard(_state);
		_state.heatingSpeed.set(speed);
	}
	if (speed > 0 && _heatingActiveSinceMs == 0) _heatingActiveSinceMs = millis();
	if (speed == 0) _heatingActiveSinceMs = 0;
}

void Controller::_stopAllOutputs() {
	_setCircSpeed(0);
	_setCoolingSpeed(0);
	_setHeatingSpeed(0);
}

void Controller::_updateOutputLedIndicators() {
	uint8_t circSpd, coolSpd, heatSpd;
	{
		StateGuard guard(_state);
		circSpd = _state.circSpeed.get();
		coolSpd = _state.coolingSpeed.get();
		heatSpd = _state.heatingSpeed.get();
	}

#ifdef GREEN_LED_PIN
	if (circSpd > 0) {
		if (coolSpd > 0) {
			unsigned long now = millis();
			if (now - _lastLedToggleMs >= LED_TOGGLE_INTERVAL_MS) {
				_lastLedToggleMs = now;
				digitalWrite(GREEN_LED_PIN, digitalRead(GREEN_LED_PIN) ? LOW : HIGH);
			}
		} else {
			digitalWrite(GREEN_LED_PIN, HIGH);
		}
	} else {
		digitalWrite(GREEN_LED_PIN, LOW);
	}
#endif

#ifdef RED_LED_PIN
	if (!_inError && heatSpd > 0) {
		unsigned long now = millis();
		if (now - _lastLedToggleMs >= LED_TOGGLE_INTERVAL_MS) {
			_lastLedToggleMs = now;
			digitalWrite(RED_LED_PIN, digitalRead(RED_LED_PIN) ? LOW : HIGH);
		}
	}
#endif
}

bool Controller::_readTemperatureOnce(DallasTemperature& sensor, float& outVal) {
	sensor.requestTemperatures();
	float val = sensor.getTempCByIndex(0);
	if (val != DEVICE_DISCONNECTED_C && val >= TEMP_MIN_CELSIUS && val <= TEMP_MAX_CELSIUS) {
		outVal = val;
		return true;
	}
	return false;
}

bool Controller::_readTemperatureRetry(DallasTemperature& sensor, float& outVal) {
	for (int attempt = 0; attempt < TEMP_RETRY_COUNT; attempt++) {
		if (_readTemperatureOnce(sensor, outVal)) return true;
		delay(50);
	}
	return false;
}

void Controller::_calculateDerivedMetrics(uint64_t rawNow, uint64_t filteredNow, unsigned long nowMs) {
	unsigned long dt = nowMs - _prevPulsesMs;
	if (dt == 0) return;

	uint64_t rawDiff = rawNow - _prevRawPulses;
	uint64_t filteredDiff = filteredNow - _prevFilteredPulses;

	unsigned int rawPerSec = (unsigned int)((rawDiff * 1000ULL) / dt);
	unsigned int filteredPerSec = (unsigned int)((filteredDiff * 1000ULL) / dt);

	unsigned int calPulses, calVolume;
	float outTemp, returnTemp;
	{
		StateGuard guard(_state);
		_state.flowPulsesRawPerSec.set(rawPerSec);
		_state.flowPulsesFilteredPerSec.set(filteredPerSec);
		calPulses = _state.calibrationFlowPulses.get();
		calVolume = _state.calibrationVolume.get();
		outTemp = _state.outTemperature.get();
		returnTemp = _state.returnTemperature.get();
	}

	float flowLpm = 0.0f;
	if (calPulses > 0) {
		flowLpm = ((float)filteredPerSec * (float)calVolume * 60.0f) / ((float)calPulses * 1000.0f);
	}
	float deltaC = returnTemp - outTemp;
	float coolingPowerW = deltaC * WATER_SPECIFIC_HEAT_J_PER_KG_C * flowLpm / SECONDS_PER_MINUTE;
	{
		StateGuard guard(_state);
		_state.flow.set(flowLpm);
		_state.temperatureDelta.set(deltaC);
		_state.coolingPower.set(coolingPowerW);
	}

	_prevRawPulses = rawNow;
	_prevFilteredPulses = filteredNow;
	_prevPulsesMs = nowMs;
}

void Controller::_checkIvLimitsAndFaults(unsigned long now) {
	int circCurrent = 0, coolCurrent = 0, heatCurrent = 0;
	unsigned int circVoltage = 0, coolVoltage = 0, heatVoltage = 0;
	uint8_t circSpd = 0, coolSpd = 0, heatSpd = 0;
	unsigned int maxCircCurrent, minCircCurrent, minCircVoltage;
	unsigned int maxCoolingCurrent, minCoolingCurrent, minCoolingVoltage;
	unsigned int maxHeatingCurrent, minHeatingCurrent, minHeatingVoltage;
	bool circIvPresent, coolingIvPresent, heatingIvPresent;

	{
		StateGuard guard(_state);
		circCurrent = _state.circCurrent.get();
		coolCurrent = _state.coolingCurrent.get();
		heatCurrent = _state.heatingCurrent.get();
		circVoltage = _state.circVoltage.get();
		coolVoltage = _state.coolingVoltage.get();
		heatVoltage = _state.heatingVoltage.get();
		circSpd = _state.circSpeed.get();
		coolSpd = _state.coolingSpeed.get();
		heatSpd = _state.heatingSpeed.get();
		maxCircCurrent = _state.maxCircCurrent.get();
		minCircCurrent = _state.minCircCurrent.get();
		minCircVoltage = _state.minCircVoltage.get();
		maxCoolingCurrent = _state.maxCoolingCurrent.get();
		minCoolingCurrent = _state.minCoolingCurrent.get();
		minCoolingVoltage = _state.minCoolingVoltage.get();
		maxHeatingCurrent = _state.maxHeatingCurrent.get();
		minHeatingCurrent = _state.minHeatingCurrent.get();
		minHeatingVoltage = _state.minHeatingVoltage.get();
		circIvPresent = _state.circIVPresent.get();
		coolingIvPresent = _state.coolingIVPresent.get();
		heatingIvPresent = _state.heatingIVPresent.get();
	}

	if (circIvPresent) {
		if (circCurrent > (int)maxCircCurrent) {
			_triggerError("Circulation current exceeded");
			return;
		}
		if (circSpd > 0 && circVoltage < minCircVoltage) {
			_triggerError("Circulation voltage too low");
			return;
		}
		if (circSpd > 0 && _circActiveSinceMs > 0 && (now - _circActiveSinceMs) >= OUTPUT_RAMP_UP_MS && circCurrent < (int)minCircCurrent) {
			_triggerError("Circulation current too low");
			return;
		}
	}

	if (coolingIvPresent) {
		if (coolCurrent > (int)maxCoolingCurrent) {
			_triggerError("Cooling current exceeded");
			return;
		}
		if (coolSpd > 0 && coolVoltage < minCoolingVoltage) {
			_triggerError("Cooling voltage too low");
			return;
		}
		if (coolSpd > 0 && _coolingActiveSinceMs > 0 && (now - _coolingActiveSinceMs) >= OUTPUT_RAMP_UP_MS && coolCurrent < (int)minCoolingCurrent) {
			_triggerError("Cooling current too low");
			return;
		}
	}

	if (heatingIvPresent) {
		if (heatCurrent > (int)maxHeatingCurrent) {
			_triggerError("Heating current exceeded");
			return;
		}
		if (heatSpd > 0 && heatVoltage < minHeatingVoltage) {
			_triggerError("Heating voltage too low");
			return;
		}
		if (heatSpd > 0 && _heatingActiveSinceMs > 0 && (now - _heatingActiveSinceMs) >= OUTPUT_RAMP_UP_MS && heatCurrent < (int)minHeatingCurrent) {
			_triggerError("Heating current too low");
			return;
		}
	}
}

void Controller::_sampleSensors() {
	unsigned long now = millis();
	_state.setMetricsInitialized(true);

	uint64_t rawNow = 0;
	uint64_t filteredNow = 0;
#ifdef FLOW_SENSOR_PIN
	rawNow = _flowSensor.getRawPulses();
	filteredNow = _flowSensor.getFilteredPulses();
#endif
	{
		StateGuard guard(_state);
		_state.flowPulsesRaw.set(rawNow);
		_state.flowPulsesFiltered.set(filteredNow);
	}

	if (_prevPulsesMs == 0) {
		_prevRawPulses = rawNow;
		_prevFilteredPulses = filteredNow;
		_prevPulsesMs = now;
	} else {
		_calculateDerivedMetrics(rawNow, filteredNow, now);
	}

#ifdef DALLAS_SENSOR_OUTGOING_PIN
	float outTemp = 0.0f;
	if (!_readTemperatureRetry(_tempOut, outTemp)) {
		_triggerError("Outgoing temperature sensor failure");
		return;
	}
	{
		StateGuard guard(_state);
		outTemp += _state.outTemperatureCalibrationOffset.get();
		_state.outTemperature.set(outTemp);
	}
#else
	_triggerError("Outgoing temperature sensor missing");
	return;
#endif

#ifdef DALLAS_SENSOR_RETURNING_PIN
	float returnTemp = 0.0f;
	bool returnOk = false;
#ifdef METHOD_VARIABLE_CIRCULATION_SPEED
	returnOk = _readTemperatureRetry(_tempReturn, returnTemp);
#else
	returnOk = _readTemperatureOnce(_tempReturn, returnTemp);
#endif
	if (returnOk) {
		StateGuard guard(_state);
		_state.returnTemperaturePresent.set(true);
		returnTemp += _state.returnTemperatureCalibrationOffset.get();
		_state.returnTemperature.set(returnTemp);
	} else {
#ifdef METHOD_VARIABLE_CIRCULATION_SPEED
		_triggerError("Return temperature sensor failure");
		return;
#else
		StateGuard guard(_state);
		_state.returnTemperaturePresent.set(false);
#endif
	}
#else
#ifdef METHOD_VARIABLE_CIRCULATION_SPEED
	_triggerError("Return temperature sensor missing");
	return;
#endif
#endif

#ifdef DALLAS_SENSOR_COOLING_PIN
	float coolingTemp = 0.0f;
	if (_readTemperatureOnce(_tempCooling, coolingTemp)) {
		StateGuard guard(_state);
		_state.coolingTemperaturePresent.set(true);
		coolingTemp += _state.coolingTemperatureCalibrationOffset.get();
		_state.coolingTemperature.set(coolingTemp);
	} else {
		StateGuard guard(_state);
		_state.coolingTemperaturePresent.set(false);
	}
#endif

#ifdef INA226_CIRCULATION_ADDRESS
	if (_inaCirc.overflow) {
		_triggerError("INA226 circulation overflow");
		return;
	}
	{
		StateGuard guard(_state);
		_state.circVoltage.set((unsigned int)(_inaCirc.getBusVoltage_V() * 1000.0f));
		_state.circCurrent.set((int)_inaCirc.getCurrent_mA());
	}
#endif
#ifdef INA226_COOLING_ADDRESS
	if (_inaCooling.overflow) {
		_triggerError("INA226 cooling overflow");
		return;
	}
	{
		StateGuard guard(_state);
		_state.coolingVoltage.set((unsigned int)(_inaCooling.getBusVoltage_V() * 1000.0f));
		_state.coolingCurrent.set((int)_inaCooling.getCurrent_mA());
	}
#endif
#ifdef INA226_HEATING_ADDRESS
	if (_inaHeating.overflow) {
		_triggerError("INA226 heating overflow");
		return;
	}
	{
		StateGuard guard(_state);
		_state.heatingVoltage.set((unsigned int)(_inaHeating.getBusVoltage_V() * 1000.0f));
		_state.heatingCurrent.set((int)_inaHeating.getCurrent_mA());
	}
#endif

	_checkIvLimitsAndFaults(now);

	if (_onTelemetryUpdated) _onTelemetryUpdated();
}

void Controller::_runMode() {
	if (_inError) return;

	String currentMode;
	unsigned int sysTime, minFlow;
	float outTemp, setPoint, returnTemp;
	unsigned int filteredPerSec;
	bool returnPresent;
	bool heatingPresent;
	uint8_t circSpd, coolSpd, heatSpd;
	uint8_t circSetPoint, coolSetPoint, heatSetPoint;
	{
		StateGuard guard(_state);
		currentMode = _state.mode.get();
		sysTime = _state.systemTime.get();
		minFlow = _state.minFlowPulsesPerSec.get();
		outTemp = _state.outTemperature.get();
		setPoint = _state.temperatureSetPoint.get();
		returnTemp = _state.returnTemperature.get();
		returnPresent = _state.returnTemperaturePresent.get();
		heatingPresent = _state.heatingPresent.get();
		filteredPerSec = _state.flowPulsesFilteredPerSec.get();
		circSpd = _state.circSpeed.get();
		coolSpd = _state.coolingSpeed.get();
		heatSpd = _state.heatingSpeed.get();
		circSetPoint = _state.circSpeedSetPoint.get();
		coolSetPoint = _state.coolingSpeedSetPoint.get();
		heatSetPoint = _state.heatingSpeedSetPoint.get();
	}

	unsigned long now = millis();
	unsigned long elapsed = (now - _modeStartMs) / 1000;

	auto flowGuardCheck = [&]() {
#ifdef FLOW_SENSOR_PIN
		if (filteredPerSec >= minFlow) {
			_lastValidFlowMs = now;
		}
		if ((now - _lastValidFlowMs) / 1000 >= sysTime) {
			_triggerError("No flow");
			return false;
		}
#endif
		return true;
	};

	if (currentMode == MODE_STOP) {
		_stopAllOutputs();
		if (returnPresent && elapsed >= TEMPERATURE_CALIBRATION_EQUILIBRIUM_SECONDS) {
			float absDelta;
			{
				StateGuard guard(_state);
				absDelta = fabsf(_state.temperatureDelta.get());
				if (absDelta > TEMPERATURE_CALIBRATION_DELTA_WARNING_THRESHOLD_C) {
					_state.status.set(TEMPERATURE_CALIBRATION_WARNING_STATUS_TEXT);
				} else {
					_state.status.set("Stopped");
				}
			}
		}
	} else if (currentMode == MODE_MANUAL_CIRC) {
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		if (circSpd != circSetPoint) _setCircSpeed(circSetPoint);
		if (!flowGuardCheck()) return;
		StateGuard guard(_state);
		_state.status.set("Circulating");
	} else if (currentMode == MODE_MANUAL_COOL) {
		if (circSpd != circSetPoint) _setCircSpeed(circSetPoint);
		if (coolSpd != coolSetPoint) _setCoolingSpeed(coolSetPoint);
		_setHeatingSpeed(0);
		if (!flowGuardCheck()) return;
		StateGuard guard(_state);
		_state.status.set("Cooling");
	} else if (currentMode == MODE_MANUAL_HEAT) {
		if (circSpd != circSetPoint) _setCircSpeed(circSetPoint);
		if (heatSpd != heatSetPoint) _setHeatingSpeed(heatSetPoint);
		_setCoolingSpeed(0);
		if (!flowGuardCheck()) return;
		StateGuard guard(_state);
		_state.status.set("Heating");
	} else if (currentMode == MODE_TEMPERATURE) {
#ifdef METHOD_VARIABLE_CIRCULATION_SPEED
		if (filteredPerSec < minFlow && circSpd < PUMP_MAX_SPEED) {
			_setCircSpeed((uint8_t)min((int)circSpd + VARIABLE_SPEED_STEP, PUMP_MAX_SPEED));
			circSpd = (uint8_t)min((int)circSpd + VARIABLE_SPEED_STEP, PUMP_MAX_SPEED);
		}
		if (now - _lastTempAdjMs >= (unsigned long)sysTime * 1000UL) {
			_lastTempAdjMs = now;
#ifdef FLOW_SENSOR_PIN
			if (returnTemp < setPoint && filteredPerSec > minFlow) {
				_setCircSpeed((uint8_t)max((int)circSpd - VARIABLE_SPEED_STEP, 0));
			} else if (returnTemp > setPoint) {
				_setCircSpeed((uint8_t)min((int)circSpd + VARIABLE_SPEED_STEP, PUMP_MAX_SPEED));
			}
#else
			if (returnTemp < setPoint && circSpd > circSetPoint) {
				_setCircSpeed((uint8_t)max((int)circSpd - VARIABLE_SPEED_STEP, (int)circSetPoint));
			} else if (returnTemp > setPoint) {
				_setCircSpeed((uint8_t)min((int)circSpd + VARIABLE_SPEED_STEP, PUMP_MAX_SPEED));
			}
#endif
		}
#else
		if (circSpd != circSetPoint) _setCircSpeed(circSetPoint);
#endif
		if (coolSpd > 0) {
			if (outTemp < setPoint + COOLING_HYSTERESIS_STOP) {
				_setCoolingSpeed(0);
			}
		} else if (heatSpd == 0 && outTemp > setPoint + COOLING_HYSTERESIS_START) {
			_setCoolingSpeed(coolSetPoint);
		}

		if (heatSpd > 0) {
			if (outTemp > setPoint + HEATING_HYSTERESIS_STOP) {
				_setHeatingSpeed(0);
			}
		} else if (heatingPresent && coolSpd == 0 && outTemp < setPoint + HEATING_HYSTERESIS_START) {
			_setHeatingSpeed(heatSetPoint);
		}

		{
			StateGuard guard(_state);
			if (_state.coolingSpeed.get() > 0) {
				// check if cooling water is colder than the set point, if not, warn the user that cooling is not effective
				if (_state.coolingTemperaturePresent.get() && _state.coolingTemperature.get() > setPoint - INEFFECTIVE_COOLING_WARNING_DELTA_C) {
					_state.status.set("Cooling (ineffective)");
				} else {
					_state.status.set("Cooling");
				}
			}
			else if (_state.heatingSpeed.get() > 0) {
				_state.status.set("Heating");
			}
			else {
				_state.status.set("Circulating");
			}
		}
		if (!flowGuardCheck()) return;
	} else if (currentMode == MODE_TEMPERATURE_PREPARE) {
		_setCoolingSpeed(0);
		_setHeatingSpeed(0);
		if (outTemp < setPoint + HEATING_HYSTERESIS_START) {
			if (circSpd != circSetPoint) {
				_setCircSpeed(circSetPoint);
			}
			StateGuard guard(_state);
			_state.status.set("Preparing");
		} else if (circSpd > 0 && outTemp > setPoint + HEATING_HYSTERESIS_STOP) {
			_setCircSpeed(0);
			StateGuard guard(_state);
			_state.status.set("Idle");
		}
	} else if (currentMode == MODE_FLOW_CALIBRATION) {
		if (elapsed >= sysTime && filteredPerSec < minFlow) {
			_triggerError("No flow during calibration");
		}
	} else if (currentMode == MODE_FLOW_TEST) {
		if (filteredPerSec < minFlow) {
			if (circSpd != FLOW_TEST_MAX_SPEED) _setCircSpeed(FLOW_TEST_MAX_SPEED);
			_lastFlowTestStepMs = now;
		} else {
			unsigned long intervalMs = (unsigned long)((circSpd == FLOW_TEST_MAX_SPEED ? FLOW_TEST_FIRST_STEP_INTERVAL_RATIO : FLOW_TEST_STEP_INTERVAL_RATIO) * (float)sysTime * 1000.0f);
			if (now - _lastFlowTestStepMs >= intervalMs) {
				_lastFlowTestStepMs = now;
				_setCircSpeed((uint8_t)max((int)circSpd - FLOW_TEST_STEP, 0));
			}
		}
	}
}
