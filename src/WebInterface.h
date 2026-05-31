#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "State.h"
#include "Controller.h"

class WebInterface {
public:
	WebInterface(State& state, Controller& controller);

	void begin();

private:
	State& _state;
	Controller& _controller;
	AsyncWebServer _server;

	void _setupRoutes();

	// GET /api/state — returns full state JSON
	void _handleGetState(AsyncWebServerRequest* request);

	// GET /api/model — returns metric/config metadata model JSON
	void _handleGetModel(AsyncWebServerRequest* request);

	// POST /api/config — accepts JSON body, applies config
	void _handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len,
	                       size_t index, size_t total);

	// POST /api/mode — set operating mode
	void _handlePostMode(AsyncWebServerRequest* request, uint8_t* data, size_t len,
	                     size_t index, size_t total);
};
