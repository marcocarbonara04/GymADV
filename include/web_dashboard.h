#pragma once

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>GymTracker Dashboard</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap');

        :root {
            --bg-base: #09090b;
            --bg-card: rgba(24, 24, 27, 0.6);
            --border-color: rgba(255, 255, 255, 0.08);
            --text-main: #f8fafc;
            --text-muted: #a1a1aa;
            
            --accent-glow: rgba(56, 189, 248, 0.15);
            --color-primary: #38bdf8;
            --color-good: #22c55e;
            --color-ok: #eab308;
            --color-bad: #ef4444;
        }

        * { box-sizing: border-box; }

        body {
            margin: 0;
            padding: 0;
            font-family: 'Outfit', sans-serif;
            background-color: var(--bg-base);
            color: var(--text-main);
            min-height: 100vh;
            background-image: 
                radial-gradient(circle at 15% 50%, var(--accent-glow), transparent 25%),
                radial-gradient(circle at 85% 30%, rgba(34, 197, 94, 0.08), transparent 25%);
            background-attachment: fixed;
            padding-bottom: 3rem;
        }

        .navbar {
            background: rgba(9, 9, 11, 0.8);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border-bottom: 1px solid var(--border-color);
            position: sticky;
            top: 0;
            z-index: 10;
            padding: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .navbar h1 {
            margin: 0;
            font-size: 1.5rem;
            font-weight: 800;
            background: linear-gradient(135deg, #7dd3fc, #0ea5e9);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status-dot {
            width: 10px; height: 10px;
            background: var(--color-good);
            border-radius: 50%;
            box-shadow: 0 0 10px var(--color-good);
            animation: pulse 2s infinite;
        }

        .container {
            max-width: 900px;
            margin: 0 auto;
            padding: 1.5rem 1rem;
            animation: fadeUp 0.6s cubic-bezier(0.16, 1, 0.3, 1);
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
            gap: 1rem;
            margin-bottom: 1.5rem;
        }

        .card {
            background: var(--bg-card);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--border-color);
            border-radius: 1.25rem;
            padding: 1.5rem;
            transition: transform 0.3s ease;
        }

        .card-header {
            font-size: 0.85rem;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 1.5px;
            margin-bottom: 0.5rem;
            font-weight: 600;
        }

        .card-value {
            font-size: 2.5rem;
            font-weight: 800;
            display: flex;
            align-items: baseline;
            gap: 0.25rem;
        }

        .card-unit { font-size: 1rem; color: var(--text-muted); font-weight: 400;}

        /* CSS Bar Chart for Form Analysis */
        .form-chart-container { margin-top: 1rem; }
        .form-bar {
            height: 8px;
            border-radius: 4px;
            display: flex;
            overflow: hidden;
            background: #27272a;
            margin-bottom: 0.5rem;
        }
        .form-segment { transition: width 1s ease-in-out; height: 100%; }
        .seg-good { background: var(--color-good); }
        .seg-ok { background: var(--color-ok); }
        .seg-bad { background: var(--color-bad); }

        .legend {
            display: flex; justify-content: space-between;
            font-size: 0.75rem; color: var(--text-muted);
        }

        /* History Table */
        .history-card { margin-top: 1.5rem; padding: 0; overflow: hidden; }
        .history-card .card-header { padding: 1.5rem 1.5rem 0 1.5rem; }
        
        .table-wrap { overflow-x: auto; padding: 0 0.5rem; }
        table { width: 100%; border-collapse: separate; border-spacing: 0 0.5rem; }
        th {
            color: var(--text-muted);
            font-weight: 600;
            font-size: 0.8rem;
            text-transform: uppercase;
            letter-spacing: 1px;
            text-align: left;
            padding: 0.5rem 1rem;
        }
        td {
            background: rgba(255,255,255,0.02);
            padding: 1rem;
        }
        tr td:first-child { border-radius: 0.75rem 0 0 0.75rem; }
        tr td:last-child { border-radius: 0 0.75rem 0.75rem 0; }
        
        .badge {
            padding: 0.35rem 0.75rem;
            border-radius: 2rem;
            font-size: 0.75rem;
            font-weight: 600;
            display: inline-block;
        }
        .badge-exercise { background: rgba(56, 189, 248, 0.15); color: var(--color-primary); border: 1px solid rgba(56, 189, 248, 0.3); }
        .badge-good { background: rgba(34, 197, 94, 0.15); color: var(--color-good); border: 1px solid rgba(34, 197, 94, 0.3); }
        .badge-ok { background: rgba(234, 179, 8, 0.15); color: var(--color-ok); border: 1px solid rgba(234, 179, 8, 0.3); }
        .badge-bad { background: rgba(239, 68, 68, 0.15); color: var(--color-bad); border: 1px solid rgba(239, 68, 68, 0.3); }

        .empty-state { text-align: center; padding: 3rem 1rem; color: var(--text-muted); }

        @keyframes fadeUp {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }
        @keyframes pulse {
            0% { box-shadow: 0 0 0 0 rgba(34, 197, 94, 0.4); }
            70% { box-shadow: 0 0 0 10px rgba(34, 197, 94, 0); }
            100% { box-shadow: 0 0 0 0 rgba(34, 197, 94, 0); }
        }

        .refresh-btn {
            background: var(--color-primary);
            color: #000;
            border: none;
            padding: 0.5rem 1rem;
            border-radius: 2rem;
            font-weight: 600;
            font-family: inherit;
            cursor: pointer;
            transition: opacity 0.2s;
        }
        .refresh-btn:active { opacity: 0.8; }

        /* Tabs and Cardio Analytics View Styling */
        .tabs {
            display: flex;
            gap: 0.5rem;
            margin-bottom: 1.5rem;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 0.5rem;
        }
        .tab-btn {
            background: transparent;
            color: var(--text-muted);
            border: none;
            padding: 0.75rem 1.25rem;
            border-radius: 0.75rem;
            font-weight: 600;
            font-size: 0.95rem;
            font-family: inherit;
            cursor: pointer;
            display: flex;
            align-items: center;
            transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
        }
        .tab-btn:hover {
            color: var(--text-main);
            background: rgba(255, 255, 255, 0.04);
        }
        .tab-btn.active {
            color: var(--color-primary);
            background: rgba(56, 189, 248, 0.1);
            box-shadow: inset 0 0 0 1px rgba(56, 189, 248, 0.2);
        }
        
        .tab-content {
            display: none;
            animation: fadeUp 0.5s cubic-bezier(0.16, 1, 0.3, 1);
        }
        .tab-content.active {
            display: block;
        }
    </style>
</head>
<body>
    <div class="navbar">
        <h1>GymTracker ADV</h1>
        <div style="display: flex; align-items: center; gap: 1rem;">
            <button class="refresh-btn" onclick="fetchData()">Sync Data</button>
            <div class="status-dot" title="Device Connected"></div>
        </div>
    </div>

    <div class="container">
        <!-- Modern iOS-style Tab Bar -->
        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('dashboard')">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.5rem;"><rect x="3" y="3" width="7" height="7"></rect><rect x="14" y="3" width="7" height="7"></rect><rect x="14" y="14" width="7" height="7"></rect><rect x="3" y="14" width="7" height="7"></rect></svg>
                Dashboard
            </button>
            <button class="tab-btn" onclick="switchTab('cardio')">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.5rem;"><path d="M22 12h-4l-3 9L9 3l-3 9H2"></path></svg>
                Cardio Analytics
            </button>
            <button class="tab-btn" onclick="switchTab('wifi')">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#22c55e" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.5rem;"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20" stroke-width="3"></line></svg>
                Wi-Fi Setup
            </button>
        </div>

        <!-- Dashboard Tab Content -->
        <div id="tab-dashboard" class="tab-content active">
            <div class="grid">
                <div class="card">
                    <div class="card-header">Total Volume</div>
                    <div class="card-value" id="val-volume">0<span class="card-unit">kg</span></div>
                </div>
                <div class="card">
                    <div class="card-header">Total Sets</div>
                    <div class="card-value" id="val-sets">0</div>
                </div>
                <div class="card">
                    <div class="card-header">Heart Rate</div>
                    <div class="card-value" id="val-heartrate" style="color: var(--text-muted);">--<span class="card-unit">bpm</span></div>
                    <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 0.5rem; display: flex; align-items: center; gap: 0.35rem;">
                        <span id="hr-status-dot" style="width: 8px; height: 8px; border-radius: 50%; background: var(--color-bad); display: inline-block; transition: background-color 0.3s, box-shadow 0.3s;"></span>
                        <span id="hr-status-text">Disconnected</span>
                    </div>
                </div>
                <div class="card">
                    <div class="card-header">Execution Quality</div>
                    <div class="card-value" id="val-quality">0<span class="card-unit">%</span></div>
                    <div class="form-chart-container">
                        <div class="form-bar">
                            <div class="form-segment seg-good" id="bar-good" style="width: 0%"></div>
                            <div class="form-segment seg-ok" id="bar-ok" style="width: 0%"></div>
                            <div class="form-segment seg-bad" id="bar-bad" style="width: 0%"></div>
                        </div>
                        <div class="legend">
                            <span style="color:var(--color-good)">Perfect</span>
                            <span style="color:var(--color-ok)">Ok</span>
                            <span style="color:var(--color-bad)">Fast/Bad</span>
                        </div>
                    </div>
                </div>
            </div>

            <div class="card history-card">
                <div class="card-header">Session Diary</div>
                <div class="table-wrap">
                    <table>
                        <thead>
                            <tr>
                                <th>Time</th>
                                <th>Exercise</th>
                                <th>Weight</th>
                                <th>Reps</th>
                                <th>VBT Profile</th>
                                <th>HR (Avg/Max)</th>
                                <th>Volume</th>
                                <th>Form Quality</th>
                            </tr>
                        </thead>
                        <tbody id="diary-body">
                            <tr><td colspan="7" class="empty-state">No workout data found. Start lifting!</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div> <!-- Close tab-dashboard -->

        <!-- Heart Rate Cardio Analytics Tab -->
        <div id="tab-cardio" class="tab-content">
            <div class="grid">
                <div class="card">
                    <div class="card-header">Average Session HR</div>
                    <div class="card-value" id="val-cardio-avg" style="color: #ef4444;">0<span class="card-unit">bpm</span></div>
                </div>
                <div class="card">
                    <div class="card-header">Peak Session HR</div>
                    <div class="card-value" id="val-cardio-max" style="color: #ef4444;">0<span class="card-unit">bpm</span></div>
                </div>
                <div class="card">
                    <div class="card-header">Active Sets with HR</div>
                    <div class="card-value" id="val-cardio-sets">0</div>
                </div>
            </div>
            
            <div class="card" style="margin-top: 1.5rem;">
                <div class="card-header" style="display: flex; justify-content: space-between; align-items: center; width:100%; flex-wrap: wrap; gap: 0.5rem;">
                    <span>Interactive Heart Rate Detail</span>
                    <select id="cardio-set-select" onchange="renderInteractiveChart()" style="background: #18181b; color: #fff; border: 1px solid var(--border-color); padding: 0.35rem 0.75rem; border-radius: 0.5rem; font-family: inherit; font-size: 0.85rem; outline: none; cursor: pointer;">
                        <option value="">No sets loaded</option>
                    </select>
                </div>
                
                <div style="padding: 1.5rem; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 250px; position: relative;">
                    <div id="cardio-chart-container" style="width: 100%; position: relative;">
                        <div class="empty-state" id="cardio-chart-empty">Select a set from the dropdown to view the heart rate curve.</div>
                    </div>
                </div>
            </div>
            
            <div class="card" style="margin-top: 1.5rem;">
                <div class="card-header">Session Heart Rate Progression (Avg/Max per Set)</div>
                <div style="padding: 1.5rem; overflow-x: auto;">
                    <div id="cardio-progression-chart" style="width: 100%; min-width: 500px; height: 180px;">
                        <!-- SVG progression timeline -->
                    </div>
                </div>
            </div>
        </div>

        <!-- Wi-Fi Setup Tab -->
        <div id="tab-wifi" class="tab-content">
            <div class="card" style="max-width: 500px; margin: 0 auto; background: var(--bg-card); backdrop-filter: blur(16px); border: 1px solid var(--border-color); border-radius: 1.25rem; padding: 1.5rem;">
                <div class="card-header" style="color: var(--color-good); display: flex; align-items: center; gap: 0.5rem; font-size: 0.95rem; font-weight: 700; text-transform: none; letter-spacing: 0.5px;">
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20" stroke-width="3"></line></svg>
                    <span>Connect GymTracker to Home Wi-Fi</span>
                </div>
                <p style="font-size: 0.85rem; color: var(--text-muted); margin: 0.75rem 0 1.25rem 0; line-height: 1.5;">
                    By connecting Cardputer to your home Wi-Fi, the dashboard will load instantly and your phone/PC will maintain full internet access during use!
                </p>
                <div style="display: flex; flex-direction: column; gap: 1.25rem;">
                    <div>
                        <label style="display: block; font-size: 0.75rem; font-weight: 700; color: var(--text-muted); text-transform: uppercase; margin-bottom: 0.45rem; letter-spacing: 0.5px;">Wi-Fi Network Name (SSID)</label>
                        <input type="text" id="wifi-ssid" placeholder="Enter network SSID" style="width: 100%; padding: 0.75rem; border-radius: 0.5rem; background: #18181b; border: 1px solid var(--border-color); color: #fff; font-family: inherit; font-size: 0.9rem; outline: none; box-sizing: border-box; transition: border-color 0.2s;">
                    </div>
                    <div>
                        <label style="display: block; font-size: 0.75rem; font-weight: 700; color: var(--text-muted); text-transform: uppercase; margin-bottom: 0.45rem; letter-spacing: 0.5px;">Wi-Fi Password</label>
                        <input type="password" id="wifi-pass" placeholder="Enter network password" style="width: 100%; padding: 0.75rem; border-radius: 0.5rem; background: #18181b; border: 1px solid var(--border-color); color: #fff; font-family: inherit; font-size: 0.9rem; outline: none; box-sizing: border-box; transition: border-color 0.2s;">
                    </div>
                    <button onclick="saveWiFiConfig()" style="background: var(--color-good); color: #09090b; font-weight: 700; border: none; padding: 0.85rem; border-radius: 0.75rem; font-family: inherit; font-size: 0.95rem; cursor: pointer; transition: transform 0.2s, background-color 0.2s; display: flex; align-items: center; justify-content: center; gap: 0.5rem; box-shadow: 0 4px 12px rgba(34, 197, 94, 0.2);">
                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path><polyline points="17 21 17 13 7 13 7 21"></polyline><polyline points="7 3 7 8 15 8"></polyline></svg>
                        Save & Connect
                    </button>
                    <div id="wifi-msg" style="text-align: center; font-size: 0.85rem; font-weight: 600; min-height: 1.25rem; margin-top: 0.25rem;"></div>
                </div>
            </div>
        </div>
    </div> <!-- Close container -->

    <script>
        let globalSessionData = [];

        function formatTime(timestamp) {
            if (timestamp < 1000000000) {
                const mins = Math.floor(timestamp / 60000);
                const secs = Math.floor((timestamp % 60000) / 1000);
                return `${mins}m ${secs}s`;
            }
            return new Date(timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        }

        function generateSparkline(hrSeries) {
            if (!hrSeries || hrSeries.length < 2) return '';
            const maxVal = Math.max(...hrSeries);
            const minVal = Math.min(...hrSeries);
            const range = maxVal - minVal || 1;
            const width = 80;
            const height = 18;
            const padding = 1.5;
            const points = hrSeries.map((val, idx) => {
                const x = (idx / (hrSeries.length - 1)) * (width - 2 * padding) + padding;
                const y = height - ((val - minVal) / range) * (height - 2 * padding) - padding;
                return `${x},${y}`;
            }).join(' ');
            return `
                <div style="display:flex; align-items:center; justify-content:center; margin-top:0.25rem;">
                    <svg width="${width}" height="${height}">
                        <polyline fill="none" stroke="#ef4444" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" points="${points}" />
                    </svg>
                </div>
            `;
        }

        function switchTab(tabId) {
            document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));
            
            if (tabId === 'dashboard') {
                document.querySelectorAll('.tab-btn')[0].classList.add('active');
                document.getElementById('tab-dashboard').classList.add('active');
            } else if (tabId === 'cardio') {
                document.querySelectorAll('.tab-btn')[1].classList.add('active');
                document.getElementById('tab-cardio').classList.add('active');
                renderInteractiveChart();
                if (globalSessionData.length > 0) {
                    renderProgressionChart(globalSessionData);
                }
            } else if (tabId === 'wifi') {
                document.querySelectorAll('.tab-btn')[2].classList.add('active');
                document.getElementById('tab-wifi').classList.add('active');
                loadWiFiConfig();
            }
        }

        async function loadWiFiConfig() {
            try {
                const res = await fetch('/wifi_get');
                const data = await res.json();
                if (data.ssid) {
                    document.getElementById('wifi-ssid').value = data.ssid;
                    if (data.has_pass) {
                        document.getElementById('wifi-pass').placeholder = "•••••••••••• (Saved)";
                    }
                }
            } catch (e) {
                console.error("Failed to load Wi-Fi config", e);
            }
        }

        async function saveWiFiConfig() {
            const ssid = document.getElementById('wifi-ssid').value.trim();
            const pass = document.getElementById('wifi-pass').value;
            const msgEl = document.getElementById('wifi-msg');
            
            if (!ssid) {
                msgEl.style.color = 'var(--color-bad)';
                msgEl.innerText = "Error: SSID cannot be empty.";
                return;
            }
            
            msgEl.style.color = '#38bdf8';
            msgEl.innerText = "Saving configuration...";
            
            try {
                const res = await fetch('/wifi_save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
                });
                
                if (res.ok) {
                    msgEl.style.color = 'var(--color-good)';
                    msgEl.innerHTML = "Save completed!<br/><span style='font-size:0.8rem;color:var(--text-muted);'>The Cardputer is rebooting to connect. Check the LCD screen for the new IP!</span>";
                } else {
                    msgEl.style.color = 'var(--color-bad)';
                    msgEl.innerText = "Error during saving.";
                }
            } catch (e) {
                msgEl.style.color = 'var(--color-bad)';
                msgEl.innerText = "Connection lost or network error.";
            }
        }

        function renderInteractiveChart() {
            const select = document.getElementById('cardio-set-select');
            const idx = select.value;
            const container = document.getElementById('cardio-chart-container');
            
            if (idx === "" || !globalSessionData[idx]) {
                container.innerHTML = '<div class="empty-state">Select a set from the dropdown to view the heart rate curve.</div>';
                return;
            }
            
            const set = globalSessionData[idx];
            const hrActive = set.hr_series || [];
            const hrRecovery = set.hr_rec_series || [];
            
            const totalLen = hrActive.length + hrRecovery.length;
            if (totalLen < 2) {
                container.innerHTML = '<div class="empty-state">Not enough heart rate data collected for this set (requires at least 2 readings).</div>';
                return;
            }
            
            const width = container.clientWidth || 800;
            const height = 260;
            const paddingLeft = 40;
            const paddingRight = 20;
            const paddingTop = 25;
            const paddingBottom = 30;
            
            const combinedSeries = [...hrActive, ...hrRecovery];
            const maxVal = Math.max(...combinedSeries);
            const minVal = Math.min(...combinedSeries);
            const yMax = Math.ceil(maxVal + 5);
            const yMin = Math.floor(Math.max(40, minVal - 5));
            const yRange = yMax - yMin || 1;
            
            const points = [];
            // Active points
            hrActive.forEach((val, i) => {
                const x = paddingLeft + (i / (totalLen - 1)) * (width - paddingLeft - paddingRight);
                const y = paddingTop + (1 - (val - yMin) / yRange) * (height - paddingTop - paddingBottom);
                points.push({ x, y, val, second: i, recovery: false });
            });
            // Recovery points
            hrRecovery.forEach((val, i) => {
                const idxInAll = hrActive.length + i;
                const x = paddingLeft + (idxInAll / (totalLen - 1)) * (width - paddingLeft - paddingRight);
                const y = paddingTop + (1 - (val - yMin) / yRange) * (height - paddingTop - paddingBottom);
                points.push({ x, y, val, second: i, recovery: true });
            });
            
            let activeLinePointsStr = '';
            let activeAreaPointsStr = '';
            let recoveryLinePointsStr = '';
            let recoveryAreaPointsStr = '';
            
            if (hrActive.length > 0) {
                const activePts = points.slice(0, hrActive.length);
                activeLinePointsStr = activePts.map(p => `${p.x},${p.y}`).join(' ');
                activeAreaPointsStr = `${activePts[0].x},${height - paddingBottom} ` + 
                                      activeLinePointsStr + 
                                      ` ${activePts[activePts.length - 1].x},${height - paddingBottom}`;
            }
            
            if (hrRecovery.length > 0) {
                const startIdx = Math.max(0, hrActive.length - 1);
                const recoveryPts = points.slice(startIdx);
                recoveryLinePointsStr = recoveryPts.map(p => `${p.x},${p.y}`).join(' ');
                recoveryAreaPointsStr = `${recoveryPts[0].x},${height - paddingBottom} ` + 
                                        recoveryLinePointsStr + 
                                        ` ${recoveryPts[recoveryPts.length - 1].x},${height - paddingBottom}`;
            }
            
            let yGridLines = '';
            const yTicksCount = 4;
            for (let i = 0; i <= yTicksCount; i++) {
                const tickVal = Math.round(yMin + (i / yTicksCount) * yRange);
                const y = paddingTop + (1 - (tickVal - yMin) / yRange) * (height - paddingTop - paddingBottom);
                yGridLines += `
                    <line x1="${paddingLeft}" y1="${y}" x2="${width - paddingRight}" y2="${y}" stroke="rgba(255,255,255,0.05)" stroke-dasharray="3,3" />
                    <text x="${paddingLeft - 8}" y="${y + 4}" fill="var(--text-muted)" font-size="10" font-weight="600" text-anchor="end">${tickVal}</text>
                `;
            }
            
            let xGridLines = '';
            const step = Math.max(1, Math.ceil(totalLen / 8));
            for (let i = 0; i < totalLen; i += step) {
                const p = points[i];
                xGridLines += `
                    <line x1="${p.x}" y1="${paddingTop}" x2="${p.x}" y2="${height - paddingBottom}" stroke="rgba(255,255,255,0.03)" />
                    <text x="${p.x}" y="${height - paddingBottom + 16}" fill="var(--text-muted)" font-size="10" font-weight="600" text-anchor="middle">${p.second}s${p.recovery ? ' (rec)' : ''}</text>
                `;
            }
            
            let verticalSplitHtml = '';
            if (hrActive.length > 0 && hrRecovery.length > 0) {
                const splitPt = points[hrActive.length - 1];
                verticalSplitHtml = `
                    <line x1="${splitPt.x}" y1="${paddingTop}" x2="${splitPt.x}" y2="${height - paddingBottom}" stroke="rgba(255,255,255,0.2)" stroke-dasharray="4,4" stroke-width="1.5" />
                    <text x="${splitPt.x + 6}" y="${paddingTop + 6}" fill="var(--text-muted)" font-size="9" font-weight="700" text-anchor="start">RECOVERY START</text>
                `;
            }
            
            let hoverZones = '';
            points.forEach((p) => {
                const color = p.recovery ? '#38bdf8' : '#ef4444';
                const idTag = `dot-${idx}-${p.recovery ? 'rec-' : 'act-'}${p.second}`;
                hoverZones += `
                    <circle cx="${p.x}" cy="${p.y}" r="4.5" fill="${color}" stroke="#fff" stroke-width="1.5" style="opacity:0; transition: opacity 0.15s;" id="${idTag}" />
                    <rect x="${p.x - (width - paddingLeft - paddingRight) / (totalLen * 2)}" y="${paddingTop}" width="${(width - paddingLeft - paddingRight) / totalLen}" height="${height - paddingTop - paddingBottom}" fill="transparent" style="cursor:pointer;" 
                        onmouseover="showChartTooltip(${p.x}, ${p.y}, ${p.val}, ${p.second}, ${p.recovery}, ${idx})" 
                        onmouseout="hideChartTooltip(${p.second}, ${p.recovery}, ${idx})" />
                `;
            });
            
            container.innerHTML = `
                <svg width="100%" height="${height}" viewBox="0 0 ${width} ${height}" style="overflow: visible;">
                    <defs>
                        <linearGradient id="hr-gradient-active" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="0%" stop-color="#ef4444" stop-opacity="0.25"/>
                            <stop offset="100%" stop-color="#ef4444" stop-opacity="0.0"/>
                        </linearGradient>
                        <linearGradient id="hr-gradient-recovery" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="0%" stop-color="#38bdf8" stop-opacity="0.25"/>
                            <stop offset="100%" stop-color="#38bdf8" stop-opacity="0.0"/>
                        </linearGradient>
                    </defs>
                    <line x1="${paddingLeft}" y1="${height - paddingBottom}" x2="${width - paddingRight}" y2="${height - paddingBottom}" stroke="var(--border-color)" stroke-width="1" />
                    ${yGridLines}
                    ${xGridLines}
                    
                    ${hrActive.length > 0 ? `<polygon points="${activeAreaPointsStr}" fill="url(#hr-gradient-active)" />` : ''}
                    ${hrRecovery.length > 0 ? `<polygon points="${recoveryAreaPointsStr}" fill="url(#hr-gradient-recovery)" />` : ''}
                    
                    ${hrActive.length > 0 ? `<polyline fill="none" stroke="#ef4444" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" points="${activeLinePointsStr}" />` : ''}
                    ${hrRecovery.length > 0 ? `<polyline fill="none" stroke="#38bdf8" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" points="${recoveryLinePointsStr}" />` : ''}
                    
                    ${verticalSplitHtml}
                    ${hoverZones}
                </svg>
                <div id="chart-tooltip" style="position: absolute; background: rgba(9,9,11,0.95); border: 1px solid var(--border-color); padding: 0.5rem; border-radius: 0.5rem; font-size: 0.75rem; color: #fff; font-weight: 600; pointer-events: none; opacity: 0; transition: opacity 0.15s; z-index: 100; box-shadow: 0 4px 12px rgba(0,0,0,0.5);"></div>
            `;
        }
        
        function showChartTooltip(x, y, val, sec, isRecovery, setIdx) {
            const tooltip = document.getElementById('chart-tooltip');
            const dot = document.getElementById(`dot-${setIdx}-${isRecovery ? 'rec-' : 'act-'}${sec}`);
            if (dot) dot.style.opacity = '1';
            
            tooltip.style.opacity = '1';
            tooltip.style.left = `${x - 40}px`;
            tooltip.style.top = `${y - 45}px`;
            
            const phase = isRecovery ? '<span style="color:#38bdf8; font-weight:800;">Recovery</span>' : '<span style="color:#ef4444; font-weight:800;">Active Set</span>';
            tooltip.innerHTML = `<span style="color:#fff; font-weight:800; font-size:0.95rem;">${val}</span> bpm<br/>${phase}<br/><span style="color:var(--text-muted)">Time: ${sec}s</span>`;
        }
        
        function hideChartTooltip(sec, isRecovery, setIdx) {
            const tooltip = document.getElementById('chart-tooltip');
            tooltip.style.opacity = '0';
            const dot = document.getElementById(`dot-${setIdx}-${isRecovery ? 'rec-' : 'act-'}${sec}`);
            if (dot) dot.style.opacity = '0';
        }

        function renderProgressionChart(data) {
            const container = document.getElementById('cardio-progression-chart');
            const setsWithHr = data.filter(s => s.hr_avg > 0);
            
            if (setsWithHr.length === 0) {
                container.innerHTML = '<div class="empty-state">No heart rate logs found in this session. Completed sets with Mi Band connected will be plotted here.</div>';
                return;
            }
            
            const width = Math.max(500, container.clientWidth || 800);
            const height = 180;
            const paddingLeft = 40;
            const paddingRight = 20;
            const paddingTop = 20;
            const paddingBottom = 40;
            
            const maxVal = Math.max(...setsWithHr.map(s => s.hr_max || 0));
            const yMax = Math.ceil(maxVal + 10);
            const yMin = 40;
            const yRange = yMax - yMin;
            
            const barWidth = Math.max(25, ((width - paddingLeft - paddingRight) / setsWithHr.length) * 0.4);
            const gap = ((width - paddingLeft - paddingRight) / setsWithHr.length) * 0.6;
            
            let barsHtml = '';
            let yGridLines = '';
            
            const yTicksCount = 3;
            for (let i = 0; i <= yTicksCount; i++) {
                const tickVal = Math.round(yMin + (i / yTicksCount) * yRange);
                const y = paddingTop + (1 - (tickVal - yMin) / yRange) * (height - paddingTop - paddingBottom);
                yGridLines += `
                    <line x1="${paddingLeft}" y1="${y}" x2="${width - paddingRight}" y2="${y}" stroke="rgba(255,255,255,0.04)" stroke-dasharray="3,3" />
                    <text x="${paddingLeft - 8}" y="${y + 4}" fill="var(--text-muted)" font-size="10" font-weight="600" text-anchor="end">${tickVal}</text>
                `;
            }
            
            setsWithHr.forEach((set, idx) => {
                const x = paddingLeft + idx * (barWidth + gap) + gap/2;
                const yAvg = paddingTop + (1 - ((set.hr_avg || 0) - yMin) / yRange) * (height - paddingTop - paddingBottom);
                const yMax = paddingTop + (1 - ((set.hr_max || 0) - yMin) / yRange) * (height - paddingTop - paddingBottom);
                const yZero = height - paddingBottom;
                const exName = (set.ex || "").substring(0, 7);
                
                barsHtml += `
                    <line x1="${x + barWidth/2}" y1="${yMax}" x2="${x + barWidth/2}" y2="${yZero}" stroke="rgba(239, 68, 68, 0.35)" stroke-width="1.5" />
                    <circle cx="${x + barWidth/2}" cy="${yMax}" r="4.5" fill="#ef4444" stroke="#09090b" stroke-width="1" />
                    
                    <rect x="${x}" y="${yAvg}" width="${barWidth}" height="${yZero - yAvg}" fill="rgba(56, 189, 248, 0.4)" rx="3" stroke="rgba(56, 189, 248, 0.6)" stroke-width="1" />
                    <text x="${x + barWidth/2}" y="${yAvg - 6}" fill="#38bdf8" font-size="9" font-weight="800" text-anchor="middle">${set.hr_avg}</text>
                    
                    <text x="${x + barWidth/2}" y="${height - paddingBottom + 16}" fill="var(--text-main)" font-size="9" font-weight="600" text-anchor="middle">S${set.s || (idx+1)}</text>
                    <text x="${x + barWidth/2}" y="${height - paddingBottom + 28}" fill="var(--text-muted)" font-size="8" font-weight="400" text-anchor="middle">${exName}</text>
                `;
            });
            
            container.innerHTML = `
                <svg width="${width}" height="${height}" style="overflow: visible;">
                    <line x1="${paddingLeft}" y1="${height - paddingBottom}" x2="${width - paddingRight}" y2="${height - paddingBottom}" stroke="var(--border-color)" stroke-width="1" />
                    ${yGridLines}
                    ${barsHtml}
                </svg>
            `;
        }

        async function fetchData() {
            try {
                const res = await fetch('/data');
                const data = await res.json();
                
                if (!data || data.length === 0) return;

                let tSets = data.length;
                let tReps = 0;
                let tVolume = 0;
                let badReps = 0;
                
                let html = '';

                [...data].reverse().forEach((set) => {
                    let reps = set.r || set.reps || 0;
                    let weight = set.w || set.weight || 0;
                    let volume = set.v || set.volume || 0;
                    let poorForm = set.pf || set.poor_form_reps || 0;
                    let exerciseName = set.ex || set.exercise || "Bench Press";
                    let ts = set.t || set.timestamp || 0;
                    let hrAvg = set.hr_avg || 0;
                    let hrMax = set.hr_max || 0;
                    let hrSeries = set.hr_series || [];
                    let repVelList = set.rep_vel || [];
                    
                    tReps += reps;
                    tVolume += volume;
                    badReps += poorForm;
                    
                    let badgeClass = 'badge-good';
                    let badgeText = 'Perfect';
                    
                    if (poorForm > 0) {
                        if (poorForm === reps) {
                            badgeClass = 'badge-bad'; badgeText = 'Too Fast';
                        } else {
                            badgeClass = 'badge-ok'; badgeText = 'Mixed';
                        }
                    }

                    let sparkline = generateSparkline(hrSeries);
                    let hrStr = hrAvg > 0 ? `<strong style="color:#ef4444">${hrAvg}</strong> <span style="font-size:0.8rem;color:var(--text-muted)">/ ${hrMax}</span>${sparkline}` : '<span style="color:var(--text-muted)">--</span>';

                    let vbtStr = '<span style="color:var(--text-muted)">--</span>';
                    if (repVelList.length > 0) {
                        let avgVel = (repVelList.reduce((a,b)=>a+b, 0) / repVelList.length).toFixed(2);
                        let titleText = repVelList.map((v, i) => `Rep ${i+1}: ${v.toFixed(2)} m/s`).join('\\n');
                        vbtStr = `<span style="border-bottom: 1px dotted var(--text-muted); cursor: help;" title="${titleText}">Ø <strong>${avgVel}</strong> m/s</span>`;
                    }

                    html += `
                        <tr>
                            <td style="color:var(--text-muted)">${formatTime(ts)}</td>
                            <td><span class="badge badge-exercise">${exerciseName}</span></td>
                            <td><strong style="color:#fff">${weight}</strong> kg</td>
                            <td><strong style="color:#fff">${reps}</strong></td>
                            <td>${vbtStr}</td>
                            <td>${hrStr}</td>
                            <td style="color:var(--color-primary)">${volume} kg</td>
                            <td><span class="badge ${badgeClass}">${badgeText}</span></td>
                        </tr>
                    `;
                });

                document.getElementById('diary-body').innerHTML = html;
                
                document.getElementById('val-volume').innerHTML = `${tVolume}<span class="card-unit">kg</span>`;
                document.getElementById('val-sets').innerHTML = tSets;
                
                let goodReps = tReps - badReps;
                let qualityPct = tReps > 0 ? Math.round((goodReps / tReps) * 100) : 0;
                document.getElementById('val-quality').innerHTML = `${qualityPct}<span class="card-unit">%</span>`;
                
                let goodW = tReps > 0 ? (goodReps/tReps)*100 : 0;
                let badW = tReps > 0 ? (badReps/tReps)*100 : 0;
                
                document.getElementById('bar-good').style.width = `${goodW}%`;
                document.getElementById('bar-ok').style.width = `0%`;
                document.getElementById('bar-bad').style.width = `${badW}%`;

                // Process and populate Cardio Analytics tab
                globalSessionData = data;
                
                let selectHtml = '<option value="">Select a set...</option>';
                let setsWithHrCount = 0;
                let sumAvgHr = 0;
                let absMaxHr = 0;
                
                data.forEach((set, idx) => {
                    let hrAvg = set.hr_avg || 0;
                    let hrMax = set.hr_max || 0;
                    let exerciseName = set.ex || set.exercise || "Bench Press";
                    let setNum = set.s || 1;
                    
                    if (hrAvg > 0) {
                        let recStr = '';
                        if (set.hr_rec_avg) {
                            recStr = ` | Rec: ${set.hr_rec_avg}/${set.hr_rec_max}`;
                        }
                        selectHtml += `<option value="${idx}">Set ${setNum} - ${exerciseName} (Active: ${hrAvg}/${hrMax}${recStr} bpm)</option>`;
                        setsWithHrCount++;
                        sumAvgHr += hrAvg;
                        if (hrMax > absMaxHr) absMaxHr = hrMax;
                    }
                });
                
                document.getElementById('cardio-set-select').innerHTML = selectHtml;
                
                let sessionAvgHr = setsWithHrCount > 0 ? Math.round(sumAvgHr / setsWithHrCount) : 0;
                document.getElementById('val-cardio-avg').innerHTML = sessionAvgHr > 0 ? `${sessionAvgHr}<span class="card-unit">bpm</span>` : `--<span class="card-unit">bpm</span>`;
                document.getElementById('val-cardio-max').innerHTML = absMaxHr > 0 ? `${absMaxHr}<span class="card-unit">bpm</span>` : `--<span class="card-unit">bpm</span>`;
                document.getElementById('val-cardio-sets').innerHTML = setsWithHrCount;
                
                const currentSelectVal = document.getElementById('cardio-set-select').value;
                if (currentSelectVal !== "") {
                    renderInteractiveChart();
                } else if (setsWithHrCount > 0) {
                    const firstSetIndex = data.findIndex(s => s.hr_avg > 0);
                    if (firstSetIndex !== -1) {
                        document.getElementById('cardio-set-select').value = firstSetIndex;
                        renderInteractiveChart();
                    }
                }
                
                renderProgressionChart(data);

            } catch (e) {
                console.error("Error syncing", e);
            }
        }

        async function fetchHeartRate() {
            try {
                const res = await fetch('/heartrate');
                const data = await res.json();
                
                const valEl = document.getElementById('val-heartrate');
                const dotEl = document.getElementById('hr-status-dot');
                const textEl = document.getElementById('hr-status-text');
                
                if (data.connected && data.hr > 0) {
                    valEl.innerHTML = `${data.hr}<span class="card-unit">bpm</span>`;
                    valEl.style.color = '#ef4444';
                    dotEl.style.backgroundColor = 'var(--color-good)';
                    dotEl.style.boxShadow = '0 0 8px var(--color-good)';
                    textEl.innerText = 'Connected';
                } else {
                    valEl.innerHTML = `--<span class="card-unit">bpm</span>`;
                    valEl.style.color = 'var(--text-muted)';
                    dotEl.style.backgroundColor = 'var(--color-bad)';
                    dotEl.style.boxShadow = 'none';
                    textEl.innerText = data.connected ? 'Searching...' : 'Disconnected';
                }
            } catch (e) {
                console.error("Error fetching HR", e);
            }
        }

        fetchData();
        setInterval(fetchData, 5000); // Sync every 5s
        
        fetchHeartRate();
        setInterval(fetchHeartRate, 1500); // Sync HR every 1.5s
        
        // Sync time with the device on load
        fetch('/time?ts=' + Date.now(), {method: 'POST'}).catch(console.error);
    </script>
</body>
</html>
)rawliteral";
