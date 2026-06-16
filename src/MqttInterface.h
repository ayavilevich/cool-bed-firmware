// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "State.h"
#include "Controller.h"

// Note that the implementation allows for just one instance of this class due to use of static callback members.

class MqttInterface {
public:
	MqttInterface(State& state, Controller& controller);

	void begin();
	void loop();

	// Called by controller when telemetry or config changes
	void publishState();

private:
	State& _state;
	Controller& _controller;
	WiFiClient _wifiClient;
	PubSubClient _mqttClient;

	bool _enabled;
	unsigned long _lastReconnectMs;

	static MqttInterface* _instance;

	void _connect();
	void _publishDiscovery();
	void _onMessage(char* topic, uint8_t* payload, unsigned int length);
	static void _mqttCallback(char* topic, uint8_t* payload, unsigned int length);

	// Build HA discovery payload for a sensor entity
	String _buildSensorDiscovery(const char* name, const char* deviceClass,
	                              const char* unit, const char* stateTopic,
	                              const char* valueTemplate);
	// Build HA discovery payload for a number entity
	String _buildNumberDiscovery(const char* name, const char* commandTopic,
	                              const char* stateTopic, const char* valueTemplate,
	                              float min, float max, float step);
	// Build HA discovery payload for a select entity
	String _buildSelectDiscovery(const char* name, const char* commandTopic,
	                              const char* stateTopic, const char* valueTemplate,
	                              const char* options);
	// Build common device JSON snippet
	void _addDeviceInfo(JsonObject& obj);
};
