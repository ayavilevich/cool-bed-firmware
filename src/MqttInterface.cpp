// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "MqttInterface.h"

#define MQTT_RECONNECT_INTERVAL_MS 5000
#define MQTT_BUFFER_SIZE (15*1024) // our auto-discovery payload is very large, so need to increase the default 256B buffer
// autodiscovery consts
#define AD_NAME "Cool Bed"
#define AD_NAME_PREFIX "Cool Bed ("
#define AD_NAME_SUFFIX ")"
#define AD_MANUFACTURER "AY Garage"
#define AD_MODEL "Cool Bed v1"
#define AD_ORIGIN_NAME "cool-bed-firmware"

MqttInterface* MqttInterface::_instance = nullptr;

MqttInterface::MqttInterface(State& state, Controller& controller)
	: _state(state), _controller(controller),
	  _mqttClient(_wifiClient),
	  _enabled(false), _lastReconnectMs(0) {
	_instance = this;
}

void MqttInterface::begin() {
	{
		StateGuard guard(_state);
		_enabled = _state.mqtt.get();
	}
	if (!_enabled) return;

	_mqttClient.setCallback(_mqttCallback);
	_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

	_connect();
}

void MqttInterface::loop() {
	if (!_enabled) return;

	if (!_mqttClient.connected()) {
		unsigned long now = millis();
		if (now - _lastReconnectMs >= MQTT_RECONNECT_INTERVAL_MS) {
			Serial.println("[MQTT] Disconnected, attempting to reconnect...");
			_lastReconnectMs = now;
			_connect();
		}
	} else {
		_mqttClient.loop();
	}
}

void MqttInterface::publishState() {
	if (!_enabled || !_mqttClient.connected()) return;

	String rootTopic;
	{
		StateGuard guard(_state);
		rootTopic = _state.mqttRootTopic.get();
	}

	JsonDocument doc;
	JsonObject obj = doc.to<JsonObject>();
	{
		StateGuard guard(_state);
		// Publish all state except mqtt* config
		_state.toJson(obj, true, true, true);
	}

	String payload;
	serializeJson(doc, payload);
	String topic = rootTopic + "/state";
	_mqttClient.publish(topic.c_str(), payload.c_str(), true /* retained */);
}

void MqttInterface::_connect() {
	String server, username, password, rootTopic, hostname;
	int port;
	bool haDiscovery;

	{
		StateGuard guard(_state);
		server = _state.mqttServer.get();
		port = _state.mqttPort.get();
		username = _state.mqttUsername.get();
		password = _state.mqttPassword.get();
		rootTopic = _state.mqttRootTopic.get();
		hostname = _state.hostname.get();
		haDiscovery = _state.mqttHADiscovery.get();
	}

	if (server.isEmpty()) {
		Serial.println("[MQTT] No server configured, skipping connect");
		return;
	}

	_mqttClient.setServer(server.c_str(), port);

	String clientId = "cool-bed-" + hostname;
	bool connected;
	if (username.isEmpty()) {
		connected = _mqttClient.connect(clientId.c_str());
	} else {
		connected = _mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str());
	}

	if (connected) {
		Serial.printf("[MQTT] Connected to %s:%d\n", server.c_str(), port);

		// Subscribe to set topics for all non-mqtt config properties
		String subTopic = rootTopic + "/+/set";
		_mqttClient.subscribe(subTopic.c_str());
		Serial.printf("[MQTT] Subscribed to: %s\n", subTopic.c_str());

		// Publish current state on connect
		publishState();

		// Publish HA discovery if enabled
		if (haDiscovery) {
			_publishDiscovery();
		}
	} else {
		Serial.printf("[MQTT] Connection failed, rc=%d\n", _mqttClient.state());
	}
}

