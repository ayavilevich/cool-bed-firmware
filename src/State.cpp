// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "State.h"

namespace {
template<typename T>
void metricModelToJsonImpl(JsonObject obj, const Metric<T>& metric) {
	JsonObject entry = obj[metric.getName()].template to<JsonObject>(); // the compiler does not know that "to" is a template method when analyzing the dependent type.
	entry["defaultValue"] = metric.getDefault();
	entry["min"] = metric.getMin();
	entry["max"] = metric.getMax();
	entry["units"] = metric.getUnits();
	entry["description"] = metric.getDescription();
}
}

State::State() : _metricsInitialized(false) {
	_mutex = xSemaphoreCreateMutex();
}

void State::begin() {
	loadConfig();
}

void State::lock() {
	xSemaphoreTake(_mutex, portMAX_DELAY);
}

void State::unlock() {
	xSemaphoreGive(_mutex);
}

void State::setMetricsInitialized(bool valid) {
	lock();
	_metricsInitialized = valid;
	unlock();
}

// load config from preferences to memory. Called at startup.
void State::loadConfig() {
	Preferences prefs;
	prefs.begin(PREFS_NAMESPACE, true); // read-only
	hostname.load(prefs);
	mqtt.load(prefs);
	mqttServer.load(prefs);
	mqttPort.load(prefs);
	mqttUsername.load(prefs);
	mqttPassword.load(prefs);
	mqttRootTopic.load(prefs);
	mqttHADiscovery.load(prefs);
	mqttHADiscoveryTopic.load(prefs);
	mode.load(prefs);
	circSpeedSetPoint.load(prefs);
	coolingSpeedSetPoint.load(prefs);
	heatingSpeedSetPoint.load(prefs);
	temperatureSetPoint.load(prefs);
	outTemperatureCalibrationOffset.load(prefs);
	returnTemperatureCalibrationOffset.load(prefs);
	coolingTemperatureCalibrationOffset.load(prefs);
	calibrationVolume.load(prefs);
	calibrationFlowPulses.load(prefs);
	systemTime.load(prefs);
	minFlowPulsesPerSec.load(prefs);
	maxCircCurrent.load(prefs);
	minCircCurrent.load(prefs);
	minCircVoltage.load(prefs);
	maxCoolingCurrent.load(prefs);
	minCoolingCurrent.load(prefs);
	minCoolingVoltage.load(prefs);
	maxHeatingCurrent.load(prefs);
	minHeatingCurrent.load(prefs);
	minHeatingVoltage.load(prefs);
	prefs.end();
}

// convert config to JSON
void State::_configToJson(JsonObject obj, bool excludeMqtt) const {
	hostname.toJson(obj);
	if (!excludeMqtt) {
		mqtt.toJson(obj);
		mqttServer.toJson(obj);
		mqttPort.toJson(obj);
		mqttUsername.toJson(obj);
		// Note: mqttPassword intentionally omitted from JSON responses
		mqttRootTopic.toJson(obj);
		mqttHADiscovery.toJson(obj);
		mqttHADiscoveryTopic.toJson(obj);
	}
	mode.toJson(obj);
	circSpeedSetPoint.toJson(obj);

#ifdef COOLING_PWM_PIN
	coolingSpeedSetPoint.toJson(obj);
#endif
#ifdef HEATING_PWM_PIN
	heatingSpeedSetPoint.toJson(obj);
#endif
	temperatureSetPoint.toJson(obj);
	outTemperatureCalibrationOffset.toJson(obj);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	returnTemperatureCalibrationOffset.toJson(obj);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	coolingTemperatureCalibrationOffset.toJson(obj);
#endif

#ifdef FLOW_SENSOR_PIN
	calibrationVolume.toJson(obj);
	calibrationFlowPulses.toJson(obj);
	minFlowPulsesPerSec.toJson(obj);
#endif

	systemTime.toJson(obj);

#ifdef INA226_CIRCULATION_ADDRESS
	maxCircCurrent.toJson(obj);
	minCircCurrent.toJson(obj);
	minCircVoltage.toJson(obj);
#endif
#ifdef INA226_COOLING_ADDRESS
	maxCoolingCurrent.toJson(obj);
	minCoolingCurrent.toJson(obj);
	minCoolingVoltage.toJson(obj);
#endif
#ifdef INA226_HEATING_ADDRESS
	maxHeatingCurrent.toJson(obj);
	minHeatingCurrent.toJson(obj);
	minHeatingVoltage.toJson(obj);
#endif
}

