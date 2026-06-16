// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "FlowSensor.h"
#include <esp_timer.h>

FlowSensor* FlowSensor::_instance = nullptr;

FlowSensor::FlowSensor(uint8_t pin)
	: _pin(pin), _gpioNum((gpio_num_t)pin),
	  _rawCount(0), _filteredCount(0),
	  _history(0), _currentState(false) {
}

void FlowSensor::begin() {
	_instance = this;

	// Configure pin as input
	pinMode(_pin, INPUT); // Assuming the flow sensor has the right pull-up/down resistor inside it.

	// Init history and current state based on current pin level
	bool initialLevel = (gpio_get_level(_gpioNum) != 0);
	_currentState = initialLevel;
	_history = initialLevel ? 0xFFFF : 0x0000;

	// Attach raw interrupt on RISING edge
	attachInterrupt(digitalPinToInterrupt(_pin), _rawIsr, RISING);

	// Setup ESP timer for filtered sampling at FLOW_TIMER_INTERVAL_US
	esp_timer_handle_t timerHandle;
	esp_timer_create_args_t timerArgs = {
		.callback = _timerIsr,
		.arg = nullptr,
		.dispatch_method = ESP_TIMER_TASK, // ESP_TIMER_ISR needs CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
		.name = "flow_filter_timer",
		.skip_unhandled_events = true,
	};
	esp_timer_create(&timerArgs, &timerHandle);
	esp_timer_start_periodic(timerHandle, FLOW_TIMER_INTERVAL_US);
}

uint64_t FlowSensor::getRawPulses() const {
	return _rawCount;
}

uint64_t FlowSensor::getFilteredPulses() const {
	return _filteredCount;
}

void IRAM_ATTR FlowSensor::_rawIsr() {
	if (_instance) {
		_instance->_rawCount++;
	}
}

void IRAM_ATTR FlowSensor::_timerIsr(void* arg) {
	FlowSensor* self = _instance;
	if (!self) return;

	// Read current pin level
	int level = gpio_get_level(self->_gpioNum);

	// Shift history left and push new bit into LSB (FIFO: newest at LSB)
	self->_history = (self->_history << 1) | (level ? 1 : 0);

	// Determine if history is fully stable (all same value)
	if (self->_history == 0xFFFF) {
		// All 16 samples are HIGH
		if (!self->_currentState) {
			self->_currentState = true;
			self->_filteredCount++; // rising edge confirmed
		}
	} else if (self->_history == 0x0000) {
		// All 16 samples are LOW
		if (self->_currentState) {
			self->_currentState = false;
			// falling edge — don't count, we count rising edges only
		}
	}
}
