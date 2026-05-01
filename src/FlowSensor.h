#pragma once
#include <Arduino.h>
#include <driver/gpio.h>

// FlowSensor implements hardware-debounced flow pulse counting.
//
// Two counters are maintained:
//   - raw: counts RISING edge interrupts directly on the pin (fast, no debounce)
//   - filtered: uses a hardware timer at 50µs intervals to sample the pin level
//     and maintain a 16-bit history. A stable transition (all 0s or all 1s) is
//     required before the edge is counted.
//
// Both counters are uint64_t and increase monotonically (never reset).
// The caller subtracts snapshots to get per-interval counts.

#define FLOW_TIMER_INTERVAL_US	50		// timer fires every 50 microseconds
#define FLOW_HISTORY_BITS		16		// bits in the pin history window

class FlowSensor {
public:
	FlowSensor(uint8_t pin);

	// Call once during setup
	void begin();

	uint64_t getRawPulses() const;
	uint64_t getFilteredPulses() const;

private:
	uint8_t _pin;
	gpio_num_t _gpioNum;

	// --- raw (interrupt-based) ---
	volatile uint64_t _rawCount;

	// --- filtered (timer-based) ---
	volatile uint64_t _filteredCount;
	volatile uint16_t _history;		// 16-bit sliding window of pin samples
	volatile bool     _currentState;	// debounced current state of the pin

	// Statics needed because ISR callbacks can't be member functions
	static FlowSensor* _instance;
	static void IRAM_ATTR _rawIsr();
	static bool IRAM_ATTR _timerIsr(void* arg);
};
