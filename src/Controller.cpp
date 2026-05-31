#include "Controller.h"
#include "Connectivity.h"
#include <Wire.h>
#include <math.h>

#define SERIAL_PRINT_INTERVAL_MS		5000
// #define SKIP_SPEED_MODE_FLOW_CHECK // for testing

Controller::Controller(State& state, Connectivity& connectivity)
	: _state(state),
	  _connectivity(connectivity),
	  _flowSensor(FLOW_SENSOR_PIN),
	  _owOut(DALLAS_SENSOR_OUTGOING_PIN),
	  _owReturn(DALLAS_SENSOR_RETURNING_PIN),
	  _tempOut(&_owOut),
	  _tempReturn(&_owReturn),
	  _ina226(INA226_I2C_ADDRESS),
	  _lastSampleMs(0),
	  _prevRawPulses(0),
	  _prevFilteredPulses(0),
	  _prevPulsesMs(0),
	  _modeStartMs(0),
	  _currentSpeed(PUMP_MAX_SPEED),
	  _calibStartPulses(0),
	  _calibStartMs(0),
	  _inError(false),
	  _buttonPressMs(0),
	  _buttonIsPressed(false),
	  _lastLedToggleMs(0),
	  _builtinLedState(false),
	  _lastTempAdjMs(0),
	  _lastFlowTestStepMs(0),
	  _lastSerialPrintMs(0) {
}

void Controller::begin() {
	// init LEDs
	pinMode(RED_LED_PIN, OUTPUT);
	pinMode(GREEN_LED_PIN, OUTPUT);
	pinMode(BUILTIN_LED_PIN, OUTPUT);
	digitalWrite(RED_LED_PIN, LOW);
	digitalWrite(GREEN_LED_PIN, LOW);
	digitalWrite(BUILTIN_LED_PIN, LOW);

	// init button with pullup
	pinMode(BUTTON_PIN, INPUT_PULLUP);

#ifdef MOTOR_DRIVER_TB6612_AIN1_PIN
	_motor = new Motor(MOTOR_DRIVER_TB6612_AIN1_PIN, MOTOR_DRIVER_TB6612_AIN2_PIN,
	                   MOTOR_DRIVER_PWM_PIN, 1, MOTOR_DRIVER_TB6612_STBY_PIN);
#else
	pinMode(MOTOR_DRIVER_PWM_PIN, OUTPUT);
	analogWrite(MOTOR_DRIVER_PWM_PIN, 0);
#endif

	_flowSensor.begin();
	_tempOut.begin();
	_tempReturn.begin();

	Wire.begin(); // SDA=21, SCL=22
	_ina226.init();
	_ina226.setResistorRange(INA226_SHUNT_RESISTANCE, INA226_MAX_CURRENT_A);
	_ina226.setAverage(INA226_AVERAGE_128);
	_ina226.setConversionTime(INA226_CONV_TIME_204);
	// _ina226.setCorrectionFactor(1.0f);
	_ina226.waitUntilConversionCompleted(); // if you comment this line the first data might be zero

	String mode;
	{
		StateGuard guard(_state);
		mode = _state.mode.get();
	}
	setMode(mode); // start with last mode
	// setMode(MODE_STOP); // start with STOP
}

void Controller::loop() {
	unsigned long now = millis();

	// sensors
	if (now - _lastSampleMs >= SENSOR_SAMPLE_INTERVAL_MS) {
		_lastSampleMs = now;
		_sampleSensors();
	}

	// BL
	_runMode();

	// Serial report
	if (now - _lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
		_lastSerialPrintMs = now;
		StateGuard guard(_state);
		Serial.printf("[State] mode=%-12s status=%-12s speed=%3u outT=%5.1f°C retT=%5.1f°C flow=%5.2fL/min V=%4umV I=%4dmA\n",
			_state.mode.get().c_str(),
			_state.status.get().c_str(),
			_state.pumpSpeed.get(),
			_state.outTemperature.get(),
			_state.returnTemperature.get(),
			_state.flow.get(),
			_state.pumpVoltage.get(),
			_state.pumpCurrent.get());
	}
}

