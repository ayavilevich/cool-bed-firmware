#include "Connectivity.h"

Connectivity::Connectivity(State& state)
	: _state(state), _connected(false) {
}

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
		Serial.printf("[Connectivity] Connected to Wi-Fi. IP: %s\n",
		              WiFi.localIP().toString().c_str());
		_setupMdns();
	} else {
		Serial.println("[Connectivity] Failed to connect to Wi-Fi");
		_connected = false;
	}
}

void Connectivity::loop() {
	if (WiFi.status() == WL_CONNECTED) {
		if (!_connected) {
			_connected = true;
			Serial.println("[Connectivity] Wi-Fi reconnected");
			_setupMdns();
		}
	} else {
		if (_connected) {
			_connected = false;
			Serial.println("[Connectivity] Wi-Fi disconnected, attempting reconnect...");
		}
		// WiFiManager / Arduino WiFi will handle reconnection automatically
		// but we can explicitly trigger it
		WiFi.reconnect();
	}
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
