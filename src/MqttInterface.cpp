#include "MqttInterface.h"

#define MQTT_RECONNECT_INTERVAL_MS 5000
#define MQTT_BUFFER_SIZE 7168 // our auto-discovery payload is large, so need to increase the default 256B buffer
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
	JsonDocument doc; // setting values via MQTT is done one by one, so create a JSON document with just one property and pass to State::applyConfigJson
	doc[propName] = value;
	JsonObjectConst obj = doc.as<JsonObjectConst>();
	bool changed;
	{
		StateGuard guard(_state);
		changed = _state.applyConfigJson(obj);
	}

	if (changed) {
		// If mode changed, notify controller
		if (propName == "mode") {
			_controller.setMode(value);
		} else {
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
	{
		StateGuard guard(_state);
		hostname = _state.hostname.get();
		isHostnameDefault = _state.hostname.isDefault();
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
	device["model"] = AD_MODEL;
	device["manufacturer"] = AD_MANUFACTURER;
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

	// Components (entities)
	JsonObject components = root["components"].to<JsonObject>();

	// --- Telemetry sensors ---
	auto addSensor = [&](const char* id, const char* name, const char* deviceClass,
	                      const char* unit, const char* valueKey) {
		JsonObject e = components[id].to<JsonObject>();
		e["platform"] = "sensor";
		e["name"] = name;
		if (deviceClass && strlen(deviceClass) > 0) e["device_class"] = deviceClass;
		if (unit && strlen(unit) > 0) e["unit_of_measurement"] = unit;
		e["state_topic"] = stateTopic;
		e["value_template"] = String("{{ value_json.") + valueKey + " }}";
		e["unique_id"] = hostname + "_" + id;
	};

	auto addBinarySensor = [&](const char* id, const char* name, const char* deviceClass,
	                            const char* valueKey) {
		JsonObject e = components[id].to<JsonObject>();
		e["platform"] = "binary_sensor";
		e["name"] = name;
		if (deviceClass && strlen(deviceClass) > 0) e["device_class"] = deviceClass;
		e["state_topic"] = stateTopic;
		// e["value_template"] = String("{{ value_json.") + valueKey + " }}";
		e["value_template"] = String("{{ 'ON' if value_json.") + valueKey + " else 'OFF' }}"; // AI suggested that Home Assistant expects "ON"/"OFF" strings, so need to convert boolean to string here
		// e["payload_on"] = "ON"; // should be default this way
		// e["payload_off"] = "OFF"; // should be default this way
		e["unique_id"] = hostname + "_" + id;
	};

	addSensor("status", "Status", "", "", "status");
	addBinarySensor("error", "Error", "problem", "error");
	addSensor("pump_speed", "Pump Speed", "", "counts", "pumpSpeed");
	addSensor("flow", "Flow", "volume_flow_rate", "L/min", "flow");
	addSensor("temperature_delta", "Temperature Delta", "temperature", "°C", "temperatureDelta");
	addSensor("cooling_power", "Cooling Power", "power", "W", "coolingPower");
	addSensor("flow_pulses_filtered_per_sec", "Flow Pulses/s (filtered)", "", "p/s", "flowPulsesFilteredPerSec");
	addSensor("flow_pulses_raw_per_sec", "Flow Pulses/s (raw)", "", "p/s", "flowPulsesRawPerSec");
	addSensor("out_temperature", "Outgoing Temperature", "temperature", "°C", "outTemperature");
	addSensor("return_temperature", "Return Temperature", "temperature", "°C", "returnTemperature");
	addSensor("pump_voltage", "Pump Voltage", "voltage", "mV", "pumpVoltage");
	addSensor("pump_current", "Pump Current", "current", "mA", "pumpCurrent");

	// --- Settable config (number entities) ---
	auto addNumber = [&](const char* id, const char* name, const char* unit,
	                      const char* valueKey, float minVal, float maxVal, float step) {
		JsonObject e = components[id].to<JsonObject>();
		e["platform"] = "number";
		e["name"] = name;
		if (unit && strlen(unit) > 0) e["unit_of_measurement"] = unit;
		e["state_topic"] = stateTopic;
		e["value_template"] = String("{{ value_json.") + valueKey + " }}";
		e["command_topic"] = rootTopic + "/" + valueKey + "/set";
		e["min"] = minVal;
		e["max"] = maxVal;
		e["step"] = step;
		e["unique_id"] = hostname + "_" + id;
	};

	addNumber("speed_set_point", "Speed Set Point", "counts", "speedSetPoint", 1, 255, 1);
	addNumber("temperature_set_point", "Temperature Set Point", "°C", "temperatureSetPoint", 10, 40, 0.5);
	addNumber("out_temp_calibration_offset", "Outgoing Temperature Offset", "°C", "outTemperatureCalibrationOffset", -10, 10, 0.1);
	addNumber("return_temp_calibration_offset", "Return Temperature Offset", "°C", "returnTemperatureCalibrationOffset", -10, 10, 0.1);
	addNumber("calibration_volume", "Calibration Volume", "ml", "calibrationVolume", 1, 5000, 1);
	addNumber("calibration_flow", "Calibration Flow", "L/min", "calibrationFlow", 0, 100, 0.01);
	addNumber("calibration_flow_pulses", "Calibration Flow Pulses", "", "calibrationFlowPulses", 1, 10000, 1);
	addNumber("system_time", "System Response Time", "s", "systemTime", 0, 600, 1);
	addNumber("min_flow_pulses_per_sec", "Min Flow Pulses/s", "p/s", "minFlowPulsesPerSec", 1, 1000, 1);
	addNumber("max_current", "Max Current", "mA", "maxCurrent", 1, 1200, 1);
	addNumber("min_voltage", "Min Voltage", "mV", "minVoltage", 0, 40000, 1);

	// --- Mode select ---
	{
		JsonObject e = components["mode"].to<JsonObject>();
		e["platform"] = "select";
		e["name"] = "Mode";
		e["state_topic"] = stateTopic;
		e["value_template"] = "{{ value_json.mode }}";
		e["command_topic"] = rootTopic + "/mode/set";
		JsonArray options = e["options"].to<JsonArray>();
		options.add(MODE_STOP);
		options.add(MODE_SPEED);
		options.add(MODE_TEMPERATURE);
		options.add(MODE_CALIBRATION);
		e["unique_id"] = hostname + "_mode";
	}

	String payload;
	serializeJson(doc, payload);
	if (payload.length() > MQTT_BUFFER_SIZE) {
		Serial.printf("[MQTT] Error - HA discovery payload size %d exceeds buffer size %d, cannot publish\n", payload.length(), MQTT_BUFFER_SIZE);
		return;
	}
	// Serial.printf("[MQTT] Publishing HA discovery to: %s\nPayload size: %d\nPayload:\n%s\n", deviceTopic.c_str(), payload.length(), payload.c_str());
	Serial.printf("[MQTT] Publishing HA discovery to: %s\nPayload size: %d\n", deviceTopic.c_str(), payload.length());
	_mqttClient.publish(deviceTopic.c_str(), payload.c_str(), true /* retained */);
	Serial.printf("[MQTT] Published HA discovery to: %s\n", deviceTopic.c_str());
}
