#include "Connectivity.h"
#ifdef OTA_ENABLE
#include <SPIFFS.h>
#include <ArduinoOTA.h>
#endif

Connectivity::Connectivity(State& state)
	: _state(state), _connected(false), _otaStarted(false) {
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
		_setupOta();
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
	ArduinoOTA.onEnd([]() {
		Serial.println("[Connectivity] OTA update finished");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		unsigned int pct = (total == 0) ? 0 : (progress * 100U) / total;
		Serial.printf("[Connectivity] OTA progress: %u%%\n", pct);
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("[Connectivity] OTA error [%u]\n", (unsigned int)error);
	});

	ArduinoOTA.begin();
	_otaStarted = true;
	Serial.printf("[Connectivity] OTA ready: %s.local:%d\n", hostname.c_str(), OTA_PORT);
#endif // OTA_ENABLE
}