void Controller::checkButton() {
	bool pressed = (digitalRead(BUTTON_PIN) == LOW);
	if (pressed && !_buttonIsPressed) { // button was pressed
		_buttonPressMs = millis();
		_buttonIsPressed = true;
	} else if (!pressed) { // button is not pressed
		_buttonIsPressed = false;
		_buttonPressMs = 0;
	} else if (pressed && _buttonIsPressed && _buttonPressMs > 0) { // button is being held
		if (millis() - _buttonPressMs >= BUTTON_HOLD_MS) {
			Serial.println("[Controller] Button held 5s - requesting WiFi reset");
			_buttonPressMs = 0; // reset timer to avoid multiple triggers
			_connectivity.resetWifi();
		}
	}
}

void Controller::updateLeds(bool wifiConnected) {
	unsigned long now = millis();
	if (!wifiConnected) {
		if (now - _lastLedToggleMs >= (BUILTIN_LED_BLINK_PERIOD_MS / 2)) {
			_lastLedToggleMs = now;
			_builtinLedState = !_builtinLedState;
			digitalWrite(BUILTIN_LED_PIN, _builtinLedState ? HIGH : LOW);
		}
	} else {
		_builtinLedState = true;
		digitalWrite(BUILTIN_LED_PIN, HIGH);
	}

	uint8_t speed;
	{
		StateGuard guard(_state);
		speed = _state.pumpSpeed.get();
	}
	digitalWrite(GREEN_LED_PIN, speed > 0 ? HIGH : LOW);
}

void Controller::setMode(const String& newMode) {
	bool calibrationError = false;

	{
		StateGuard guard(_state);
		if (_state.mode.get() == MODE_CALIBRATION && newMode != MODE_CALIBRATION) { // is leaving calibration mode
			uint64_t pulsesDelta = _state.flowPulsesFiltered.get() - _calibStartPulses;
			unsigned int fps = _state.flowPulsesFilteredPerSec.get();
			if (pulsesDelta == 0 || fps < _state.minFlowPulsesPerSec.get()) {
				calibrationError = true;
			} else {
				_state.calibrationFlowPulses.set((unsigned int)pulsesDelta);
				_state.calibrationFlow.set(_state.flow.get());
				_state.status.set("calibrated");
				Serial.printf("[Controller] Calibration complete: %llu pulses, %.2f L/min\n",
				              pulsesDelta, _state.flow.get());

				Preferences prefs;
				prefs.begin(PREFS_NAMESPACE, false);
				{
					_state.calibrationFlowPulses.save(prefs);
					_state.calibrationFlow.save(prefs);
				}
				prefs.end();
			}
		}
	}

	{
		StateGuard guard(_state);
		_state.mode.set(newMode);
	}
	_clearError();
	_modeStartMs = millis();
	_currentSpeed = PUMP_MAX_SPEED;
	_lastTempAdjMs = 0;
	_lastFlowTestStepMs = 0;

	// init new mode
	if (newMode == MODE_CALIBRATION) { // mark start of calibration process
		uint8_t setPoint;
		{
			StateGuard guard(_state);
			_calibStartPulses = _state.flowPulsesFiltered.get();
			setPoint = _state.speedSetPoint.get();
			_state.status.set("calibrating");
		}
		_calibStartMs = millis();
		_setPumpSpeed(setPoint);
	} else if (newMode == MODE_SPEED) {
		uint8_t setPoint;
		{
			StateGuard guard(_state);
			setPoint = _state.speedSetPoint.get();
			_state.status.set("speed");
		}
		_currentSpeed = setPoint;
		_setPumpSpeed(_currentSpeed);
	} else if (newMode == MODE_TEMPERATURE) {
		// in temperature mode, pump speed will be adjusted in _runMode() based on temperature difference, so just set it to max for now
		_currentSpeed = PUMP_MAX_SPEED;
		_setPumpSpeed(_currentSpeed);
		{
			StateGuard guard(_state);
			_state.status.set("temperature");
		}
		_lastTempAdjMs = millis();
	} else if (newMode == MODE_STOP) {
		_setPumpSpeed(0);
		{
			StateGuard guard(_state);
			_state.status.set("stopped");
		}
	} else if (newMode == MODE_FLOW_TEST) {
		_currentSpeed = FLOW_TEST_MAX_SPEED;
		_setPumpSpeed(_currentSpeed);
		_lastFlowTestStepMs = millis();
		{
			StateGuard guard(_state);
			_state.status.set("flow test");
		}
	}

	if (calibrationError) {
		_triggerError("calibration ended with no flow");
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
		_state.status.set("error: " + cause);
		_state.mode.set(MODE_STOP);
	}
	_inError = true;
	_errorCause = cause;
	_setPumpSpeed(0);
	digitalWrite(RED_LED_PIN, HIGH);
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
	digitalWrite(RED_LED_PIN, LOW);
}

