// ---- Constants ----
const NORMAL_INTERVAL_MS = 5000;
const FAST_INTERVAL_MS = 1000;
const FAST_POLL_COUNT = 10;
const HISTORY_DURATION_MS = 10 * 60 * 1000; // 10 minutes

// ---- Alpine.js app ----
function coolBedApp() {
	return {
		state: {},
		speedSetPoint: 128,
		tempSetPoint: 20,
		calibrationVolume: 500,

		// Polling state
		_pollTimer: null,
		_fastPollRemaining: 0,
		_history: [], // array of { timestamp, ...metrics }
		_chart: null,

		init() {
			this._loadFromState();
			this._startPolling();
		},

		_loadFromState() {
			this.speedSetPoint = this.state.speedSetPoint ?? 128;
			this.tempSetPoint = this.state.temperatureSetPoint ?? 20;
			this.calibrationVolume = this.state.calibrationVolume ?? 500;
		},

		async _fetchState() {
			try {
				const res = await fetch('/api/state');
				if (!res.ok) return;
				const data = await res.json();
				this.state = data;
				this._loadFromState();
				this._recordHistory(data);
			} catch (e) {
				console.error('Failed to fetch state:', e);
			}
			try {
				// this._updateChart();
			} catch (e) {
				console.error('Failed to update chart:', e);
			}
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

		async startSpeed() {
			await this._postConfig({ speedSetPoint: this.speedSetPoint });
			await this._postMode('speed');
		},

		async startTemperature() {
			await this._postConfig({ temperatureSetPoint: this.tempSetPoint });
			await this._postMode('temperature');
		},

		async startCalibration() {
			await this._postConfig({ calibrationVolume: this.calibrationVolume });
			await this._postMode('calibration');
		},

		startFlowTest() {
			this._postMode('flowtest');
		},

		// ---- Chart ----
		_updateChart() {
			if (!this._history.length) return;

			const labels = this._history.map(h =>
				new Date(h.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
			);

			const datasets = [
				{ label: 'Flow (L/min)',       data: this._history.map(h => h.flow),                      borderColor: '#60a5fa', tension: 0.3, yAxisID: 'y' },
				{ label: 'Temp Delta (°C)',    data: this._history.map(h => h.temperatureDelta),          borderColor: '#06b6d4', tension: 0.3, yAxisID: 'y' },
				{ label: 'Out Temp (°C)',       data: this._history.map(h => h.outTemperature),            borderColor: '#f97316', tension: 0.3, yAxisID: 'y' },
				{ label: 'Return Temp (°C)',    data: this._history.map(h => h.returnTemperature),         borderColor: '#fb923c', tension: 0.3, yAxisID: 'y' },
				{ label: 'Cooling Power (W)',   data: this._history.map(h => h.coolingPower),              borderColor: '#22c55e', tension: 0.3, yAxisID: 'y2' },
				{ label: 'Pump Speed',          data: this._history.map(h => h.pumpSpeed),                borderColor: '#a78bfa', tension: 0.3, yAxisID: 'y2' },
				{ label: 'Current (mA)',        data: this._history.map(h => h.pumpCurrent),              borderColor: '#f43f5e', tension: 0.3, yAxisID: 'y2' },
				{ label: 'Flow Pulses/s',       data: this._history.map(h => h.flowPulsesFilteredPerSec), borderColor: '#34d399', tension: 0.3, yAxisID: 'y2' },
			];

			if (!this._chart) {
				const canvas = document.getElementById('telemetryChart');
				if (!canvas) return;
				const ctx = canvas.getContext('2d');
				if (!ctx) return;

				this._chart = new Chart(ctx, {
					type: 'line',
					data: { labels, datasets },
					options: {
						animation: false,
						responsive: true,
						interaction: { mode: 'index', intersect: false },
						plugins: {
							// Work around a legend layout crash seen in some browser/Chart.js combinations.
							legend: false,
							// legend: { labels: { color: '#e2e8f0', boxWidth: 12 } },
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
							y2: {
								type: 'linear', position: 'right',
								ticks: { color: '#64748b' },
								grid:  { drawOnChartArea: false },
							},
						},
					},
				});
			} else {
				this._chart.data.labels = labels;
				this._chart.data.datasets = datasets;
				this._chart.update('none');
			}
		},
	};
}
