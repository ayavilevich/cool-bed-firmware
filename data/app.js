// ---- Constants ----
const NORMAL_INTERVAL_MS = 5000;
const FAST_INTERVAL_MS = 1000;
const FAST_POLL_COUNT = 10;
const HISTORY_DURATION_MS = 10 * 60 * 1000; // 10 minutes
const STATE_STALE_MS = 60 * 1000; // 1 minute

// ---- Alpine.js app ----
function coolBedApp() {
	return {
		state: {},
		model: { config: {}, telemetry: {}, modes: [] },
		modelLoaded: false,
		lastValidStateAt: 0,
		nowMs: Date.now(),

		// Polling state
		_pollTimer: null,
		_fastPollRemaining: 0,
		_uiClockTimer: null,
		_history: [], // array of { timestamp, ...metrics }
		_chart: null,

		async init() {
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

		labelFor(key, fallback = '') {
			const meta = this._getConfigMeta(key);
			return meta.description || fallback;
		},

		unitsFor(key, isConfig = true) {
			const meta = isConfig ? this._getConfigMeta(key) : this._getTelemetryMeta(key);
			return meta.units || '';
		},

		minFor(key) {
			const min = this._getConfigMeta(key).min;
			return Number.isFinite(min) ? min : null;
		},

		maxFor(key) {
			const max = this._getConfigMeta(key).max;
			return Number.isFinite(max) ? max : null;
		},

		defaultFor(key, fallback = 0) {
			const val = this._getConfigMeta(key).defaultValue;
			return val !== undefined && val !== null ? val : fallback;
		},

		formatTelemetry(key, decimals = 0) {
			const rawValue = this.state[key];
			const n = Number(rawValue ?? 0);
			const value = Number.isFinite(n) ? n.toFixed(decimals) : String(rawValue ?? 0);
			const units = this.unitsFor(key, false);
			return units ? `${value} ${units}` : value;
		},

		async _fetchState() {
			try {
				const res = await fetch('/api/state');
				if (!res.ok) return;
				const data = await res.json();
				this.state = data;
				if (this._isStatePayloadValid(data)) {
					this.lastValidStateAt = Date.now();
					this._recordHistory(data); // this will trigger updateChart via _history watcher in next microtask switch
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
				pumpSpeed: data.pumpSpeed ?? 0,
				pumpCurrent: data.pumpCurrent ?? 0,
				pumpVoltage: data.pumpVoltage ?? 0,
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

		onSpeedSetPointChanged() {
			this._postConfig({ speedSetPoint: this.state.speedSetPoint });
		},

		onTemperatureSetPointChanged() {
			this._postConfig({ temperatureSetPoint: this.state.temperatureSetPoint });
		},

		onCalibrationVolumeChanged() {
			this._postConfig({ calibrationVolume: this.state.calibrationVolume });
		},

		startSpeed() {
			this._postMode('speed');
		},

		startTemperature() {
			this._postMode('temperature');
		},

		startCalibration() {
			this._postMode('calibration');
		},

		startFlowTest() {
			this._postMode('flowtest');
		},

		// ---- Chart ----
		_initChart() {
			const chartOptions = {
				type: 'line',
				data: { labels: [], datasets: [] }, // init empty and populate via _history watcher
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
			};
			// create objects for charts.js and don't store them in state, to avoid reactivity overhead and chart re-creation on every update
			const tCtx = this.$refs.temperatureChart?.getContext('2d');
			if (!tCtx) return console.error('Temperature chart context not found');
			const tChart = new Chart(tCtx, chartOptions);
			// flow
			const fCtx = this.$refs.flowChart?.getContext('2d');
			if (!fCtx) return console.error('Flow chart context not found');
			const fChart = new Chart(fCtx, chartOptions);
			// misc graphs
			const mCtx = this.$refs.miscChart?.getContext('2d');
			if (!mCtx) return console.error('Misc chart context not found');
			const mChart = new Chart(mCtx, chartOptions);


			// setup watch on _history to update chart when new data comes in
			this.$watch('_history', (history) => {
				// this will run every time _history changes, which happens on every new state fetch that has valid data. We extract the relevant arrays for the chart and update it.
				const labels = history.map(h =>
					new Date(h.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
				);

				const tDatasets = [
					// { label: 'Temp Delta (°C)',    data: history.map(h => h.temperatureDelta),          borderColor: '#06b6d4', tension: 0.3, yAxisID: 'y' },
					{ label: 'Out Temp (°C)',       data: history.map(h => h.outTemperature),           borderColor: '#f97316', tension: 0.3, yAxisID: 'y' },
					{ label: 'Return Temp (°C)',    data: history.map(h => h.returnTemperature),        borderColor: '#f43f5e', tension: 0.3, yAxisID: 'y' },
					{ label: 'Cooling Temp (°C)',   data: history.map(h => h.coolingTemperature),       borderColor: '#0ea5e9', tension: 0.3, yAxisID: 'y' },
				];
				const fDatasets = [
					{ label: 'Flow (L/min)',       data: history.map(h => h.flow),                      borderColor: '#60a5fa', tension: 0.3, yAxisID: 'y' },
					// { label: 'Flow Pulses/s',       data: history.map(h => h.flowPulsesFilteredPerSec), borderColor: '#34d399', tension: 0.3, yAxisID: 'y2' },
				];
				const mDatasets = [
					{ label: 'Cooling Power (W)',   data: history.map(h => h.coolingPower),             borderColor: '#22c55e', tension: 0.3, yAxisID: 'y' },
					{ label: 'Pump Speed',          data: history.map(h => h.pumpSpeed),                borderColor: '#a78bfa', tension: 0.3, yAxisID: 'y' },
					{ label: 'Current (mA)',        data: history.map(h => h.pumpCurrent),              borderColor: '#f43f5e', tension: 0.3, yAxisID: 'y' },
				];

				tChart.data.labels = labels;
				tChart.data.datasets = tDatasets;
				tChart.update('none');
				fChart.data.labels = labels;
				fChart.data.datasets = fDatasets;
				fChart.update('none');
				mChart.data.labels = labels;
				mChart.data.datasets = mDatasets;
				mChart.update('none');
			}/*, { deep: true }*/); 
			// we don't need deep watch because we replace the _history array entirely on each update, so shallow watch is sufficient.
			// if we ever just push to the array without replacing it, we would need deep watch.
		},
	};
}