void Controller::_setPumpSpeed(uint8_t speed) {
	{
		StateGuard guard(_state);
		_state.pumpSpeed.set(speed);
	}
#ifdef MOTOR_DRIVER_TB6612_AIN1_PIN
	if (_motor) {
		// this is a pump, not a wheel, no need to use braking or handle reverse direction
		// if (speed == 0) {
		// 	_motor->brake();
		// } else {
		// 	_motor->drive(speed);
		// }
		_motor->drive(speed);
	}
#else
	analogWrite(MOTOR_DRIVER_PWM_PIN, speed);
#endif
}

bool Controller::_readTemperature(DallasTemperature& sensor, float& outVal) {
	for (int attempt = 0; attempt < TEMP_RETRY_COUNT; attempt++) {
		sensor.requestTemperatures();
		float val = sensor.getTempCByIndex(0);
		if (val != DEVICE_DISCONNECTED_C && val >= TEMP_MIN_CELSIUS && val <= TEMP_MAX_CELSIUS) {
			outVal = val;
			return true;
		}
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

void Controller::_sampleSensors() {
	unsigned long now = millis();

	uint64_t rawNow = _flowSensor.getRawPulses();
	uint64_t filteredNow = _flowSensor.getFilteredPulses();
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

	float outTemp = 0.0f;
	if (!_readTemperature(_tempOut, outTemp)) {
		_triggerError("outgoing temperature sensor failure");
		return;
	}
	float outTempOffset = 0.0f;
	{
		StateGuard guard(_state);
		outTempOffset = _state.outTemperatureCalibrationOffset.get();
	}
	outTemp += outTempOffset;
	{
		StateGuard guard(_state);
		_state.outTemperature.set(outTemp);
	}

	float returnTemp = 0.0f;
	if (!_readTemperature(_tempReturn, returnTemp)) {
		_triggerError("return temperature sensor failure");
		return;
	}
	float returnTempOffset = 0.0f;
	{
		StateGuard guard(_state);
		returnTempOffset = _state.returnTemperatureCalibrationOffset.get();
	}
	returnTemp += returnTempOffset;
	{
		StateGuard guard(_state);
		_state.returnTemperature.set(returnTemp);
	}

	if (_ina226.overflow) {
		_triggerError("INA226 overflow");
		return;
	}
	float busVoltageV = _ina226.getBusVoltage_V();
	float currentMaF = _ina226.getCurrent_mA(); // some times returned -1 due to noise or EMF, so important to support negative values for current so not to overflow.
	unsigned int voltageMs = (unsigned int)(busVoltageV * 1000.0f);
	int currentMa = (int)currentMaF;
	{
		StateGuard guard(_state);
		_state.pumpVoltage.set(voltageMs);
		_state.pumpCurrent.set(currentMa);
	}

	int maxCur;
	unsigned int minVolt;
	uint8_t spd;
	{
		StateGuard guard(_state);
		maxCur = (int)_state.maxCurrent.get();
		minVolt = _state.minVoltage.get();
		spd = _state.pumpSpeed.get();
	}
	if (currentMa > maxCur) {
		_triggerError("pump current exceeded");
		return;
	}
	if (voltageMs < minVolt && spd > 0) {
		_triggerError("pump voltage too low");
		return;
	}

	_state.setMetricsValid(true);

	if (_onTelemetryUpdated) _onTelemetryUpdated();
}

void Controller::_runMode() {
	if (_inError) return;

	String currentMode;
	unsigned int sysTime, minFlow;
	{
		StateGuard guard(_state);
		currentMode = _state.mode.get();
		sysTime = _state.systemTime.get();
		minFlow = _state.minFlowPulsesPerSec.get();
	}

	unsigned long now = millis();
	unsigned long elapsed = (now - _modeStartMs) / 1000;

	if (currentMode == MODE_STOP) {
		if (elapsed >= TEMPERATURE_CALIBRATION_EQUILIBRIUM_SECONDS) {
			float absDelta;
			String status;
			{
				StateGuard guard(_state);
				absDelta = fabsf(_state.temperatureDelta.get());
				status = _state.status.get();
			}
			if (absDelta > TEMPERATURE_CALIBRATION_DELTA_WARNING_THRESHOLD_C && status != TEMPERATURE_CALIBRATION_WARNING_STATUS_TEXT) {
				StateGuard guard(_state);
				_state.status.set(TEMPERATURE_CALIBRATION_WARNING_STATUS_TEXT);
			}
		}
	} else if (currentMode == MODE_SPEED) {
		uint8_t setPoint;
		{
			StateGuard guard(_state);
			setPoint = _state.speedSetPoint.get();
		}
		// update speed
		if (setPoint != _currentSpeed) {
			_currentSpeed = setPoint;
			_setPumpSpeed(_currentSpeed);
		}
		// check for errors
#ifndef SKIP_SPEED_MODE_FLOW_CHECK
		if (elapsed >= sysTime) {
			unsigned int fps;
			{
				StateGuard guard(_state);
				fps = _state.flowPulsesFilteredPerSec.get();
			}
			if (fps < minFlow) {
				_triggerError("no flow in speed mode");
			}
		}
#endif // SKIP_SPEED_MODE_FLOW_CHECK
	} else if (currentMode == MODE_TEMPERATURE) {
		float returnTemp, setPoint;
		unsigned int filteredPerSec;
		{
			StateGuard guard(_state);
			returnTemp = _state.returnTemperature.get();
			setPoint = _state.temperatureSetPoint.get();
			filteredPerSec = _state.flowPulsesFilteredPerSec.get();
		}

		// Every sample: if flow too low, boost speed
		if (filteredPerSec < minFlow && _currentSpeed < PUMP_MAX_SPEED) {
			_currentSpeed = (uint8_t)min((int)_currentSpeed + TEMP_STEP, PUMP_MAX_SPEED);
			_setPumpSpeed(_currentSpeed);
		}

		// Every systemTime interval: temperature-based speed adjustment
		if (now - _lastTempAdjMs >= (unsigned long)sysTime * 1000UL) {
			_lastTempAdjMs = now;
			if (returnTemp < setPoint && filteredPerSec > minFlow) {
				_currentSpeed = (uint8_t)max((int)_currentSpeed - TEMP_STEP, 0);
				_setPumpSpeed(_currentSpeed);
			} else if (returnTemp > setPoint) {
				_currentSpeed = (uint8_t)min((int)_currentSpeed + TEMP_STEP, PUMP_MAX_SPEED);
				_setPumpSpeed(_currentSpeed);
			}
			// Equal: do nothing
		}

	} else if (currentMode == MODE_CALIBRATION) {
		// start and end are handled in setMode(); here just check that flow started within systemTime, otherwise trigger error
		if (elapsed >= sysTime) {
			unsigned int fps;
			{
				StateGuard guard(_state);
				fps = _state.flowPulsesFilteredPerSec.get();
			}
			if (fps < minFlow) {
				_triggerError("no flow during calibration");
			}
		}
	} else if (currentMode == MODE_FLOW_TEST) {
		unsigned int filteredPerSec;
		unsigned int systemTimeSec;
		{
			StateGuard guard(_state);
			filteredPerSec = _state.flowPulsesFilteredPerSec.get();
			systemTimeSec = _state.systemTime.get();
		}

		// Cycle start / restart: run at max speed until measured flow is above threshold.
		if (filteredPerSec < minFlow) {
			if (_currentSpeed != FLOW_TEST_MAX_SPEED) {
				_currentSpeed = FLOW_TEST_MAX_SPEED;
				_setPumpSpeed(_currentSpeed);
			}
			_lastFlowTestStepMs = now;
		} else if (filteredPerSec > minFlow) {
			unsigned long intervalMs = (unsigned long)( (_currentSpeed == FLOW_TEST_MAX_SPEED ? FLOW_TEST_FIRST_STEP_INTERVAL_RATIO : FLOW_TEST_STEP_INTERVAL_RATIO) * (float)systemTimeSec * 1000.0f);
			if (now - _lastFlowTestStepMs >= intervalMs) {
				_lastFlowTestStepMs = now;
				uint8_t newSpeed = (uint8_t)max((int)_currentSpeed - FLOW_TEST_STEP, 0);
				if (newSpeed != _currentSpeed) {
					_currentSpeed = newSpeed;
					_setPumpSpeed(_currentSpeed);
				}
			}
		}
	}
}
