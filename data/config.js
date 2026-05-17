// Alpine.js component for config.html
function configApp() {
	return {
		cfg: {},
		notification: null,
		notificationClass: '',
		_notifTimer: null,

		async load() {
			try {
				const res = await fetch('/api/state');
				if (!res.ok) return;
				const data = await res.json();
				// Extract only config properties (exclude telemetry)
				this.cfg = {
					hostname:               data.hostname ?? 'cool-bed',
					mqtt:                   data.mqtt ?? false,
					mqttServer:             data.mqttServer ?? '',
					mqttPort:               data.mqttPort ?? 1883,
					mqttUsername:           data.mqttUsername ?? '',
					mqttPassword:           '',  // never pre-fill password
					mqttRootTopic:          data.mqttRootTopic ?? 'cool-bed',
					mqttHADiscovery:        data.mqttHADiscovery ?? true,
					mqttHADiscoveryTopic:   data.mqttHADiscoveryTopic ?? 'homeassistant',
					speedSetPoint:          data.speedSetPoint ?? 128,
					temperatureSetPoint:    data.temperatureSetPoint ?? 20,
					outTemperatureCalibrationOffset: data.outTemperatureCalibrationOffset ?? 0,
					returnTemperatureCalibrationOffset: data.returnTemperatureCalibrationOffset ?? 0,
					calibrationVolume:      data.calibrationVolume ?? 500,
					calibrationFlow:        data.calibrationFlow ?? 1.0,
					calibrationFlowPulses:  data.calibrationFlowPulses ?? 500,
					systemTime:             data.systemTime ?? 30,
					minFlowPulsesPerSec:    data.minFlowPulsesPerSec ?? 5,
					maxCurrent:             data.maxCurrent ?? 1000,
					minVoltage:             data.minVoltage ?? 4000,
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