// convert telemetry to JSON
void State::_telemetryToJson(JsonObject obj) const {
	status.toJson(obj);
	error.toJson(obj);
	fwVersion.toJson(obj);
	rssi.toJson(obj);
	buildDateTime.toJson(obj);
	buildTimestamp.toJson(obj);
	circSpeed.toJson(obj);

#ifdef COOLING_PWM_PIN
	coolingSpeed.toJson(obj);
#endif
#ifdef HEATING_PWM_PIN
	heatingSpeed.toJson(obj);
#endif

#ifdef FLOW_SENSOR_PIN
	flowPulsesRaw.toJson(obj);
	flowPulsesFiltered.toJson(obj);
	flowPulsesRawPerSec.toJson(obj);
	flowPulsesFilteredPerSec.toJson(obj);
	flow.toJson(obj);
#endif

#ifdef INA226_CIRCULATION_ADDRESS
	circVoltage.toJson(obj);
	circCurrent.toJson(obj);
#endif
#ifdef INA226_COOLING_ADDRESS
	coolingVoltage.toJson(obj);
	coolingCurrent.toJson(obj);
#endif
#ifdef INA226_HEATING_ADDRESS
	heatingVoltage.toJson(obj);
	heatingCurrent.toJson(obj);
#endif

	outTemperature.toJson(obj);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	returnTemperature.toJson(obj);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	coolingTemperature.toJson(obj);
#endif

	flowSensorPresent.toJson(obj);
	coolingPresent.toJson(obj);
	heatingPresent.toJson(obj);
	coolingTemperaturePresent.toJson(obj);
	returnTemperaturePresent.toJson(obj);
	circIVPresent.toJson(obj);
	coolingIVPresent.toJson(obj);
	heatingIVPresent.toJson(obj);



	temperatureDelta.toJson(obj);
	coolingPower.toJson(obj);
}

void State::_configModelToJson(JsonObject obj, bool excludeMqtt) const {
	metricModelToJsonImpl(obj, hostname);
	if (!excludeMqtt) {
		metricModelToJsonImpl(obj, mqtt);
		metricModelToJsonImpl(obj, mqttServer);
		metricModelToJsonImpl(obj, mqttPort);
		metricModelToJsonImpl(obj, mqttUsername);
		metricModelToJsonImpl(obj, mqttPassword);
		metricModelToJsonImpl(obj, mqttRootTopic);
		metricModelToJsonImpl(obj, mqttHADiscovery);
		metricModelToJsonImpl(obj, mqttHADiscoveryTopic);
	}
	metricModelToJsonImpl(obj, mode);
	metricModelToJsonImpl(obj, circSpeedSetPoint);

#ifdef COOLING_PWM_PIN
	metricModelToJsonImpl(obj, coolingSpeedSetPoint);
#endif
#ifdef HEATING_PWM_PIN
	metricModelToJsonImpl(obj, heatingSpeedSetPoint);
#endif

	metricModelToJsonImpl(obj, temperatureSetPoint);
	metricModelToJsonImpl(obj, outTemperatureCalibrationOffset);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	metricModelToJsonImpl(obj, returnTemperatureCalibrationOffset);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	metricModelToJsonImpl(obj, coolingTemperatureCalibrationOffset);
#endif

#ifdef FLOW_SENSOR_PIN
	metricModelToJsonImpl(obj, calibrationVolume);
	metricModelToJsonImpl(obj, calibrationFlowPulses);
	metricModelToJsonImpl(obj, minFlowPulsesPerSec);
#endif

	metricModelToJsonImpl(obj, systemTime);

#ifdef INA226_CIRCULATION_ADDRESS
	metricModelToJsonImpl(obj, maxCircCurrent);
	metricModelToJsonImpl(obj, minCircCurrent);
	metricModelToJsonImpl(obj, minCircVoltage);
#endif
#ifdef INA226_COOLING_ADDRESS
	metricModelToJsonImpl(obj, maxCoolingCurrent);
	metricModelToJsonImpl(obj, minCoolingCurrent);
	metricModelToJsonImpl(obj, minCoolingVoltage);
#endif
#ifdef INA226_HEATING_ADDRESS
	metricModelToJsonImpl(obj, maxHeatingCurrent);
	metricModelToJsonImpl(obj, minHeatingCurrent);
	metricModelToJsonImpl(obj, minHeatingVoltage);
#endif
}

