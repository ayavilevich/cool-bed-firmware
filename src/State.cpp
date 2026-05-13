#include "State.h"

State::State() {
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
}

// convert state to JSON
void State::toJson(JsonObject obj, bool includeConfig, bool includeTelemetry, bool excludeMqttConfig) const {
	if (includeConfig) {
		_configToJson(obj, excludeMqttConfig);
	}
	if (includeTelemetry) {
		_telemetryToJson(obj);
	}
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
