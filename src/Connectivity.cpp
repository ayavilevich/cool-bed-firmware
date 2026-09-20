// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "Connectivity.h"
#ifdef OTA_ENABLE
	#include <SPIFFS.h>
	#include <ArduinoOTA.h>
#endif

#define LOOP_LOG_INTERVAL_MS			 4000
#define RECONNECTION_ATTEMPT_INTERVAL_MS 20000

Connectivity::Connectivity(State& state) : _state(state), _connected(false), _otaStarted(false), _lastLoopLogMs(0), _lastReconnectionAttemptMs(0) {}

void Connectivity::begin() {
	String apSsid, hostname;
	{
		StateGuard guard(_state);
		apSsid = _state.hostname.get();
		hostname = _state.hostname.get();
	}

	_wifiManager.setConfigPortalBlocking(true);
	_wifiManager.setHostname(hostname.c_str());

	// Connect or open config portal
	bool ok = _wifiManager.autoConnect(apSsid.c_str());
	if (ok) {
		_connected = true;
		Serial.printf("[Connectivity] Connected to Wi-Fi. IP: %s\n", WiFi.localIP().toString().c_str());
		_updateRssiMetric();
		_setupMdns();
		_setupOta();
	} else {
		Serial.println("[Connectivity] Failed to connect to Wi-Fi");
		_connected = false;
	}
}

void Connectivity::loop() {
	// log state for troubleshooting
	unsigned long now = millis();
	// if (now - _lastLoopLogMs >= LOOP_LOG_INTERVAL_MS) {
	// 	_lastLoopLogMs = now;
	// 	Serial.printf("[Connectivity] Looping, Wi-Fi status: %s, connected flag: %s\n",
	// 				  WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
	// 				  _connected ? "true" : "false");
	// }
	// update state when changes happen
	if (WiFi.status() == WL_CONNECTED) {
		_updateRssiMetric();
		if (!_connected) { // Wi-Fi has just (re)connected
			_connected = true;
			Serial.println("[Connectivity] Wi-Fi reconnected");
			_updateRssiMetric();
			_setupMdns();
			_setupOta();
		}
		if (_otaStarted) {
#ifdef OTA_ENABLE_PIN
			// Check if OTA is enabled via pin
			if (digitalRead(OTA_ENABLE_PIN) == LOW) {
				ArduinoOTA.handle();
			}
#else // no pin and OTA active, then handle all the time
			ArduinoOTA.handle();
#endif
		}
	} else {
		if (_connected) {
			_connected = false;
			_otaStarted = false;
			{
				StateGuard guard(_state);
				_state.rssi.set(_state.rssi.getDefault());
			}
			Serial.println("[Connectivity] Wi-Fi disconnected.");
		}
		// start reconnection process unless we are still waiting for the previous reconnection attempt
		if (now - _lastReconnectionAttemptMs >= RECONNECTION_ATTEMPT_INTERVAL_MS) {
			_lastReconnectionAttemptMs = now;
			WiFi.reconnect(); // can't just hammer "reconnect" as we will never get connected this way
			Serial.println("[Connectivity] Attempting Wi-Fi reconnect...");
		}
	}
}

void Connectivity::_updateRssiMetric() {
	StateGuard guard(_state);
	_state.rssi.set((int)WiFi.RSSI());
}

bool Connectivity::isConnected() const {
	return _connected && (WiFi.status() == WL_CONNECTED);
}

void Connectivity::resetWifi() {
	Serial.println("[Connectivity] Resetting WiFi credentials...");
	_wifiManager.resetSettings();
	// Restart to enter config portal
	ESP.restart();
}

void Connectivity::_setupMdns() {
	String hostname;
	{
		StateGuard guard(_state);
		hostname = _state.hostname.get();
	}
	if (MDNS.begin(hostname.c_str())) {
		MDNS.addService("http", "tcp", 80);
		Serial.printf("[Connectivity] mDNS started: http://%s.local\n", hostname.c_str());
	} else {
		Serial.println("[Connectivity] mDNS failed to start");
	}
}

void Connectivity::_setupOta() {
	if (_otaStarted) {
		return;
	}

#ifdef OTA_ENABLE

	#ifdef OTA_ENABLE_PIN
	pinMode(OTA_ENABLE_PIN, INPUT_PULLUP);
	#endif

	String hostname;
	{
		StateGuard guard(_state);
		hostname = _state.hostname.get();
	}

	ArduinoOTA.setHostname(hostname.c_str());
	ArduinoOTA.setPort(OTA_PORT);

	#ifdef OTA_PASSWORD
	ArduinoOTA.setPassword(OTA_PASSWORD);
	Serial.println("[Connectivity] OTA password is enabled");
	#else
	Serial.println("[Connectivity] OTA password is not configured");
	#endif

	ArduinoOTA.onStart([]() {
		const char* type = "sketch";
		if (ArduinoOTA.getCommand() == U_SPIFFS) {
			type = "filesystem";
			// CRITICAL: Unmount SPIFFS here so the OTA tool can safely overwrite it
			SPIFFS.end();
		}
		Serial.printf("[Connectivity] OTA update started: type = \"%s\"\n", type);
	});
	ArduinoOTA.onEnd([]() { Serial.println("[Connectivity] OTA update finished"); });
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		unsigned int pct = (total == 0) ? 0 : (progress * 100U) / total;
		Serial.printf("[Connectivity] OTA progress: %u%%\n", pct);
	});
	ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[Connectivity] OTA error [%u]\n", (unsigned int)error); });

	ArduinoOTA.begin();
	_otaStarted = true;
	Serial.printf("[Connectivity] OTA ready: %s.local:%d\n", hostname.c_str(), OTA_PORT);
#endif // OTA_ENABLE
}