void State::_telemetryModelToJson(JsonObject obj) const {
	metricModelToJsonImpl(obj, status);
	metricModelToJsonImpl(obj, error);
	metricModelToJsonImpl(obj, fwVersion);
	metricModelToJsonImpl(obj, rssi);
	metricModelToJsonImpl(obj, buildDateTime);
	metricModelToJsonImpl(obj, buildTimestamp);
	metricModelToJsonImpl(obj, circSpeed);

#ifdef COOLING_PWM_PIN
	metricModelToJsonImpl(obj, coolingSpeed);
#endif
#ifdef HEATING_PWM_PIN
	metricModelToJsonImpl(obj, heatingSpeed);
#endif

#ifdef FLOW_SENSOR_PIN
	metricModelToJsonImpl(obj, flowPulsesRaw);
	metricModelToJsonImpl(obj, flowPulsesFiltered);
	metricModelToJsonImpl(obj, flowPulsesRawPerSec);
	metricModelToJsonImpl(obj, flowPulsesFilteredPerSec);
	metricModelToJsonImpl(obj, flow);
#endif

#ifdef INA226_CIRCULATION_ADDRESS
	metricModelToJsonImpl(obj, circVoltage);
	metricModelToJsonImpl(obj, circCurrent);
#endif
#ifdef INA226_COOLING_ADDRESS
	metricModelToJsonImpl(obj, coolingVoltage);
	metricModelToJsonImpl(obj, coolingCurrent);
#endif
#ifdef INA226_HEATING_ADDRESS
	metricModelToJsonImpl(obj, heatingVoltage);
	metricModelToJsonImpl(obj, heatingCurrent);
#endif

	metricModelToJsonImpl(obj, outTemperature);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	metricModelToJsonImpl(obj, returnTemperature);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	metricModelToJsonImpl(obj, coolingTemperature);
#endif

	metricModelToJsonImpl(obj, flowSensorPresent);
	metricModelToJsonImpl(obj, coolingPresent);
	metricModelToJsonImpl(obj, heatingPresent);
	metricModelToJsonImpl(obj, coolingTemperaturePresent);
	metricModelToJsonImpl(obj, returnTemperaturePresent);
	metricModelToJsonImpl(obj, circIVPresent);
	metricModelToJsonImpl(obj, coolingIVPresent);
	metricModelToJsonImpl(obj, heatingIVPresent);
	metricModelToJsonImpl(obj, temperatureDelta);
	metricModelToJsonImpl(obj, coolingPower);
}

// convert state to JSON
void State::toJson(JsonObject obj, bool includeConfig, bool includeTelemetry, bool excludeMqttConfig) const {
	if (includeConfig) {
		_configToJson(obj, excludeMqttConfig);
	}
	if (includeTelemetry && _metricsInitialized) {
		_telemetryToJson(obj);
	}
}