// on incoming mqtt data
void MqttInterface::_onMessage(char* topic, uint8_t* payload, unsigned int length) {
	String topicStr = String(topic);
	String rootTopic;
	{
		StateGuard guard(_state);
		rootTopic = _state.mqttRootTopic.get();
	}

	// Extract property name: ${rootTopic}/<name>/set
	String prefix = rootTopic + "/";
	String suffix = "/set";
	if (!topicStr.startsWith(prefix) || !topicStr.endsWith(suffix)) return;

	String propName = topicStr.substring(prefix.length(), topicStr.length() - suffix.length());

	// Filter out mqtt* properties — these must only be set via web interface
	if (propName.startsWith("mqtt") || propName.startsWith("Mqtt")) {
		Serial.printf("[MQTT] Ignoring mqtt* property via MQTT: %s\n", propName.c_str());
		return;
	}

	// String value = String((char*)payload).substring(0, length); // don't do this as payload might not be null terminated
	String value;
	value.reserve(length); // reserve will add space for null terminator
	for (unsigned int i = 0; i < length; i++) {
		value += (char)payload[i];
	}
	Serial.printf("[MQTT] Received %s = %s\n", propName.c_str(), value.c_str());

	// Apply to state
	// If mode changed, do a different path
	if (propName == "mode") {
		_controller.setMode(value);
	} else { // this is for other config properties
		JsonDocument doc; // setting values via MQTT is done one by one, so create a JSON document with just one property and pass to State::applyConfigJson
		doc[propName] = value;
		JsonObjectConst obj = doc.as<JsonObjectConst>();
		bool changed;
		{
			StateGuard guard(_state);
			changed = _state.applyConfigJson(obj);
		}
		if (changed) {
			_controller.notifyConfigChanged();
		}
	}
}

// static callback to pass data to our single instance method
void MqttInterface::_mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
	if (_instance) {
		_instance->_onMessage(topic, payload, length);
	}
}

void MqttInterface::_addDeviceInfo(JsonObject& obj) {
	String hostname;
	bool isHostnameDefault;
	String fwVersion;
	{
		StateGuard guard(_state);
		hostname = _state.hostname.get();
		isHostnameDefault = _state.hostname.isDefault();
		fwVersion = _state.fwVersion.get();
	}
	JsonObject device = obj["device"].to<JsonObject>();
	JsonArray ids = device["identifiers"].to<JsonArray>();
	ids.add(hostname);
	if (isHostnameDefault) {
		device["name"] = AD_NAME;
		Serial.printf("[MQTT] Hostname is default (%s), using generic device name: %s\n", hostname.c_str(), AD_NAME);
	} else {
		device["name"] = AD_NAME_PREFIX + hostname + AD_NAME_SUFFIX;
		Serial.printf("[MQTT] Hostname is custom (%s), using device name: %s\n", hostname.c_str(), (AD_NAME_PREFIX + hostname + AD_NAME_SUFFIX).c_str());
	}
	device["mdl"] = AD_MODEL; // model
	device["mf"] = AD_MANUFACTURER; // manufacturer
	// Home Assistant device-level firmware version field.
	device["sw"] = fwVersion; // sw_version
}

