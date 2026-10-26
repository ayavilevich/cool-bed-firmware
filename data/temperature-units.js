(function (global) {
	const CELSIUS = 'C';
	const FAHRENHEIT = 'F';
	const TEMP_UNIT_STORAGE_KEY = 'coolBed.temperatureUnit';
	const TEMP_RANGE_START_C_STORAGE_KEY = 'coolBed.temperatureRangeStartC';
	const TEMP_RANGE_END_C_STORAGE_KEY = 'coolBed.temperatureRangeEndC';
	const DECIMAL_PLACES = 1;
	const DECIMAL_PLACES_DELTA = 2;

	function normalizeUnit(unit) {
		return unit === FAHRENHEIT ? FAHRENHEIT : CELSIUS;
	}

	function roundToDecimalPlaces(value, decimalPlaces) {
		const factor = Math.pow(10, decimalPlaces);
		return Math.round(value * factor) / factor;
	}

	function celsiusToFahrenheit(value) {
		return roundToDecimalPlaces((value * 9) / 5 + 32, DECIMAL_PLACES);
	}

	function fahrenheitToCelsius(value) {
		return roundToDecimalPlaces(((value - 32) * 5) / 9, DECIMAL_PLACES);
	}

	function celsiusDeltaToFahrenheit(value) {
		return roundToDecimalPlaces((value * 9) / 5, DECIMAL_PLACES_DELTA);
	}

	function fahrenheitDeltaToCelsius(value) {
		return roundToDecimalPlaces((value * 5) / 9, DECIMAL_PLACES_DELTA);
	}

	function fromCelsius(value, options = {}) {
		const n = Number(value);
		if (!Number.isFinite(n)) return value;
		const unit = normalizeUnit(options.unit);
		if (unit === CELSIUS) return n;
		return options.delta ? celsiusDeltaToFahrenheit(n) : celsiusToFahrenheit(n);
	}

	function toCelsius(value, options = {}) {
		const n = Number(value);
		if (!Number.isFinite(n)) return value;
		const unit = normalizeUnit(options.unit);
		if (unit === CELSIUS) return n;
		return options.delta ? fahrenheitDeltaToCelsius(n) : fahrenheitToCelsius(n);
	}

	function convert(value, options = {}) {
		const n = Number(value);
		if (!Number.isFinite(n)) return value;
		const from = normalizeUnit(options.from);
		const to = normalizeUnit(options.to);
		if (from === to) return n;
		if (from === CELSIUS) {
			return options.delta ? celsiusDeltaToFahrenheit(n) : celsiusToFahrenheit(n);
		}
		return options.delta ? fahrenheitDeltaToCelsius(n) : fahrenheitToCelsius(n);
	}

	function unitSymbol(unit) {
		const normalized = normalizeUnit(unit);
		if (normalized === FAHRENHEIT) {
			return '°F';
		}
		return '°C';
	}

	function getStoredUnit() {
		try {
			const value = global.localStorage.getItem(TEMP_UNIT_STORAGE_KEY);
			return normalizeUnit(value);
		} catch (e) {
			return CELSIUS;
		}
	}

	function setStoredUnit(unit) {
		try {
			global.localStorage.setItem(TEMP_UNIT_STORAGE_KEY, normalizeUnit(unit));
		} catch (e) {
			// Ignore storage write failures.
		}
	}

	function _toFiniteOrNull(value) {
		const n = Number(value);
		return Number.isFinite(n) ? n : null;
	}

	function getStoredTemperatureRangeC() {
		try {
			const start = global.localStorage.getItem(TEMP_RANGE_START_C_STORAGE_KEY);
			const end = global.localStorage.getItem(TEMP_RANGE_END_C_STORAGE_KEY);
			if (start === null || end === null) { // if one of the values is not set in localStorage
				return { start: null, end: null }; // return an empty range
			}
			return { start: _toFiniteOrNull(start), end: _toFiniteOrNull(end) }; // otherwise, try to work with the stored values
		} catch (e) {
			return { start: null, end: null };
		}
	}

	function setStoredTemperatureRangeC(start, end) {
		try {
			const startN = _toFiniteOrNull(start);
			const endN = _toFiniteOrNull(end);
			if (startN === null) {
				global.localStorage.removeItem(TEMP_RANGE_START_C_STORAGE_KEY);
			} else {
				global.localStorage.setItem(TEMP_RANGE_START_C_STORAGE_KEY, String(startN));
			}
			if (endN === null) {
				global.localStorage.removeItem(TEMP_RANGE_END_C_STORAGE_KEY);
			} else {
				global.localStorage.setItem(TEMP_RANGE_END_C_STORAGE_KEY, String(endN));
			}
		} catch (e) {
			// Ignore storage write failures.
		}
	}

	global.TempUnits = {
		CELSIUS,
		FAHRENHEIT,
		// TEMP_UNIT_STORAGE_KEY,
		// TEMP_RANGE_START_C_STORAGE_KEY,
		// TEMP_RANGE_END_C_STORAGE_KEY,
		DECIMAL_PLACES,
		DECIMAL_PLACES_DELTA,
		normalizeUnit,
		fromCelsius,
		toCelsius,
		convert,
		unitSymbol,
		getStoredUnit,
		setStoredUnit,
		getStoredTemperatureRangeC,
		setStoredTemperatureRangeC,
	};
})(window);
