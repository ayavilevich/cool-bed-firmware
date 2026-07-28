// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include "State.h"
#include "Connectivity.h"
#include "Controller.h"
#include "WebInterface.h"
#include "MqttInterface.h"

#define SERIAL_BAUD_RATE 115200

State         g_state;
Connectivity  g_connectivityObj(g_state);
Controller    g_controller(g_state, g_connectivityObj);
WebInterface  g_webInterface(g_state, g_controller);
MqttInterface g_mqttInterface(g_state, g_controller);

void setup() {
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.println("\n[Main] Cool Bed firmware starting...");

	// Initialise state (loads persisted config)
	g_state.begin();

	// Wire up MQTT callbacks to controller events
	g_controller.onTelemetryUpdated([&]() {
		g_mqttInterface.publishState();
	});
	g_controller.onConfigUpdated([&]() {
		g_mqttInterface.publishState();
	});

	// Start hardware controller
	g_controller.begin();

	// Connect to Wi-Fi (blocks until connected or portal opened)
	g_connectivityObj.begin();

	// Start web server
	g_webInterface.begin();

	// Start MQTT (only if enabled in config)
	g_mqttInterface.begin();

	Serial.println("[Main] Setup complete");
}

void loop() {
	g_connectivityObj.loop();
	g_controller.loop();
	g_controller.checkButton();
	g_controller.updateLeds(g_connectivityObj.isConnected());
	g_mqttInterface.loop();
}
