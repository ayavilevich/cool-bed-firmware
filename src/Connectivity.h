// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include "State.h"

class Connectivity {
public:
	Connectivity(State& state);

	// Call once during setup — blocks until Wi-Fi is configured and connected
	void begin();

	// Call from main loop — handles reconnection
	void loop();

	bool isConnected() const;

	// Request reset of Wi-Fi credentials (called from button handler)
	void resetWifi();

private:
	State& _state;
	WiFiManager _wifiManager;
	bool _connected;
	bool _otaStarted;
	unsigned long _lastLoopLogMs;
	unsigned long _lastReconnectionAttemptMs;

	void _setupMdns();
	void _setupOta();
	void _updateRssiMetric();
	static void _wifiConnectedCallback(WiFiManager* wm);
};