void MqttInterface::_publishDiscovery() {
	String haTopic, rootTopic, hostname;
	{
		StateGuard guard(_state);
		haTopic = _state.mqttHADiscoveryTopic.get();
		rootTopic = _state.mqttRootTopic.get();
		hostname = _state.hostname.get();
	}

	String stateTopic = rootTopic + "/state";
	String deviceTopic = haTopic + "/device/" + hostname + "/config";

	// Build single device discovery document with all entities
	JsonDocument doc;
	JsonObject root = doc.to<JsonObject>();

	// Device info
	_addDeviceInfo(root);

	// Origin
	JsonObject origin = root["origin"].to<JsonObject>();
	origin["name"] = AD_ORIGIN_NAME;

	// note use of abbreviations
	// https://www.home-assistant.io/integrations/mqtt/#supported-abbreviations-in-mqtt-discovery-messages

	// device_class
	// https://www.home-assistant.io/integrations/sensor/#device-class
	// https://www.home-assistant.io/integrations/binary_sensor/#device-class

	// Components (entities)
	JsonObject components = root["components"].to<JsonObject>();

	// --- Telemetry sensors ---
	auto addSensor = [&](const char* id, const char* deviceClass, const auto& metric) {
		JsonObject e = components[id].to<JsonObject>();
		e["p"] = "sensor"; // platform
		e["name"] = metric.getDescription();
		if (deviceClass && strlen(deviceClass) > 0) e["dev_cla"] = deviceClass;
		if (strlen(metric.getUnits()) > 0) e["unit_of_meas"] = metric.getUnits();
		e["stat_t"] = stateTopic; // state_topic
		e["val_tpl"] = String("{{ value_json.") + metric.getName() + " }}"; // value_template
		e["unique_id"] = hostname + "_" + id;
		// if class is volume_flow_rate then set precision to 2 decimal places because the default seems to be 0 and it is too low for our application
		if (strcmp(deviceClass, "volume_flow_rate") == 0) {
			e["sug_dsp_prc"] = 2; // suggested_display_precision
		}
	};

	auto addBinarySensor = [&](const char* id, const char* deviceClass, const auto& metric) {
		JsonObject e = components[id].to<JsonObject>();
		e["p"] = "binary_sensor"; // platform
		e["name"] = metric.getDescription();
		if (deviceClass && strlen(deviceClass) > 0) e["dev_cla"] = deviceClass;
		e["stat_t"] = stateTopic; // state_topic
		// e["value_template"] = String("{{ value_json.") + valueKey + " }}";
		e["val_tpl"] = String("{{ 'ON' if value_json.") + metric.getName() + " else 'OFF' }}"; // AI suggested that Home Assistant expects "ON"/"OFF" strings, so need to convert boolean to string here
		// e["payload_on"] = "ON"; // should be default this way
		// e["payload_off"] = "OFF"; // should be default this way
		e["unique_id"] = hostname + "_" + id;
	};

	addSensor("status", "", _state.status);
	addBinarySensor("error", "problem", _state.error);
	addSensor("fw_version", "", _state.fwVersion);
	addSensor("wifi_rssi", "signal_strength", _state.rssi);
	addSensor("build_date_time", "", _state.buildDateTime);
	addSensor("build_timestamp", "", _state.buildTimestamp);
	addSensor("circ_speed", "", _state.circSpeed);

#ifdef COOLING_PWM_PIN
	addSensor("cooling_speed", "", _state.coolingSpeed);
#endif
#ifdef HEATING_PWM_PIN
	addSensor("heating_speed", "", _state.heatingSpeed);
#endif

#ifdef FLOW_SENSOR_PIN
	addSensor("flow", "volume_flow_rate", _state.flow);
	addSensor("flow_pulses_filtered_per_sec", "", _state.flowPulsesFilteredPerSec);
	addSensor("flow_pulses_raw_per_sec", "", _state.flowPulsesRawPerSec);
#endif

	addSensor("out_temperature", "temperature", _state.outTemperature);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	addSensor("return_temperature", "temperature", _state.returnTemperature);
	addSensor("temperature_delta", "temperature_delta", _state.temperatureDelta);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	addSensor("cooling_temperature", "temperature", _state.coolingTemperature);
#endif

#if defined(DALLAS_SENSOR_RETURNING_PIN) && defined(FLOW_SENSOR_PIN)
	addSensor("cooling_power", "power", _state.coolingPower);
#endif

#ifdef INA226_CIRCULATION_ADDRESS
	addSensor("circ_voltage", "voltage", _state.circVoltage);
	addSensor("circ_current", "current", _state.circCurrent);
#endif
#ifdef INA226_COOLING_ADDRESS
	addSensor("cooling_voltage", "voltage", _state.coolingVoltage);
	addSensor("cooling_current", "current", _state.coolingCurrent);
#endif
#ifdef INA226_HEATING_ADDRESS
	addSensor("heating_voltage", "voltage", _state.heatingVoltage);
	addSensor("heating_current", "current", _state.heatingCurrent);
#endif

	addBinarySensor("flow_sensor_present", "connectivity", _state.flowSensorPresent);
	addBinarySensor("cooling_present", "power", _state.coolingPresent);
	addBinarySensor("heating_present", "power", _state.heatingPresent);
	addBinarySensor("cooling_temperature_present", "connectivity", _state.coolingTemperaturePresent);
	addBinarySensor("return_temperature_present", "connectivity", _state.returnTemperaturePresent);
	addBinarySensor("circ_iv_present", "connectivity", _state.circIVPresent);
	addBinarySensor("cooling_iv_present", "connectivity", _state.coolingIVPresent);
	addBinarySensor("heating_iv_present", "connectivity", _state.heatingIVPresent);

	// --- Settable config (number entities) ---
	// https://www.home-assistant.io/integrations/number.mqtt/
	auto addNumber = [&](const char* id, const char* deviceClass, const auto& var, float step) {
		JsonObject e = components[id].to<JsonObject>();
		e["p"] = "number"; // platform
		e["name"] = var.getDescription();
		if (deviceClass && strlen(deviceClass) > 0) e["dev_cla"] = deviceClass;
		if (strlen(var.getUnits()) > 0) e["unit_of_meas"] = var.getUnits();
		e["stat_t"] = stateTopic; // state_topic
		e["val_tpl"] = String("{{ value_json.") + var.getName() + " }}"; // value_template
		e["cmd_t"] = rootTopic + "/" + var.getName() + "/set"; // command_topic
		e["min"] = var.getMin();
		e["max"] = var.getMax();
		e["step"] = step;
		e["unique_id"] = hostname + "_" + id;
	};

	addNumber("circ_speed_set_point", "", _state.circSpeedSetPoint, 1);

#ifdef COOLING_PWM_PIN
	addNumber("cooling_speed_set_point", "", _state.coolingSpeedSetPoint, 1);
#endif
#ifdef HEATING_PWM_PIN
	addNumber("heating_speed_set_point", "", _state.heatingSpeedSetPoint, 1);
#endif

	addNumber("temperature_set_point", "temperature", _state.temperatureSetPoint, 0.5f);
	addNumber("out_temp_calibration_offset", "temperature_delta", _state.outTemperatureCalibrationOffset, 0.1f);

#ifdef DALLAS_SENSOR_RETURNING_PIN
	addNumber("return_temp_calibration_offset", "temperature_delta", _state.returnTemperatureCalibrationOffset, 0.1f);
#endif
#ifdef DALLAS_SENSOR_COOLING_PIN
	addNumber("cooling_temp_calibration_offset", "temperature_delta", _state.coolingTemperatureCalibrationOffset, 0.1f);
#endif

#ifdef FLOW_SENSOR_PIN
	addNumber("calibration_volume", "volume", _state.calibrationVolume, 1);
	addNumber("calibration_flow_pulses", "", _state.calibrationFlowPulses, 1);
	addNumber("min_flow_pulses_per_sec", "", _state.minFlowPulsesPerSec, 1);
#endif

	addNumber("system_time", "", _state.systemTime, 1);

#ifdef INA226_CIRCULATION_ADDRESS
	addNumber("max_circ_current", "current", _state.maxCircCurrent, 1);
	addNumber("min_circ_current", "current", _state.minCircCurrent, 1);
	addNumber("min_circ_voltage", "voltage", _state.minCircVoltage, 1);
#endif
#ifdef INA226_COOLING_ADDRESS
	addNumber("max_cooling_current", "current", _state.maxCoolingCurrent, 1);
	addNumber("min_cooling_current", "current", _state.minCoolingCurrent, 1);
	addNumber("min_cooling_voltage", "voltage", _state.minCoolingVoltage, 1);
#endif
#ifdef INA226_HEATING_ADDRESS
	addNumber("max_heating_current", "current", _state.maxHeatingCurrent, 1);
	addNumber("min_heating_current", "current", _state.minHeatingCurrent, 1);
	addNumber("min_heating_voltage", "voltage", _state.minHeatingVoltage, 1);
#endif

	// --- Mode select ---
	{
		JsonObject e = components["mode"].to<JsonObject>();
		e["p"] = "select"; // platform
		e["name"] = "Mode";
		e["stat_t"] = stateTopic; // state_topic
		e["val_tpl"] = "{{ value_json.mode }}"; // value_template
		e["cmd_t"] = rootTopic + "/mode/set"; // command_topic
		JsonArray options = e["options"].to<JsonArray>();
		if (_state.isModeSupported(MODE_STOP)) options.add(MODE_STOP);
		if (_state.isModeSupported(MODE_TEMPERATURE)) options.add(MODE_TEMPERATURE);
		if (_state.isModeSupported(MODE_MANUAL_CIRC)) options.add(MODE_MANUAL_CIRC);
		if (_state.isModeSupported(MODE_MANUAL_COOL)) options.add(MODE_MANUAL_COOL);
		if (_state.isModeSupported(MODE_MANUAL_HEAT)) options.add(MODE_MANUAL_HEAT);
		if (_state.isModeSupported(MODE_FLOW_CALIBRATION)) options.add(MODE_FLOW_CALIBRATION);
		if (_state.isModeSupported(MODE_FLOW_TEST)) options.add(MODE_FLOW_TEST);
		e["unique_id"] = hostname + "_mode";
	}

	String errorTopic = rootTopic + "/error";
	String payload;
	serializeJson(doc, payload);
	if (payload.length() > MQTT_BUFFER_SIZE) {
		Serial.printf("[MQTT] Error - HA discovery payload size %d exceeds buffer size %d, cannot publish\n", payload.length(), MQTT_BUFFER_SIZE);
		// report to mqtt the payload length and max buffer size
		String errorPayload = String("HA discovery payload size ") + payload.length() + " exceeds buffer size " + MQTT_BUFFER_SIZE + ", cannot publish";
		_mqttClient.publish(errorTopic.c_str(), errorPayload.c_str());
		return;
	}
	// Serial.printf("[MQTT] Publishing HA discovery to: %s\nPayload size: %d\nPayload:\n%s\n", deviceTopic.c_str(), payload.length(), payload.c_str());
	Serial.printf("[MQTT] Publishing HA discovery to: %s, Payload size: %d\n", deviceTopic.c_str(), payload.length());
	_mqttClient.publish(deviceTopic.c_str(), payload.c_str(), true /* retained */);
	Serial.printf("[MQTT] Published HA discovery to: %s\n", deviceTopic.c_str());
}
