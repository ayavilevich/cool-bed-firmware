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

State::State() : _metricsValid(false) {
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

void State::setMetricsValid(bool valid) {
	lock();
	_metricsValid = valid;
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
	speedSetPoint.load(prefs);
	temperatureSetPoint.load(prefs);
	outTemperatureCalibrationOffset.load(prefs);
	returnTemperatureCalibrationOffset.load(prefs);
	calibrationVolume.load(prefs);
	calibrationFlow.load(prefs);
	calibrationFlowPulses.load(prefs);
	systemTime.load(prefs);
	minFlowPulsesPerSec.load(prefs);
	maxCurrent.load(prefs);
	minVoltage.load(prefs);
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
	speedSetPoint.toJson(obj);
	temperatureSetPoint.toJson(obj);
	outTemperatureCalibrationOffset.toJson(obj);
	returnTemperatureCalibrationOffset.toJson(obj);
	calibrationVolume.toJson(obj);
	calibrationFlow.toJson(obj);
	calibrationFlowPulses.toJson(obj);
	systemTime.toJson(obj);
	minFlowPulsesPerSec.toJson(obj);
	maxCurrent.toJson(obj);
	minVoltage.toJson(obj);
}

// convert telemetry to JSON
void State::_telemetryToJson(JsonObject obj) const {
	status.toJson(obj);
	error.toJson(obj);
	pumpSpeed.toJson(obj);
	flowPulsesRaw.toJson(obj);
	flowPulsesFiltered.toJson(obj);
	pumpVoltage.toJson(obj);
	pumpCurrent.toJson(obj);
	outTemperature.toJson(obj);
	returnTemperature.toJson(obj);
	flowPulsesRawPerSec.toJson(obj);
	flowPulsesFilteredPerSec.toJson(obj);
	flow.toJson(obj);
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
	metricModelToJsonImpl(obj, speedSetPoint);
	metricModelToJsonImpl(obj, temperatureSetPoint);
	metricModelToJsonImpl(obj, outTemperatureCalibrationOffset);
	metricModelToJsonImpl(obj, returnTemperatureCalibrationOffset);
	metricModelToJsonImpl(obj, calibrationVolume);
	metricModelToJsonImpl(obj, calibrationFlow);
	metricModelToJsonImpl(obj, calibrationFlowPulses);
	metricModelToJsonImpl(obj, systemTime);
	metricModelToJsonImpl(obj, minFlowPulsesPerSec);
	metricModelToJsonImpl(obj, maxCurrent);
	metricModelToJsonImpl(obj, minVoltage);
}

void State::_telemetryModelToJson(JsonObject obj) const {
	metricModelToJsonImpl(obj, status);
	metricModelToJsonImpl(obj, error);
	metricModelToJsonImpl(obj, pumpSpeed);
	metricModelToJsonImpl(obj, flowPulsesRaw);
	metricModelToJsonImpl(obj, flowPulsesFiltered);
	metricModelToJsonImpl(obj, pumpVoltage);
	metricModelToJsonImpl(obj, pumpCurrent);
	metricModelToJsonImpl(obj, outTemperature);
	metricModelToJsonImpl(obj, returnTemperature);
	metricModelToJsonImpl(obj, flowPulsesRawPerSec);
	metricModelToJsonImpl(obj, flowPulsesFilteredPerSec);
	metricModelToJsonImpl(obj, flow);
	metricModelToJsonImpl(obj, temperatureDelta);
	metricModelToJsonImpl(obj, coolingPower);
}

// convert state to JSON
void State::toJson(JsonObject obj, bool includeConfig, bool includeTelemetry, bool excludeMqttConfig) const {
	if (includeConfig) {
		_configToJson(obj, excludeMqttConfig);
	}
	if (includeTelemetry && _metricsValid) {
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
	modes.add(MODE_STOP);
	modes.add(MODE_SPEED);
	modes.add(MODE_TEMPERATURE);
	modes.add(MODE_CALIBRATION);
	modes.add(MODE_FLOW_TEST);
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
	applyAndSave(mode);
	applyAndSave(speedSetPoint);
	applyAndSave(temperatureSetPoint);
	applyAndSave(outTemperatureCalibrationOffset);
	applyAndSave(returnTemperatureCalibrationOffset);
	applyAndSave(calibrationVolume);
	applyAndSave(calibrationFlow);
	applyAndSave(calibrationFlowPulses);
	applyAndSave(systemTime);
	applyAndSave(minFlowPulsesPerSec);
	applyAndSave(maxCurrent);
	applyAndSave(minVoltage);

	prefs.end();
	return changed;
}
