// Alpine.js component for config.html
const TEMP_CONFIG_ABSOLUTE_KEYS = new Set([
	'temperatureSetPoint',
]);

const TEMP_CLIENT_ABSOLUTE_KEYS = new Set([
	'temperatureRangeStart',
	'temperatureRangeEnd',
]);

const LOCAL_RANGE_FIELDS = {
	temperatureSetPoint: { start: 'temperatureRangeStart', end: 'temperatureRangeEnd' },
	circSpeedSetPoint: { start: 'circSpeedRangeStart', end: 'circSpeedRangeEnd' },
	coolingSpeedSetPoint: { start: 'coolingSpeedRangeStart', end: 'coolingSpeedRangeEnd' },
	heatingSpeedSetPoint: { start: 'heatingSpeedRangeStart', end: 'heatingSpeedRangeEnd' },
};

const TEMP_CONFIG_DELTA_KEYS = new Set([
	'outTemperatureCalibrationOffset',
	'returnTemperatureCalibrationOffset',
	'coolingTemperatureCalibrationOffset',
]);

function configApp() {
	return {
		cfg: {}, // the values in the config form. temperature values are in the current temperature unit (C or F), not necessarily Celsius.
		model: { config: {}, telemetry: {}, modes: [] },
		isLoading: true,
		temperatureUnit: 'C', // current temperature unit (C or F). This is used to convert temperature values in cfg to/from Celsius when saving/loading. This is a client side setting.
		notification: null,
		notificationClass: '',
		_notifTimer: null,

		// returns the metadata for a config key, or an empty object if not found
		_meta(key) {
			return this.model?.config?.[key] || {};
		},

		hasConfigKey(key) {
			return Object.prototype.hasOwnProperty.call(this.model?.config || {}, key);
		},

		hasAnyConfig(keys) {
			return keys.some((key) => this.hasConfigKey(key));
		},

		labelFor(key, fallback = '') {
			return this._meta(key).description || fallback;
		},

		unitsFor(key) {
			if (this._isTemperatureConfigKey(key)) {
				return this.temperatureUnitLabel();
			}
			return this._meta(key).units || '';
		},

		minFor(key) {
			if (this._isLocalRangeManagedKey(key)) {
				return this._effectiveDisplayRangeForKey(key).start;
			}
			const min = this._meta(key).min;
			if (!Number.isFinite(min)) return null;
			if (this._isTemperatureConfigKey(key)) {
				return this._fromCelsius(min, this._isTemperatureDeltaConfigKey(key));
			}
			return min;
		},

		maxFor(key) {
			if (this._isLocalRangeManagedKey(key)) {
				return this._effectiveDisplayRangeForKey(key).end;
			}
			const max = this._meta(key).max;
			if (!Number.isFinite(max)) return null;
			if (this._isTemperatureConfigKey(key)) {
				return this._fromCelsius(max, this._isTemperatureDeltaConfigKey(key));
			}
			return max;
		},

		defaultFor(key, fallback = null) {
			const value = this._meta(key).defaultValue;
			return value !== undefined && value !== null ? value : fallback;
		},

		async load() {
			console.log('Loading config...');
			try {
				this.isLoading = true;
				this.temperatureUnit = TempUnits.getStoredUnit();

				const [modelRes, stateRes] = await Promise.all([
					fetch('/api/model'),
					fetch('/api/state'),
				]);
				if (!modelRes.ok || !stateRes.ok) return;

				this.model = await modelRes.json();
				const data = await stateRes.json();

				// Extract only config properties (exclude telemetry)
				this.cfg = {
					hostname:               data.hostname ?? this.defaultFor('hostname', ''),
					mqtt:                   data.mqtt ?? this.defaultFor('mqtt', false),
					mqttServer:             data.mqttServer ?? this.defaultFor('mqttServer', ''),
					mqttPort:               data.mqttPort ?? this.defaultFor('mqttPort', 0),
					mqttUsername:           data.mqttUsername ?? this.defaultFor('mqttUsername', ''),
					mqttPassword:           '',  // never pre-fill password
					mqttRootTopic:          data.mqttRootTopic ?? this.defaultFor('mqttRootTopic', ''),
					mqttHADiscovery:        data.mqttHADiscovery ?? this.defaultFor('mqttHADiscovery', true),
					mqttHADiscoveryTopic:   data.mqttHADiscoveryTopic ?? this.defaultFor('mqttHADiscoveryTopic', ''),
					circSpeedSetPoint:      data.circSpeedSetPoint ?? this.defaultFor('circSpeedSetPoint', 0),
					coolingSpeedSetPoint:   data.coolingSpeedSetPoint ?? this.defaultFor('coolingSpeedSetPoint', 0),
					heatingSpeedSetPoint:   data.heatingSpeedSetPoint ?? this.defaultFor('heatingSpeedSetPoint', 0),
					temperatureSetPoint:    this._displayConfigValue('temperatureSetPoint', data.temperatureSetPoint ?? this.defaultFor('temperatureSetPoint', 0)),
					outTemperatureCalibrationOffset: this._displayConfigValue('outTemperatureCalibrationOffset', data.outTemperatureCalibrationOffset ?? this.defaultFor('outTemperatureCalibrationOffset', 0)),
					returnTemperatureCalibrationOffset: this._displayConfigValue('returnTemperatureCalibrationOffset', data.returnTemperatureCalibrationOffset ?? this.defaultFor('returnTemperatureCalibrationOffset', 0)),
					coolingTemperatureCalibrationOffset: this._displayConfigValue('coolingTemperatureCalibrationOffset', data.coolingTemperatureCalibrationOffset ?? this.defaultFor('coolingTemperatureCalibrationOffset', 0)),
					calibrationVolume:      data.calibrationVolume ?? this.defaultFor('calibrationVolume', 0),
					calibrationFlowPulses:  data.calibrationFlowPulses ?? this.defaultFor('calibrationFlowPulses', 0),
					systemTime:             data.systemTime ?? this.defaultFor('systemTime', 0),
					minFlowPulsesPerSec:    data.minFlowPulsesPerSec ?? this.defaultFor('minFlowPulsesPerSec', 0),
					maxCircCurrent:         data.maxCircCurrent ?? this.defaultFor('maxCircCurrent', 0),
					minCircCurrent:         data.minCircCurrent ?? this.defaultFor('minCircCurrent', 0),
					minCircVoltage:         data.minCircVoltage ?? this.defaultFor('minCircVoltage', 0),
					maxCoolingCurrent:      data.maxCoolingCurrent ?? this.defaultFor('maxCoolingCurrent', 0),
					minCoolingCurrent:      data.minCoolingCurrent ?? this.defaultFor('minCoolingCurrent', 0),
					minCoolingVoltage:      data.minCoolingVoltage ?? this.defaultFor('minCoolingVoltage', 0),
					maxHeatingCurrent:      data.maxHeatingCurrent ?? this.defaultFor('maxHeatingCurrent', 0),
					minHeatingCurrent:      data.minHeatingCurrent ?? this.defaultFor('minHeatingCurrent', 0),
					minHeatingVoltage:      data.minHeatingVoltage ?? this.defaultFor('minHeatingVoltage', 0),
				};
				this._loadValidatedClientRangesIntoConfig();
				this._clampAllManagedSetPointValues();
			} catch (e) {
				console.error('Failed to load config:', e);
			} finally {
				this.isLoading = false;
			}
		},

		async save() {
			TempUnits.setStoredUnit(this.temperatureUnit); // persist temperature unit setting (this is client side config)
			const validationError = this._validateAndPersistAllClientRanges();
			if (validationError) {
				this._showNotification(validationError, 'error');
				return;
			}
			this._clampAllManagedSetPointValues();

			// process and send server side config
			try {
				// Only send keys that exist in the current model (hardware dependent config can be absent).
				const payload = {};
				for (const key of Object.keys(this.model?.config || {})) {
					if (Object.prototype.hasOwnProperty.call(this.cfg, key)) {
						payload[key] = this.cfg[key];
					}
				}

				// Don't send empty password (means "keep unchanged")
				if (!payload.mqttPassword) delete payload.mqttPassword;

				// convert temperature values back to Celsius before sending to the server
				for (const key of TEMP_CONFIG_ABSOLUTE_KEYS) {
					if (payload[key] !== undefined && payload[key] !== null) {
						payload[key] = this._toCelsius(payload[key], false);
					}
				}
				for (const key of TEMP_CONFIG_DELTA_KEYS) {
					if (payload[key] !== undefined && payload[key] !== null) {
						payload[key] = this._toCelsius(payload[key], true);
					}
				}

				// send
				const res = await fetch('/api/config', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify(payload),
				});

				if (res.ok) {
					this._showNotification('Configuration saved successfully.', 'success');
				} else {
					const text = await res.text();
					this._showNotification('Save failed: ' + text, 'error');
				}
			} catch (e) {
				this._showNotification('Save error: ' + e.message, 'error');
			}
		},

		_showNotification(msg, type) {
			this.notification = msg;
			this.notificationClass = type;
			if (this._notifTimer) clearTimeout(this._notifTimer);
			this._notifTimer = setTimeout(() => { this.notification = null; }, 5000);
		},

		changeTemperatureUnit(nextUnit) {
			const normalizedNext = TempUnits.normalizeUnit(nextUnit);
			const previous = this.temperatureUnit;
			if (normalizedNext === previous) return;

			// first change the unit, otherwise the range control will not update properly.
			this.temperatureUnit = normalizedNext;

			// convert relevant config properties to the new values based on the new unit.
			for (const key of TEMP_CONFIG_ABSOLUTE_KEYS) {
				this.cfg[key] = TempUnits.convert(this.cfg[key], {
					from: previous,
					to: normalizedNext,
					delta: false,
				});
			}
			for (const key of TEMP_CLIENT_ABSOLUTE_KEYS) {
				this.cfg[key] = TempUnits.convert(this.cfg[key], {
					from: previous,
					to: normalizedNext,
					delta: false,
				});
			}
			for (const key of TEMP_CONFIG_DELTA_KEYS) {
				this.cfg[key] = TempUnits.convert(this.cfg[key], {
					from: previous,
					to: normalizedNext,
					delta: true,
				});
			}
			this._clampAllManagedSetPointValues();
		},

		temperatureUnitLabel() {
			return TempUnits.unitSymbol(this.temperatureUnit);
		},

		_displayConfigValue(key, value) {
			if (!this._isTemperatureConfigKey(key)) return value;
			return this._fromCelsius(value, this._isTemperatureDeltaConfigKey(key));
		},

		_fromCelsius(value, isDelta = false) {
			return TempUnits.fromCelsius(value, { unit: this.temperatureUnit, delta: isDelta });
		},

		_toCelsius(value, isDelta = false) {
			return TempUnits.toCelsius(value, { unit: this.temperatureUnit, delta: isDelta });
		},

		_isTemperatureConfigKey(key) {
			return TEMP_CONFIG_ABSOLUTE_KEYS.has(key) || TEMP_CONFIG_DELTA_KEYS.has(key);
		},

		_isTemperatureDeltaConfigKey(key) {
			return TEMP_CONFIG_DELTA_KEYS.has(key);
		},

		_toFiniteOrNull(value) {
			const n = Number(value);
			return Number.isFinite(n) ? n : null;
		},

		_serverRangeForKey(key) {
			const meta = this._meta(key);
			return {
				start: this._toFiniteOrNull(meta.min),
				end: this._toFiniteOrNull(meta.max),
			};
		},

		_hasValidServerRange(key) {
			const range = this._serverRangeForKey(key);
			return Number.isFinite(range.start) && Number.isFinite(range.end) && range.start <= range.end;
		},

		_localRangeFieldsForKey(key) {
			return LOCAL_RANGE_FIELDS[key];
		},

		_isLocalRangeManagedKey(key) {
			return Object.prototype.hasOwnProperty.call(LOCAL_RANGE_FIELDS, key);
		},

		_isTemperatureRangeManagedKey(key) {
			return key === 'temperatureSetPoint';
		},

		_toCanonicalRangeValue(key, displayValue) {
			const n = this._toFiniteOrNull(displayValue);
			if (n === null) return null;
			if (this._isTemperatureRangeManagedKey(key)) {
				return this._toFiniteOrNull(this._toCelsius(n, false));
			}
			return n;
		},

		_fromCanonicalRangeValue(key, canonicalValue) {
			if (canonicalValue === null || canonicalValue === undefined) return null;
			if (this._isTemperatureRangeManagedKey(key)) {
				return this._fromCelsius(canonicalValue, false);
			}
			return canonicalValue;
		},

		_loadValidatedClientRangeForKey(key) {
			const server = this._serverRangeForKey(key);
			const stored = Ranges.getStoredClientRange(key);
			if (!this._hasValidServerRange(key)) {
				return {
					start: this._toFiniteOrNull(stored.start),
					end: this._toFiniteOrNull(stored.end),
				};
			}

			// Initialize start and end with stored values, fallback to server values if not finite/defined.
			let start = stored.start;
			let end = stored.end;
			if (!Number.isFinite(start)) start = server.start;
			if (!Number.isFinite(end)) end = server.end;

			// Clamp start and end to the server-defined range.
			start = this._clampToRange(start, server.start, server.end);
			end = this._clampToRange(end, server.start, server.end);

			// If the start is greater than the end, reset to server range.
			if (start > end) {
				start = server.start;
				end = server.end;
			}

			Ranges.setStoredClientRange(key, start, end);
			return { start, end };
		},

		_loadValidatedClientRangesIntoConfig() {
			for (const key of Object.keys(LOCAL_RANGE_FIELDS)) {
				if (!this.hasConfigKey(key)) continue;
				const fields = this._localRangeFieldsForKey(key);
				const range = this._loadValidatedClientRangeForKey(key);
				this.cfg[fields.start] = this._fromCanonicalRangeValue(key, range.start);
				this.cfg[fields.end] = this._fromCanonicalRangeValue(key, range.end);
			}
		},

		_clientCanonicalRangeFromConfigForKey(key) {
			const fields = this._localRangeFieldsForKey(key);
			return {
				start: this._toCanonicalRangeValue(key, this.cfg[fields.start]),
				end: this._toCanonicalRangeValue(key, this.cfg[fields.end]),
			};
		},

		_effectiveCanonicalRangeForKey(key) {
			const server = this._serverRangeForKey(key);
			const client = this._clientCanonicalRangeFromConfigForKey(key);
			let start = server.start;
			let end = server.end;
			if (Number.isFinite(client.start)) start = Math.max(start, client.start);
			if (Number.isFinite(client.end)) end = Math.min(end, client.end);
			if (!Number.isFinite(start) || !Number.isFinite(end) || start > end) {
				return server;
			}
			return { start, end };
		},

		_effectiveDisplayRangeForKey(key) {
			const range = this._effectiveCanonicalRangeForKey(key);
			return {
				start: this._fromCanonicalRangeValue(key, range.start),
				end: this._fromCanonicalRangeValue(key, range.end),
			};
		},

		_clampConfigValueToEffectiveRangeForKey(key) {
			if (!this.hasConfigKey(key)) return;
			const range = this._effectiveDisplayRangeForKey(key);
			if (!Number.isFinite(range.start) || !Number.isFinite(range.end)) return;
			this.cfg[key] = this._clampToRange(this.cfg[key], range.start, range.end);
		},

		_clampAllManagedSetPointValues() {
			for (const key of Object.keys(LOCAL_RANGE_FIELDS)) {
				this._clampConfigValueToEffectiveRangeForKey(key);
			}
		},

		_validateAndPersistClientRangeForKey(key) {
			const fields = this._localRangeFieldsForKey(key);
			const startDisplay = this._toFiniteOrNull(this.cfg[fields.start]);
			const endDisplay = this._toFiniteOrNull(this.cfg[fields.end]);
			if (startDisplay === null || endDisplay === null) {
				return `Client ${key} range start/end must be valid numbers.`;
			}
			if (startDisplay > endDisplay) {
				return `Client ${key} range start must be less than or equal to end.`;
			}

			const server = this._serverRangeForKey(key);
			const startCanonical = this._toCanonicalRangeValue(key, startDisplay);
			const endCanonical = this._toCanonicalRangeValue(key, endDisplay);
			if (Number.isFinite(server.start) && Number.isFinite(server.end)
				&& (startCanonical < server.start || endCanonical > server.end)) {
				const serverStart = this._fromCanonicalRangeValue(key, server.start);
				const serverEnd = this._fromCanonicalRangeValue(key, server.end);
				const units = this.unitsFor(key);
				return `Client ${key} range must stay within server range (${serverStart} to ${serverEnd}${units ? ` ${units}` : ''}).`;
			}

			Ranges.setStoredClientRange(key, startCanonical, endCanonical);
			return null;
		},

		_validateAndPersistAllClientRanges() {
			for (const key of Object.keys(LOCAL_RANGE_FIELDS)) {
				if (!this.hasConfigKey(key)) continue;
				const error = this._validateAndPersistClientRangeForKey(key);
				if (error) return error;
			}
			return null;
		},

		_clampToRange(value, min, max) {
			const n = Number(value);
			if (!Number.isFinite(n)) return min;
			return Math.min(Math.max(n, min), max);
		},

		formatTemperatureSetPoint() {
			const n = Number(this.cfg.temperatureSetPoint ?? 0);
			const value = Number.isFinite(n) ? n.toFixed(TempUnits.DECIMAL_PLACES) : '0.0';
			const units = this.unitsFor('temperatureSetPoint');
			return units ? `${value} ${units}` : value;
		},
	};
}
