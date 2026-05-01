#include <Arduino.h>
#include "State.h"
#include "Connectivity.h"
#include "Controller.h"
#include "WebInterface.h"
#include "MqttInterface.h"

// Global instances (accessible by Connectivity.cpp for button-triggered reset)
Connectivity* g_connectivity = nullptr;

State         g_state;
Controller    g_controller(g_state);
Connectivity  g_connectivityObj(g_state);
WebInterface  g_webInterface(g_state, g_controller);
MqttInterface g_mqttInterface(g_state, g_controller);

void setup() {
	Serial.begin(115200);
	Serial.println("\n[Main] Cool Bed firmware starting...");

	// Initialise state (loads persisted config)
	g_state.begin();

	// Set global pointer used by requestWifiReset()
	g_connectivity = &g_connectivityObj;

	// Wire up MQTT callbacks to controller events
	g_controller.onTelemetryUpdated([&]() {
		g_mqttInterface.publishState();
	});
	g_controller.onConfigUpdated([&]() {
		g_mqttInterface.publishState();
	});

	// Connect to Wi-Fi (blocks until connected or portal opened)
	g_connectivityObj.begin();

	// Start hardware controller
	g_controller.begin();

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
