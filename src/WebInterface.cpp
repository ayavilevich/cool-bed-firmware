// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "WebInterface.h"

WebInterface::WebInterface(State& state, Controller& controller)
	: _state(state), _controller(controller), _server(80) {
}

void WebInterface::begin() {
	if (!SPIFFS.begin(true)) {
		Serial.println("[WebInterface] SPIFFS mount failed");
		return;
	}

	_setupRoutes();
	_server.begin();
	Serial.println("[WebInterface] HTTP server started on port 80");
}

void WebInterface::_setupRoutes() {
	// Serve static files from SPIFFS
	_server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

	// GET /api/state
	_server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
		_handleGetState(request);
	});

	// GET /api/model
	_server.on("/api/model", HTTP_GET, [this](AsyncWebServerRequest* request) {
		_handleGetModel(request);
	});

	// POST /api/config
	_server.on("/api/config", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		nullptr,
		[this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
			_handlePostConfig(request, data, len, index, total);
		}
	);

	// POST /api/mode
	_server.on("/api/mode", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		nullptr,
		[this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
			_handlePostMode(request, data, len, index, total);
		}
	);

	// 404 handler
	_server.onNotFound([](AsyncWebServerRequest* request) {
		request->send(404, "text/plain", "Not found");
	});
}

void WebInterface::_handleGetState(AsyncWebServerRequest* request) {
	JsonDocument doc;
	JsonObject obj = doc.to<JsonObject>();
	{
		StateGuard guard(_state);
		_state.toJson(obj, true, true, false);
		// Remove password from response for security
		// obj.remove("mqttPassword"); // Already excluded by toJson
	}

	String response;
	serializeJson(doc, response);
	request->send(200, "application/json", response);
}

void WebInterface::_handleGetModel(AsyncWebServerRequest* request) {
	JsonDocument doc;
	JsonObject obj = doc.to<JsonObject>();
	{
		StateGuard guard(_state);
		_state.toModelJson(obj, true, true, false);
	}

	String response;
	serializeJson(doc, response);
	request->send(200, "application/json", response);
}

void WebInterface::_handlePostConfig(AsyncWebServerRequest* request, uint8_t* data,
                                     size_t len, size_t index, size_t total) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, data, len);
	if (err) {
		request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
		return;
	}

	JsonObjectConst obj = doc.as<JsonObjectConst>();
	bool changed;
	{
		StateGuard guard(_state);
		changed = _state.applyConfigJson(obj);
	}

	if (changed) {
		_controller.notifyConfigChanged();
		// If mode changed, apply it through controller
		if (!obj["mode"].isNull()) { // normally this should not happen because mode should be changed through /api/mode endpoint, but just in case if mode is included in config JSON, apply it
			_controller.setMode(obj["mode"].as<String>());
		}
		request->send(200, "application/json", "{\"ok\":true}");
	} else {
		request->send(200, "application/json", "{\"ok\":false,\"message\":\"Nothing changed\"}");
	}
}

void WebInterface::_handlePostMode(AsyncWebServerRequest* request, uint8_t* data,
                                   size_t len, size_t index, size_t total) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, data, len);
	if (err || !doc["mode"].is<String>()) {
		request->send(400, "application/json", "{\"error\":\"Missing mode field\"}");
		return;
	}

	String newMode = doc["mode"].as<String>();
	// Validate mode value
	if (newMode != MODE_STOP && newMode != MODE_SPEED && newMode != MODE_TEMPERATURE && newMode != MODE_CALIBRATION && newMode != MODE_FLOW_TEST) {
		request->send(400, "application/json", "{\"error\":\"Invalid mode\"}");
		return;
	}

	_controller.setMode(newMode);
	request->send(200, "application/json", "{\"ok\":true}");
}
