(function (global) {
	const CLIENT_RANGES = { // these are the range ids used below
		TEMPERATURE: 'temperatureSetPoint',
		CIRC_SPEED: 'circSpeedSetPoint',
		COOLING_SPEED: 'coolingSpeedSetPoint',
		HEATING_SPEED: 'heatingSpeedSetPoint',
	};
	const CLIENT_RANGE_STORAGE_KEYS = { // this is internal consts for this module
		[CLIENT_RANGES.TEMPERATURE]: {
			start: 'coolBed.temperatureRangeStartC',
			end: 'coolBed.temperatureRangeEndC',
		},
		[CLIENT_RANGES.CIRC_SPEED]: {
			start: 'coolBed.circSpeedRangeStart',
			end: 'coolBed.circSpeedRangeEnd',
		},
		[CLIENT_RANGES.COOLING_SPEED]: {
			start: 'coolBed.coolingSpeedRangeStart',
			end: 'coolBed.coolingSpeedRangeEnd',
		},
		[CLIENT_RANGES.HEATING_SPEED]: {
			start: 'coolBed.heatingSpeedRangeStart',
			end: 'coolBed.heatingSpeedRangeEnd',
		},
	};

	function toFiniteOrNull(value) {
		const n = Number(value);
		return Number.isFinite(n) ? n : null;
	}

	function getStoredClientRange(rangeId) {
		const keys = CLIENT_RANGE_STORAGE_KEYS[rangeId];
		if (!keys) return { start: null, end: null };
		try {
			const start = global.localStorage.getItem(keys.start);
			const end = global.localStorage.getItem(keys.end);
			if (start === null || end === null) return { start: null, end: null };
			return { start: toFiniteOrNull(start), end: toFiniteOrNull(end) };
		} catch (e) {
			return { start: null, end: null };
		}
	}

	function setStoredClientRange(rangeId, start, end) {
		const keys = CLIENT_RANGE_STORAGE_KEYS[rangeId];
		if (!keys) return;
		try {
			const startN = toFiniteOrNull(start);
			const endN = toFiniteOrNull(end);
			if (startN === null) {
				global.localStorage.removeItem(keys.start);
			} else {
				global.localStorage.setItem(keys.start, String(startN));
			}
			if (endN === null) {
				global.localStorage.removeItem(keys.end);
			} else {
				global.localStorage.setItem(keys.end, String(endN));
			}
		} catch (e) {
			// Ignore storage write failures.
		}
	}

	global.Ranges = {
		CLIENT_RANGES,
		getStoredClientRange,
		setStoredClientRange,
	};
})(window);