void State::toModelJson(JsonObject obj, bool includeConfig, bool includeTelemetry, bool excludeMqttConfig) const {
	if (includeConfig) {
		JsonObject configObj = obj["config"].to<JsonObject>();
		_configModelToJson(configObj, excludeMqttConfig);
	}
	if (includeTelemetry) {
		JsonObject telemetryObj = obj["telemetry"].to<JsonObject>();
		_telemetryModelToJson(telemetryObj);
	}

	JsonArray modes = obj["modes"].to<JsonArray>();
	if (isModeSupported(MODE_STOP)) modes.add(MODE_STOP);
	if (isModeSupported(MODE_MANUAL_CIRC)) modes.add(MODE_MANUAL_CIRC);
	if (isModeSupported(MODE_TEMPERATURE)) modes.add(MODE_TEMPERATURE);
	if (isModeSupported(MODE_MANUAL_COOL)) modes.add(MODE_MANUAL_COOL);
	if (isModeSupported(MODE_MANUAL_HEAT)) modes.add(MODE_MANUAL_HEAT);
	if (isModeSupported(MODE_FLOW_CALIBRATION)) modes.add(MODE_FLOW_CALIBRATION);
	if (isModeSupported(MODE_FLOW_TEST)) modes.add(MODE_FLOW_TEST);
}

bool State::isModeSupported(const String& mode) const {
	if (mode == MODE_STOP || mode == MODE_MANUAL_CIRC || mode == MODE_TEMPERATURE) {
		return true;
	}
	if (mode == MODE_MANUAL_COOL) {
#ifdef COOLING_PWM_PIN
		return true;
#else
		return false;
#endif
	}
	if (mode == MODE_MANUAL_HEAT) {
#ifdef HEATING_PWM_PIN
		return true;
#else
		return false;
#endif
	}
	if (mode == MODE_FLOW_CALIBRATION || mode == MODE_FLOW_TEST) {
#ifdef FLOW_SENSOR_PIN
		return true;
#else
		return false;
#endif
	}
	return false;
}

bool State::isValidMode(const String& mode) const {
	return isModeSupported(mode);
}

// convert JSON with config to State and Preferences
bool State::applyConfigJson(const JsonObjectConst& obj) {
	bool changed = false;
	Preferences prefs;
	prefs.begin(PREFS_NAMESPACE, false);

	auto applyAndSave = [&](auto& var) {
		String key = var.getName(); // get config name
		if (!obj[key].isNull()) { // check if config is in JSON
			var.fromJson(obj); // load value from JSON to memory
			var.save(prefs); // persist to preferences
			changed = true; // mark flag
		}
	};

	applyAndSave(hostname);
	applyAndSave(mqtt);
	applyAndSave(mqttServer);
	applyAndSave(mqttPort);
	applyAndSave(mqttUsername);
	applyAndSave(mqttPassword);
	applyAndSave(mqttRootTopic);
	applyAndSave(mqttHADiscovery);
	applyAndSave(mqttHADiscoveryTopic);
	if (!obj["mode"].isNull()) {
		String requestedMode = obj["mode"].as<String>();
		if (isValidMode(requestedMode)) {
			mode.set(requestedMode);
			mode.save(prefs);
			changed = true;
		}
	}
	applyAndSave(circSpeedSetPoint);

#ifdef COOLING_PWM_PIN
	applyAndSave(coolingSpeedSetPoint);
#endif
#ifdef HEATING_PWM_PIN
	applyAndSave(heatingSpeedSetPoint);
#endif

	applyAndSave(temperatureSetPoint);
	applyAndSave(outTemperatureCalibrationOffset);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	applyAndSave(returnTemperatureCalibrationOffset);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	applyAndSave(coolingTemperatureCalibrationOffset);
#endif

#ifdef FLOW_SENSOR_PIN
	applyAndSave(calibrationVolume);
	applyAndSave(calibrationFlowPulses);
	applyAndSave(minFlowPulsesPerSec);

#endif
	applyAndSave(systemTime);

#ifdef INA226_CIRCULATION_ADDRESS
	applyAndSave(maxCircCurrent);
	applyAndSave(minCircCurrent);
	applyAndSave(minCircVoltage);
#endif
#ifdef INA226_COOLING_ADDRESS
	applyAndSave(maxCoolingCurrent);
	applyAndSave(minCoolingCurrent);
	applyAndSave(minCoolingVoltage);
#endif
#ifdef INA226_HEATING_ADDRESS
	applyAndSave(maxHeatingCurrent);
	applyAndSave(minHeatingCurrent);
	applyAndSave(minHeatingVoltage);
#endif

	prefs.end();
	return changed;
}
