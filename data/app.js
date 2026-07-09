// ---- Constants ----
const NORMAL_INTERVAL_MS = 5000;
const FAST_INTERVAL_MS = 1000;
const FAST_POLL_COUNT = 10;
const HISTORY_DURATION_MS = 10 * 60 * 1000; // 10 minutes
const STATE_STALE_MS = 60 * 1000; // 1 minute

const TEMP_TELEMETRY_ABSOLUTE_KEYS = new Set([
	'outTemperature',
	'returnTemperature',
	'coolingTemperature',
]);

const TEMP_TELEMETRY_DELTA_KEYS = new Set([
	'temperatureDelta',
]);

const TEMP_CONFIG_ABSOLUTE_KEYS = new Set([
	'temperatureSetPoint',
]);

const TEMP_CONFIG_DELTA_KEYS = new Set([
	'outTemperatureCalibrationOffset',
	'returnTemperatureCalibrationOffset',
	'coolingTemperatureCalibrationOffset',
]);

// ---- Alpine.js app ----
function coolBedApp() {
	return {
		state: {},
		model: { config: {}, telemetry: {}, modes: [] },
		modelLoaded: false,
		lastValidStateAt: 0,
		nowMs: Date.now(),
		temperatureUnit: 'C',
		temperatureSetPointDisplay: 0, // the value of the temperature set point input field. This is in temperatureUnit. The value in state is in Celsius, so we convert it for display and convert back on submit.

		// Polling state
		_pollTimer: null,
		_fastPollRemaining: 0,
		_uiClockTimer: null,
		_history: [], // array of { timestamp, ...metrics }
		_chart: null,
		_editingTemperatureSetPoint: false,

		async init() {
			this._loadTemperatureUnitPreference();
			this._startUiClock();
			await this._fetchModel();
			await this._fetchState();
			this._startPolling();
			this._initChart();
		},

		_startUiClock() {
			if (this._uiClockTimer) clearInterval(this._uiClockTimer);
			this._uiClockTimer = setInterval(() => {
				this.nowMs = Date.now();
			}, 1000);
		},

		async _fetchModel() {
			try {
				const res = await fetch('/api/model');
				if (!res.ok) return;
				this.model = await res.json();
				this.modelLoaded = true;
			} catch (e) {
				console.error('Failed to fetch model:', e);
			}
		},

		_getConfigMeta(key) {
			return this.model?.config?.[key] || {};
		},

		_getTelemetryMeta(key) {
			return this.model?.telemetry?.[key] || {};
		},

		hasConfigKey(key) {
			return Object.prototype.hasOwnProperty.call(this.model?.config || {}, key);
		},

		hasTelemetryKey(key) {
			return Object.prototype.hasOwnProperty.call(this.model?.telemetry || {}, key);
		},

		hasMode(mode) {
			return Array.isArray(this.model?.modes) && this.model.modes.includes(mode);
		},

		hasAnyTelemetry(keys) {
			return keys.some((key) => this.hasTelemetryKey(key));
		},

		labelFor(key, fallback = '') {
			const meta = this._getConfigMeta(key);
			return meta.description || fallback;
		},

		unitsFor(key, isConfig = true) {
			if (isConfig && this._isTemperatureConfigKey(key)) {
				return this.temperatureUnitLabel();
			}
			if (!isConfig && this._isTemperatureTelemetryKey(key)) {
				return this.temperatureUnitLabel();
			}
			const meta = isConfig ? this._getConfigMeta(key) : this._getTelemetryMeta(key);
			return meta.units || '';
		},

		minFor(key) {
			const min = this._getConfigMeta(key).min;
			if (!Number.isFinite(min)) return null;
			if (this._isTemperatureConfigKey(key)) {
				return this.fromCelsius(min, this._isTemperatureDeltaConfigKey(key));
			}
			return min;
		},

		maxFor(key) {
			const max = this._getConfigMeta(key).max;
			if (!Number.isFinite(max)) return null;
			if (this._isTemperatureConfigKey(key)) {
				return this.fromCelsius(max, this._isTemperatureDeltaConfigKey(key));
			}
			return max;
		},

		defaultFor(key, fallback = 0) {
			const val = this._getConfigMeta(key).defaultValue;
			return val !== undefined && val !== null ? val : fallback;
		},

		formatTelemetry(key, decimals = 0) {
			const rawValue = this.state[key];
			const converted = this._isTemperatureTelemetryKey(key)
				? this.fromCelsius(rawValue, this._isTemperatureDeltaTelemetryKey(key))
				: rawValue;
			const n = Number(converted ?? 0);
			const value = Number.isFinite(n) ? n.toFixed(decimals) : String(rawValue ?? 0);
			const units = this.unitsFor(key, false);
			return units ? `${value} ${units}` : value;
		},

		formatTemperatureSetPointDisplay() {
			const n = Number(this.temperatureSetPointDisplay ?? 0);
			const value = Number.isFinite(n) ? n.toFixed(TempUnits.DECIMAL_PLACES) : '0.0';
			const units = this.unitsFor('temperatureSetPoint');
			return units ? `${value} ${units}` : value;
		},

		async _fetchState() {
			try {
				const res = await fetch('/api/state');
				if (!res.ok) {
					console.warn('Fetch not ok, text:', await res.text());
					return;
				}
				const data = await res.json();
				this.state = data;
				if (!this._editingTemperatureSetPoint) { // if not editing the set point, update the display value to reflect any changes from outside
					this.temperatureSetPointDisplay = this.fromCelsius(
						data.temperatureSetPoint ?? this.defaultFor('temperatureSetPoint', 0),
						false
					); // update display value to match state, converting from Celsius to display unit
				}
				if (this._isStatePayloadValid(data)) {
					this.lastValidStateAt = Date.now();
					this._recordHistory(data); // this will trigger updateChart via _history watcher in next microtask switch
				} else {
					console.warn('Received invalid state payload:', data);
				}
			} catch (e) {
				console.error('Failed to fetch state:', e);
			}
		},

		_isStatePayloadValid(data) {
			return data && Object.prototype.hasOwnProperty.call(data, 'status');
		},

		isStateFresh() {
			if (!this.lastValidStateAt) return false;
			return (this.nowMs - this.lastValidStateAt) <= STATE_STALE_MS;
		},

		stateStatusTitle() {
			return this.lastValidStateAt ? 'Reconnecting' : 'Loading';
		},

		stateStatusMessage() {
			if (!this.lastValidStateAt) return 'Waiting for valid state...';
			return `Reconnecting, last connected ${this._formatElapsed(this.nowMs - this.lastValidStateAt)} ago`;
		},

		_formatElapsed(ms) {
			const minutes = Math.floor(ms / 60000);
			if (minutes <= 0) return 'less than a minute';
			if (minutes === 1) return '1 minute';
			return `${minutes} minutes`;
		},

		_recordHistory(data) {
			const now = Date.now();
			this._history.push({
				timestamp: now,
				flow: data.flow ?? 0,
				temperatureDelta: data.temperatureDelta ?? 0,
				coolingPower: data.coolingPower ?? 0,
				outTemperature: data.outTemperature ?? 0,
				returnTemperature: data.returnTemperature ?? 0,
				coolingTemperature: data.coolingTemperature ?? 0,
				circSpeed: data.circSpeed ?? 0,
				circCurrent: data.circCurrent ?? 0,
				circVoltage: data.circVoltage ?? 0,
				flowPulsesFilteredPerSec: data.flowPulsesFilteredPerSec ?? 0,
			});
			// Trim to 10-minute window
			const cutoff = now - HISTORY_DURATION_MS;
			this._history = this._history.filter(h => h.timestamp >= cutoff);
		},

		_startPolling() {
			const poll = async () => {
				await this._fetchState();
				const interval = this._fastPollRemaining > 0
					? FAST_INTERVAL_MS
					: NORMAL_INTERVAL_MS;
				if (this._fastPollRemaining > 0) this._fastPollRemaining--;
				this._pollTimer = setTimeout(poll, interval);
			};
			poll();
		},

		_triggerFastPoll() {
			this._fastPollRemaining = FAST_POLL_COUNT;
			if (this._pollTimer) {
				clearTimeout(this._pollTimer);
				this._pollTimer = null;
			}
			this._startPolling();
		},

		async _postMode(mode, extra = {}) {
			try {
				const body = { mode, ...extra };
				const res = await fetch('/api/mode', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify(body),
				});
				if (!res.ok) console.error('setMode failed:', await res.text());
			} catch (e) {
				console.error('setMode error:', e);
			}
			this._triggerFastPoll();
		},

		async _postConfig(patch) {
			try {
				const res = await fetch('/api/config', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify(patch),
				});
				if (!res.ok) console.error('postConfig failed:', await res.text());
			} catch (e) {
				console.error('postConfig error:', e);
			}
		},

		setMode(mode) {
			this._postMode(mode);
		},

		onCircSpeedSetPointChanged() {
			this._postConfig({ circSpeedSetPoint: this.state.circSpeedSetPoint });
		},

		onTemperatureSetPointChanged() {
			const valueC = this.toCelsius(this.temperatureSetPointDisplay, false);
			this.state.temperatureSetPoint = valueC;
			this._postConfig({ temperatureSetPoint: valueC });
			this._editingTemperatureSetPoint = false;
		},

		onTemperatureSetPointInput(value) {
			this._editingTemperatureSetPoint = true;
			this.temperatureSetPointDisplay = Number(value);
		},

		onCalibrationVolumeChanged() {
			this._postConfig({ calibrationVolume: this.state.calibrationVolume });
		},

		startManualCirc() {
			this._postMode('manual_circ');
		},

		startManualCool() {
			this._postMode('manual_cool');
		},

		startManualHeat() {
			this._postMode('manual_heat');
		},

		startTemperature() {
			this._postMode('temperature');
		},

		startTemperaturePrepare() {
			this._postMode('temperature_prepare');
		},

		startFlowCalibration() {
			this._postMode('flow_calibration');
		},

		startFlowTest() {
			this._postMode('flow_test');
		},

		_loadTemperatureUnitPreference() {
			this.temperatureUnit = TempUnits.getStoredUnit();
		},

		temperatureUnitLabel() {
			return TempUnits.unitSymbol(this.temperatureUnit);
		},

		fromCelsius(value, isDelta = false) {
			return TempUnits.fromCelsius(value, { unit: this.temperatureUnit, delta: isDelta });
		},

		toCelsius(value, isDelta = false) {
			return TempUnits.toCelsius(value, { unit: this.temperatureUnit, delta: isDelta });
		},

		_isTemperatureConfigKey(key) {
			return TEMP_CONFIG_ABSOLUTE_KEYS.has(key) || TEMP_CONFIG_DELTA_KEYS.has(key);
		},

		_isTemperatureDeltaConfigKey(key) {
			return TEMP_CONFIG_DELTA_KEYS.has(key);
		},

		_isTemperatureTelemetryKey(key) {
			return TEMP_TELEMETRY_ABSOLUTE_KEYS.has(key) || TEMP_TELEMETRY_DELTA_KEYS.has(key);
		},

		_isTemperatureDeltaTelemetryKey(key) {
			return TEMP_TELEMETRY_DELTA_KEYS.has(key);
		},

		// ---- Chart ----
		_initChart() {
			const createChartConfig = (datasets) => ({
				type: 'line',
				data: { labels: [], datasets },
				options: {
					animation: false,
					responsive: true,
					interaction: { mode: 'index', intersect: false },
					plugins: {
						legend: { labels: { color: '#e2e8f0', boxWidth: 12 } },
					},
					scales: {
						x: {
							ticks: { color: '#64748b', maxTicksLimit: 8 },
							grid:  { color: '#1e293b' },
						},
						y: {
							type: 'linear', position: 'left',
							ticks: { color: '#64748b' },
							grid:  { color: '#334155' },
						},
					},
				},
			});

			const tSeries = [];
			if (this.hasTelemetryKey('outTemperature')) tSeries.push({ key: 'outTemperature', labelBase: 'Out Temp', color: '#f97316', isTemp: true });
			if (this.hasTelemetryKey('returnTemperature')) tSeries.push({ key: 'returnTemperature', labelBase: 'Return Temp', color: '#f43f5e', isTemp: true });
			if (this.hasTelemetryKey('coolingTemperature')) tSeries.push({ key: 'coolingTemperature', labelBase: 'Cooling Temp', color: '#0ea5e9', isTemp: true });
			const tDatasets = tSeries.map((s) => ({ label: s.labelBase, data: [], borderColor: s.color, tension: 0.3, yAxisID: 'y' }));

			const fSeries = [];
			if (this.hasTelemetryKey('flow')) fSeries.push({ key: 'flow', labelBase: 'Flow (L/min)', color: '#60a5fa', isTemp: false });
			const fDatasets = fSeries.map((s) => ({ label: s.labelBase, data: [], borderColor: s.color, tension: 0.3, yAxisID: 'y' }));

			const mSeries = [];
			if (this.hasTelemetryKey('coolingPower')) mSeries.push({ key: 'coolingPower', labelBase: 'Cooling Power (W)', color: '#22c55e', isTemp: false });
			if (this.hasTelemetryKey('circSpeed')) mSeries.push({ key: 'circSpeed', labelBase: 'Circulation Speed', color: '#a78bfa', isTemp: false });
			if (this.hasTelemetryKey('circCurrent')) mSeries.push({ key: 'circCurrent', labelBase: 'Current (mA)', color: '#f43f5e', isTemp: false });
			const mDatasets = mSeries.map((s) => ({ label: s.labelBase, data: [], borderColor: s.color, tension: 0.3, yAxisID: 'y' }));

			// create objects for charts.js and don't store them in state, to avoid reactivity overhead and chart re-creation on every update
			let tChart = null;
			if (tDatasets.length > 0) {
				const tCtx = this.$refs.temperatureChart?.getContext('2d');
				if (tCtx) tChart = new Chart(tCtx, createChartConfig(tDatasets));
			}
			// flow
			let fChart = null;
			if (fDatasets.length > 0) {
				const fCtx = this.$refs.flowChart?.getContext('2d');
				if (fCtx) fChart = new Chart(fCtx, createChartConfig(fDatasets));
			}
			// misc graphs
			let mChart = null;
			if (mDatasets.length > 0) {
				const mCtx = this.$refs.miscChart?.getContext('2d');
				if (mCtx) mChart = new Chart(mCtx, createChartConfig(mDatasets));
			}


			// setup watch on _history to update chart when new data comes in
			this.$watch('_history', (history) => {
				// this will run every time _history changes, which happens on every new state fetch that has valid data. We extract the relevant arrays for the chart and update it.
				const temperatureUnits = this.temperatureUnitLabel();
				const labels = history.map(h =>
					new Date(h.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
				);

				tSeries.forEach((series, idx) => {
					tDatasets[idx].label = `${series.labelBase} (${temperatureUnits})`;
					tDatasets[idx].data = history.map((h) => this.fromCelsius(h[series.key], false));
				});
				fSeries.forEach((series, idx) => {
					fDatasets[idx].label = series.labelBase;
					fDatasets[idx].data = history.map((h) => h[series.key]);
				});
				mSeries.forEach((series, idx) => {
					mDatasets[idx].label = series.labelBase;
					mDatasets[idx].data = history.map((h) => h[series.key]);
				});

				const updateSafely = (chart) => {
					chart.setActiveElements([]);
					if (chart.tooltip?.setActiveElements) {
						chart.tooltip.setActiveElements([], { x: 0, y: 0 });
					}
					chart.data.labels = labels;
					chart.update('none');
				};

				if (tChart) updateSafely(tChart);
				if (fChart) updateSafely(fChart);
				if (mChart) updateSafely(mChart);
			}/*, { deep: true }*/); 
			// we don't need deep watch because we replace the _history array entirely on each update, so shallow watch is sufficient.
			// if we ever just push to the array without replacing it, we would need deep watch.
		},
	};
}
