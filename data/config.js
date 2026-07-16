// Alpine.js component for config.html
const TEMP_CONFIG_ABSOLUTE_KEYS = new Set([
	'temperatureSetPoint',
]);

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
			const min = this._meta(key).min;
			if (!Number.isFinite(min)) return null;
			if (this._isTemperatureConfigKey(key)) {
				return this._fromCelsius(min, this._isTemperatureDeltaConfigKey(key));
			}
			return min;
		},

		maxFor(key) {
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
			} catch (e) {
				console.error('Failed to load config:', e);
			} finally {
				this.isLoading = false;
			}
		},

		async save() {
			TempUnits.setStoredUnit(this.temperatureUnit); // persist temperature unit setting (this is client side config)

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
			for (const key of TEMP_CONFIG_DELTA_KEYS) {
				this.cfg[key] = TempUnits.convert(this.cfg[key], {
					from: previous,
					to: normalizedNext,
					delta: true,
				});
			}
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
	};
}
