// Alpine.js component for config.html
function configApp() {
	return {
		cfg: {},
		model: { config: {}, telemetry: {}, modes: [] },
		notification: null,
		notificationClass: '',
		_notifTimer: null,

		_meta(key) {
			return this.model?.config?.[key] || {};
		},

		labelFor(key, fallback = '') {
			return this._meta(key).description || fallback;
		},

		unitsFor(key) {
			return this._meta(key).units || '';
		},

		minFor(key) {
			const min = this._meta(key).min;
			return Number.isFinite(min) ? min : null;
		},

		maxFor(key) {
			const max = this._meta(key).max;
			return Number.isFinite(max) ? max : null;
		},

		defaultFor(key, fallback = null) {
			const value = this._meta(key).defaultValue;
			return value !== undefined && value !== null ? value : fallback;
		},

		async load() {
			try {
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
					speedSetPoint:          data.speedSetPoint ?? this.defaultFor('speedSetPoint', 0),
					temperatureSetPoint:    data.temperatureSetPoint ?? this.defaultFor('temperatureSetPoint', 0),
					outTemperatureCalibrationOffset: data.outTemperatureCalibrationOffset ?? this.defaultFor('outTemperatureCalibrationOffset', 0),
					returnTemperatureCalibrationOffset: data.returnTemperatureCalibrationOffset ?? this.defaultFor('returnTemperatureCalibrationOffset', 0),
					coolingTemperatureCalibrationOffset: data.coolingTemperatureCalibrationOffset ?? this.defaultFor('coolingTemperatureCalibrationOffset', 0),
					calibrationVolume:      data.calibrationVolume ?? this.defaultFor('calibrationVolume', 0),
					calibrationFlowPulses:  data.calibrationFlowPulses ?? this.defaultFor('calibrationFlowPulses', 0),
					systemTime:             data.systemTime ?? this.defaultFor('systemTime', 0),
					minFlowPulsesPerSec:    data.minFlowPulsesPerSec ?? this.defaultFor('minFlowPulsesPerSec', 0),
					maxCurrent:             data.maxCurrent ?? this.defaultFor('maxCurrent', 0),
					minVoltage:             data.minVoltage ?? this.defaultFor('minVoltage', 0),
				};
			} catch (e) {
				console.error('Failed to load config:', e);
			}
		},

		async save() {
			try {
				// Don't send empty password (means "keep unchanged")
				const payload = { ...this.cfg };
				if (!payload.mqttPassword) delete payload.mqttPassword;

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
	};
}
