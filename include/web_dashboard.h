#pragma once

const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>GymTracker ADV - Dashboard</title>
    <style>
        /* System fonts - no external dependencies, works offline in AP mode */

        :root {
            --bg-base: #030712;
            --bg-card: rgba(17, 24, 39, 0.45);
            --bg-card-hover: rgba(31, 41, 55, 0.6);
            --border-color: rgba(255, 255, 255, 0.06);
            --border-focus: rgba(6, 182, 212, 0.4);
            --text-main: #f3f4f6;
            --text-muted: #9ca3af;
            
            --accent-glow: rgba(6, 182, 212, 0.12);
            --color-primary: #06b6d4;
            --color-primary-glow: rgba(6, 182, 212, 0.25);
            --color-good: #10b981;
            --color-good-glow: rgba(16, 185, 129, 0.2);
            --color-ok: #f59e0b;
            --color-ok-glow: rgba(245, 158, 11, 0.2);
            --color-bad: #ef4444;
            --color-bad-glow: rgba(239, 68, 68, 0.2);
            
            --transition-smooth: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background-color: var(--bg-base);
            color: var(--text-main);
            min-height: 100vh;
            background-image: 
                radial-gradient(circle at 10% 20%, var(--accent-glow), transparent 35%),
                radial-gradient(circle at 90% 80%, rgba(16, 185, 129, 0.06), transparent 35%),
                radial-gradient(circle at 50% 50%, rgba(239, 68, 68, 0.03), transparent 45%);
            background-attachment: fixed;
            padding-bottom: 4rem;
            overflow-y: scroll;
        }

        /* Scrollbar styling */
        ::-webkit-scrollbar {
            width: 8px;
            height: 8px;
        }
        ::-webkit-scrollbar-track {
            background: rgba(0, 0, 0, 0.2);
        }
        ::-webkit-scrollbar-thumb {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 4px;
        }
        ::-webkit-scrollbar-thumb:hover {
            background: rgba(255, 255, 255, 0.2);
        }

        .navbar {
            background: rgba(3, 7, 18, 0.7);
            backdrop-filter: blur(20px) saturate(180%);
            -webkit-backdrop-filter: blur(20px) saturate(180%);
            border-bottom: 1px solid var(--border-color);
            position: sticky;
            top: 0;
            z-index: 100;
            padding: 0.85rem 2rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.4);
        }

        .brand {
            display: flex;
            align-items: center;
            gap: 0.65rem;
        }

        .brand-logo {
            width: 28px;
            height: 28px;
            background: linear-gradient(135deg, var(--color-primary), #3b82f6);
            border-radius: 0.5rem;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #000;
            font-weight: 800;
            font-size: 0.95rem;
            box-shadow: 0 0 15px rgba(6, 182, 212, 0.4);
        }

        .navbar h1 {
            font-size: 1.25rem;
            font-weight: 800;
            font-family: 'Segoe UI', Roboto, 'Courier New', monospace;
            background: linear-gradient(135deg, #a5f3fc, var(--color-primary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: -0.5px;
        }

        .nav-controls {
            display: flex;
            align-items: center;
            gap: 1.25rem;
        }

        .status-badge {
            background: rgba(16, 185, 129, 0.08);
            border: 1px solid rgba(16, 185, 129, 0.2);
            padding: 0.35rem 0.75rem;
            border-radius: 2rem;
            font-size: 0.8rem;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 0.45rem;
            transition: var(--transition-smooth);
        }

        .status-badge.disconnected {
            background: rgba(239, 68, 68, 0.08);
            border: 1px solid rgba(239, 68, 68, 0.2);
        }

        .status-dot {
            width: 8px;
            height: 8px;
            background: var(--color-good);
            border-radius: 50%;
            box-shadow: 0 0 8px var(--color-good);
        }

        .status-badge.disconnected .status-dot {
            background: var(--color-bad);
            box-shadow: 0 0 8px var(--color-bad);
            animation: pulse-red 2.0s infinite;
        }

        .refresh-btn {
            background: linear-gradient(135deg, var(--color-primary), #0284c7);
            color: #ffffff;
            border: none;
            padding: 0.45rem 1.1rem;
            border-radius: 2rem;
            font-weight: 600;
            font-family: inherit;
            font-size: 0.85rem;
            cursor: pointer;
            transition: var(--transition-smooth);
            display: flex;
            align-items: center;
            gap: 0.35rem;
            box-shadow: 0 4px 12px rgba(6, 182, 212, 0.25);
        }

        .refresh-btn:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 16px rgba(6, 182, 212, 0.4);
            filter: brightness(1.1);
        }

        .refresh-btn:active {
            transform: translateY(0);
        }

        .container {
            max-width: 1000px;
            margin: 0 auto;
            padding: 1.5rem 1.5rem;
            animation: fadeUp 0.6s cubic-bezier(0.16, 1, 0.3, 1);
        }

        /* Modern iOS-style Tab Bar */
        .tabs {
            display: flex;
            background: rgba(17, 24, 39, 0.3);
            border: 1px solid var(--border-color);
            padding: 0.25rem;
            border-radius: 0.85rem;
            margin-bottom: 2rem;
            gap: 0.25rem;
            width: max-content;
        }

        .tab-btn {
            background: transparent;
            color: var(--text-muted);
            border: none;
            padding: 0.6rem 1.25rem;
            border-radius: 0.65rem;
            font-weight: 600;
            font-size: 0.9rem;
            font-family: inherit;
            cursor: pointer;
            display: flex;
            align-items: center;
            transition: var(--transition-smooth);
        }

        .tab-btn svg {
            transition: var(--transition-smooth);
        }

        .tab-btn:hover {
            color: var(--text-main);
            background: rgba(255, 255, 255, 0.03);
        }

        .tab-btn.active {
            color: #000000;
            background: #ffffff;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
        }

        .tab-btn.active svg {
            stroke: #000000 !important;
        }

        .tab-content {
            display: none;
            animation: fadeUp 0.5s cubic-bezier(0.16, 1, 0.3, 1);
        }

        .tab-content.active {
            display: block;
        }

        /* Cards Grid */
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 1.25rem;
            margin-bottom: 1.5rem;
        }

        .card {
            background: var(--bg-card);
            backdrop-filter: blur(24px) saturate(180%);
            -webkit-backdrop-filter: blur(24px) saturate(180%);
            border: 1px solid var(--border-color);
            border-radius: 1.25rem;
            padding: 1.5rem;
            transition: var(--transition-smooth);
            position: relative;
            overflow: hidden;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
        }

        .card::before {
            content: '';
            position: absolute;
            top: 0; left: 0; right: 0; height: 1px;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.08), transparent);
        }

        .card:hover {
            transform: translateY(-2px);
            border-color: rgba(255, 255, 255, 0.12);
            background: var(--bg-card-hover);
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.3);
        }

        .card-header {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 1.5px;
            margin-bottom: 0.75rem;
            font-weight: 700;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .card-value {
            font-size: 2.5rem;
            font-weight: 700;
            font-family: 'Segoe UI', Roboto, 'Courier New', monospace;
            display: flex;
            align-items: baseline;
            gap: 0.2rem;
            line-height: 1.1;
        }

        .card-unit {
            font-size: 1.1rem;
            color: var(--text-muted);
            font-weight: 500;
            font-family: 'Outfit', sans-serif;
        }

        /* Heart Rate Card Specific */
        @keyframes heartPulse {
            0% { transform: scale(1); }
            14% { transform: scale(1.15); }
            28% { transform: scale(1); }
            42% { transform: scale(1.15); }
            70% { transform: scale(1); }
            100% { transform: scale(1); }
        }

        .pulsing-heart {
            animation: heartPulse var(--pulse-speed, 1s) infinite;
            transform-origin: center;
        }

        /* Quality Form Bar */
        .form-chart-container {
            margin-top: 1rem;
        }

        .form-bar {
            height: 6px;
            border-radius: 10px;
            display: flex;
            overflow: hidden;
            background: rgba(255, 255, 255, 0.05);
            margin-bottom: 0.65rem;
            border: 1px solid rgba(255,255,255,0.02);
        }

        .form-segment {
            transition: width 0.8s cubic-bezier(0.4, 0, 0.2, 1);
            height: 100%;
        }

        .seg-good { background: var(--color-good); box-shadow: 0 0 10px var(--color-good-glow); }
        .seg-ok { background: var(--color-ok); box-shadow: 0 0 10px var(--color-ok-glow); }
        .seg-bad { background: var(--color-bad); box-shadow: 0 0 10px var(--color-bad-glow); }

        .legend {
            display: flex;
            justify-content: space-between;
            font-size: 0.75rem;
            font-weight: 600;
        }

        /* Device Control Center Card */
        .control-grid {
            display: flex;
            flex-direction: column;
            gap: 0.85rem;
            margin-top: 0.5rem;
        }

        .control-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            background: rgba(0, 0, 0, 0.15);
            padding: 0.55rem 0.75rem;
            border-radius: 0.75rem;
            border: 1px solid rgba(255,255,255,0.02);
        }

        .control-label {
            font-size: 0.8rem;
            font-weight: 600;
            color: var(--text-muted);
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        /* Battery Visualization */
        .battery-container {
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .battery-bar-wrap {
            width: 32px;
            height: 14px;
            border: 1.5px solid var(--text-muted);
            border-radius: 3px;
            padding: 1px;
            position: relative;
            display: flex;
        }

        .battery-bar-wrap::after {
            content: '';
            width: 2px;
            height: 6px;
            background: var(--text-muted);
            position: absolute;
            right: -3px;
            top: 2.5px;
            border-radius: 0 1px 1px 0;
        }

        .battery-bar-fill {
            height: 100%;
            background: var(--color-good);
            border-radius: 1px;
            transition: width 0.5s ease;
        }

        .battery-text {
            font-family: 'Segoe UI', Roboto, 'Courier New', monospace;
            font-size: 0.85rem;
            font-weight: 700;
        }

        /* iOS Toggle Switch */
        .switch {
            position: relative;
            display: inline-block;
            width: 38px;
            height: 20px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0; left: 0; right: 0; bottom: 0;
            background-color: #374151;
            transition: .3s;
            border-radius: 20px;
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 16px;
            width: 16px;
            left: 2px;
            bottom: 2px;
            background-color: white;
            transition: .3s;
            border-radius: 50%;
            box-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }

        input:checked + .slider {
            background-color: var(--color-good);
            box-shadow: 0 0 8px rgba(16, 185, 129, 0.4);
        }

        input:checked + .slider:before {
            transform: translateX(18px);
        }

        /* History Session Card */
        .history-card {
            margin-top: 1.5rem;
            padding: 0;
            overflow: hidden;
        }

        .history-card .card-header {
            padding: 1.5rem 1.5rem 0.5rem 1.5rem;
            font-size: 0.95rem;
            font-family: 'Segoe UI', Roboto, 'Courier New', monospace;
            text-transform: none;
            letter-spacing: 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .export-btn {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--border-color);
            color: var(--text-main);
            padding: 0.35rem 0.85rem;
            border-radius: 0.5rem;
            font-size: 0.75rem;
            font-weight: 600;
            cursor: pointer;
            transition: var(--transition-smooth);
            display: flex;
            align-items: center;
            gap: 0.35rem;
        }

        .export-btn:hover {
            background: rgba(255, 255, 255, 0.1);
            border-color: rgba(255,255,255,0.2);
        }

        .table-wrap {
            overflow-x: auto;
            padding: 0 1.25rem 1.25rem 1.25rem;
        }

        table {
            width: 100%;
            border-collapse: separate;
            border-spacing: 0 0.5rem;
            min-width: 750px;
        }

        th {
            color: var(--text-muted);
            font-weight: 700;
            font-size: 0.75rem;
            text-transform: uppercase;
            letter-spacing: 1px;
            text-align: left;
            padding: 0.5rem 1rem;
            border-bottom: 1px solid rgba(255,255,255,0.03);
        }

        td {
            background: rgba(255, 255, 255, 0.015);
            padding: 0.95rem 1rem;
            font-size: 0.9rem;
            border-top: 1px solid rgba(255,255,255,0.01);
            border-bottom: 1px solid rgba(255,255,255,0.01);
            transition: var(--transition-smooth);
        }

        tr:hover td {
            background: rgba(255, 255, 255, 0.04);
            border-color: rgba(255, 255, 255, 0.04);
        }

        tr td:first-child {
            border-left: 1px solid rgba(255,255,255,0.01);
            border-radius: 0.75rem 0 0 0.75rem;
        }
        tr td:last-child {
            border-right: 1px solid rgba(255,255,255,0.01);
            border-radius: 0 0.75rem 0.75rem 0;
        }

        /* Badges */
        .badge {
            padding: 0.3rem 0.7rem;
            border-radius: 2rem;
            font-size: 0.72rem;
            font-weight: 700;
            display: inline-flex;
            align-items: center;
            gap: 0.25rem;
        }

        .badge-exercise {
            background: rgba(6, 182, 212, 0.08);
            color: var(--color-primary);
            border: 1px solid rgba(6, 182, 212, 0.2);
            box-shadow: 0 2px 8px rgba(6, 182, 212, 0.05);
        }

        .badge-good {
            background: rgba(16, 185, 129, 0.08);
            color: var(--color-good);
            border: 1px solid rgba(16, 185, 129, 0.2);
        }

        .badge-ok {
            background: rgba(245, 158, 11, 0.08);
            color: var(--color-ok);
            border: 1px solid rgba(245, 158, 11, 0.2);
        }

        .badge-bad {
            background: rgba(239, 68, 68, 0.08);
            color: var(--color-bad);
            border: 1px solid rgba(239, 68, 68, 0.2);
        }

        .empty-state {
            text-align: center;
            padding: 3.5rem 1.5rem;
            color: var(--text-muted);
            font-size: 0.95rem;
            font-weight: 500;
        }

        /* Wi-Fi Setup Layout */
        .wifi-form-card {
            max-width: 480px;
            margin: 0 auto;
        }

        .form-group {
            margin-bottom: 1.25rem;
        }

        .form-group label {
            display: block;
            font-size: 0.75rem;
            font-weight: 700;
            color: var(--text-muted);
            text-transform: uppercase;
            margin-bottom: 0.45rem;
            letter-spacing: 0.5px;
        }

        .form-input {
            width: 100%;
            padding: 0.75rem 1rem;
            border-radius: 0.75rem;
            background: rgba(0, 0, 0, 0.25);
            border: 1px solid var(--border-color);
            color: #ffffff;
            font-family: inherit;
            font-size: 0.9rem;
            outline: none;
            transition: var(--transition-smooth);
        }

        .form-input:focus {
            border-color: var(--color-primary);
            box-shadow: 0 0 0 3px var(--color-primary-glow);
            background: rgba(0, 0, 0, 0.4);
        }

        .submit-btn {
            background: linear-gradient(135deg, var(--color-good), #059669);
            color: #030712;
            font-weight: 700;
            border: none;
            padding: 0.85rem;
            width: 100%;
            border-radius: 0.75rem;
            font-family: inherit;
            font-size: 0.95rem;
            cursor: pointer;
            transition: var(--transition-smooth);
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.5rem;
            box-shadow: 0 4px 15px rgba(16, 185, 129, 0.25);
        }

        .submit-btn:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 20px rgba(16, 185, 129, 0.45);
            filter: brightness(1.05);
        }

        .submit-btn:active {
            transform: translateY(0);
        }

        /* Toast Notifications */
        .toast-container {
            position: fixed;
            bottom: 2rem;
            right: 2rem;
            z-index: 1000;
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
            max-width: 350px;
            width: calc(100% - 4rem);
        }

        .toast {
            background: rgba(17, 24, 39, 0.95);
            backdrop-filter: blur(8px);
            border: 1px solid var(--border-color);
            color: var(--text-main);
            padding: 0.85rem 1.1rem;
            border-radius: 0.85rem;
            box-shadow: 0 10px 25px rgba(0,0,0,0.5);
            display: flex;
            align-items: center;
            gap: 0.65rem;
            font-size: 0.85rem;
            font-weight: 600;
            transform: translateY(50px);
            opacity: 0;
            animation: toastIn 0.35s cubic-bezier(0.16, 1, 0.3, 1) forwards;
            transition: var(--transition-smooth);
        }

        @keyframes toastIn {
            to { transform: translateY(0); opacity: 1; }
        }

        /* Keyframes */
        @keyframes fadeUp {
            from { opacity: 0; transform: translateY(15px); }
            to { opacity: 1; transform: translateY(0); }
        }

        @keyframes pulse-red {
            0% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.5); }
            70% { box-shadow: 0 0 0 8px rgba(239, 68, 68, 0); }
            100% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); }
        }
    </style>
</head>
<body>
    <div class="navbar">
        <div class="brand">
            <div class="brand-logo">G</div>
            <h1>GymTracker ADV</h1>
        </div>
        <div class="nav-controls">
            <div class="status-badge" id="device-status-badge">
                <div class="status-dot"></div>
                <span id="device-status-text">Connected</span>
            </div>
            <button class="refresh-btn" onclick="triggerManualSync()">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67"/></svg>
                Sync
            </button>
        </div>
    </div>

    <div class="container">
        <!-- Modern iOS-style Segmented Tab Control -->
        <div class="tabs">
            <button class="tab-btn active" id="btn-tab-dashboard" onclick="switchTab('dashboard')">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#6b7280" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.45rem;"><rect x="3" y="3" width="7" height="7"></rect><rect x="14" y="3" width="7" height="7"></rect><rect x="14" y="14" width="7" height="7"></rect><rect x="3" y="14" width="7" height="7"></rect></svg>
                Dashboard
            </button>
            <button class="tab-btn" id="btn-tab-cardio" onclick="switchTab('cardio')">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.45rem;"><path d="M22 12h-4l-3 9L9 3l-3 9H2"></path></svg>
                Cardio Analytics
            </button>
            <button class="tab-btn" id="btn-tab-wifi" onclick="switchTab('wifi')">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#10b981" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="margin-right: 0.45rem;"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20" stroke-width="3"></line></svg>
                Wi-Fi Setup
            </button>
        </div>

        <!-- Dashboard Tab Content -->
        <div id="tab-dashboard" class="tab-content active">
            <div class="grid">
                <!-- Volume Card -->
                <div class="card">
                    <div class="card-header">
                        <span>Total Volume</span>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--color-primary)" stroke-width="2.5"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
                    </div>
                    <div class="card-value" id="val-volume">0<span class="card-unit">kg</span></div>
                </div>
                <!-- Sets Card -->
                <div class="card">
                    <div class="card-header">
                        <span>Total Sets</span>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--color-good)" stroke-width="2.5"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path><polyline points="22 4 12 14.01 9 11.01"></polyline></svg>
                    </div>
                    <div class="card-value" id="val-sets">0</div>
                </div>
                <!-- Heart Rate Card -->
                <div class="card">
                    <div class="card-header">
                        <span>Heart Rate</span>
                        <svg class="pulsing-heart" id="heart-svg" width="14" height="14" viewBox="0 0 24 24" fill="#ef4444"><path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/></svg>
                    </div>
                    <div class="card-value" id="val-heartrate" style="color: var(--text-muted);">--<span class="card-unit">bpm</span></div>
                    <div style="font-size: 0.72rem; color: var(--text-muted); margin-top: 0.45rem; display: flex; align-items: center; justify-content: space-between;">
                        <span id="hr-status-text">Sensor disconnected</span>
                        <span id="ping-text" style="font-family: 'Segoe UI', Roboto, monospace;">Ping: --</span>
                    </div>
                </div>
                <!-- Execution Quality Card -->
                <div class="card">
                    <div class="card-header">
                        <span>Execution Form</span>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--color-ok)" stroke-width="2.5"><circle cx="12" cy="12" r="10"></circle><path d="M8 14s1.5 2 4 2 4-2 4-2M9 9h.01M15 9h.01"/></svg>
                    </div>
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

            <!-- Device Control Center Panel -->
            <div class="grid" style="grid-template-columns: 1fr;">
                <div class="card">
                    <div class="card-header" style="font-size:0.8rem; font-family: 'Segoe UI', Roboto, monospace; text-transform:none; letter-spacing:0; font-weight:700; color:var(--text-main);">
                        <span>Device Control Panel (Cardputer Hardware)</span>
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--text-muted)" stroke-width="2"><rect x="2" y="3" width="20" height="14" rx="2" ry="2"></rect><line x1="8" y1="21" x2="16" y2="21"></line><line x1="12" y1="17" x2="12" y2="21"></line></svg>
                    </div>
                    
                    <div class="control-grid" style="grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); display:grid; gap:1rem;">
                        <!-- Battery telemetry -->
                        <div class="control-row">
                            <div class="control-label">
                                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><rect x="1" y="6" width="18" height="12" rx="2" ry="2"></rect><line x1="23" y1="11" x2="23" y2="13"></line></svg>
                                <span>Cardputer Battery</span>
                            </div>
                            <div class="battery-container">
                                <div class="battery-bar-wrap">
                                    <div class="battery-bar-fill" id="ctrl-batt-fill" style="width: 0%;"></div>
                                </div>
                                <span class="battery-text" id="ctrl-batt-text">--%</span>
                            </div>
                        </div>
                        <!-- Speaker mute toggle -->
                        <div class="control-row">
                            <div class="control-label">
                                <svg id="ctrl-speaker-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"></polygon><path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"></path></svg>
                                <span id="ctrl-sound-label">Speaker Audio</span>
                            </div>
                            <label class="switch">
                                <input type="checkbox" id="ctrl-mute-switch" onchange="toggleMuteState()">
                                <span class="slider"></span>
                            </label>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Session Diary -->
            <div class="card history-card">
                <div class="card-header">
                    <span>Session Diary</span>
                    <button class="export-btn" onclick="location.href='/export'">
                        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>
                        Export CSV
                    </button>
                </div>
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
                            <tr><td colspan="8" class="empty-state">No workout data found. Start lifting!</td></tr>
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
                    <div class="card-value" id="val-cardio-avg" style="color: #ef4444;">--<span class="card-unit">bpm</span></div>
                </div>
                <div class="card">
                    <div class="card-header">Peak Session HR</div>
                    <div class="card-value" id="val-cardio-max" style="color: #ef4444;">--<span class="card-unit">bpm</span></div>
                </div>
                <div class="card">
                    <div class="card-header">Sets with HR logs</div>
                    <div class="card-value" id="val-cardio-sets">0</div>
                </div>
            </div>
            
            <div class="card" style="margin-top: 1.5rem;">
                <div class="card-header" style="display: flex; justify-content: space-between; align-items: center; width:100%; flex-wrap: wrap; gap: 0.5rem;">
                    <span style="font-family: 'Segoe UI', Roboto, monospace; font-size: 0.95rem; text-transform:none; font-weight:700; color:var(--text-main);">Interactive Set Curve</span>
                    <select id="cardio-set-select" onchange="renderInteractiveChart()" style="background: #111827; color: #fff; border: 1px solid var(--border-color); padding: 0.45rem 1rem; border-radius: 0.65rem; font-family: inherit; font-size: 0.85rem; outline: none; cursor: pointer; transition: var(--transition-smooth);">
                        <option value="">No sets loaded</option>
                    </select>
                </div>
                
                <div style="padding: 1rem 0; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 270px; position: relative;">
                    <div id="cardio-chart-container" style="width: 100%; position: relative;">
                        <div class="empty-state" id="cardio-chart-empty">Select a set from the dropdown to view the heart rate curve.</div>
                    </div>
                </div>
            </div>
            
            <div class="card" style="margin-top: 1.5rem;">
                <div class="card-header" style="font-family: 'Segoe UI', Roboto, monospace; font-size: 0.95rem; text-transform:none; font-weight:700; color:var(--text-main);">Session Heart Rate Progression (Avg/Max per Set)</div>
                <div style="padding: 1rem 0; overflow-x: auto;">
                    <div id="cardio-progression-chart" style="width: 100%; min-width: 500px; height: 180px;">
                        <div class="empty-state">No heart rate progression available.</div>
                    </div>
                </div>
            </div>
        </div>

        <!-- Wi-Fi Setup Tab -->
        <div id="tab-wifi" class="tab-content">
            <div class="card wifi-form-card">
                <div class="card-header" style="color: var(--color-primary); display: flex; align-items: center; gap: 0.5rem; font-size: 1rem; font-weight: 700; text-transform: none; letter-spacing: 0;">
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20" stroke-width="3"></line></svg>
                    <span>Wi-Fi Network Setup</span>
                </div>
                <p style="font-size: 0.85rem; color: var(--text-muted); margin: 0.75rem 0 1.5rem 0; line-height: 1.6;">
                    Connect GymTracker to your local network. Once saved, the M5Cardputer will automatically boot in client mode, permitting simultaneous phone data logging and full internet connectivity.
                </p>
                <div style="display: flex; flex-direction: column; gap: 1rem;">
                    <div class="form-group">
                        <label>Network SSID</label>
                        <input type="text" id="wifi-ssid" class="form-input" placeholder="Enter network name">
                    </div>
                    <div class="form-group">
                        <label>Password</label>
                        <input type="password" id="wifi-pass" class="form-input" placeholder="Enter password">
                    </div>
                    <button class="submit-btn" onclick="saveWiFiConfig()">
                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path><polyline points="17 21 17 13 7 13 7 21"></polyline><polyline points="7 3 7 8 15 8"></polyline></svg>
                        Save & Apply Config
                    </button>
                    <div id="wifi-msg" style="text-align: center; font-size: 0.85rem; font-weight: 600; min-height: 1.25rem; margin-top: 0.25rem; line-height: 1.4;"></div>
                </div>
            </div>
        </div>
    </div> <!-- Close container -->

    <div class="toast-container" id="toast-box"></div>

    <script>
        let globalSessionData = [];
        let isMutedState = false;
        let lastPingTime = 0;

        function showToast(text, type = 'success') {
            const container = document.getElementById('toast-box');
            const toast = document.createElement('div');
            toast.className = 'toast';
            
            let color = 'var(--color-primary)';
            let iconSvg = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>';
            
            if (type === 'success') {
                color = 'var(--color-good)';
                iconSvg = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"></polyline></svg>';
            } else if (type === 'error') {
                color = 'var(--color-bad)';
                iconSvg = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line></svg>';
            }
            
            toast.style.borderColor = color;
            toast.innerHTML = `<span style="color:${color}; display:flex; align-items:center;">${iconSvg}</span><span>${text}</span>`;
            
            container.appendChild(toast);
            
            setTimeout(() => {
                toast.style.opacity = '0';
                toast.style.transform = 'translateY(20px)';
                setTimeout(() => toast.remove(), 300);
            }, 3500);
        }

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
            
            const selectedBtn = document.getElementById(`btn-tab-${tabId}`);
            const selectedContent = document.getElementById(`tab-${tabId}`);
            
            if (selectedBtn && selectedContent) {
                selectedBtn.classList.add('active');
                selectedContent.classList.add('active');
                
                if (tabId === 'cardio') {
                    renderInteractiveChart();
                    if (globalSessionData.length > 0) {
                        renderProgressionChart(globalSessionData);
                    }
                } else if (tabId === 'wifi') {
                    loadWiFiConfig();
                }
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
            
            msgEl.style.color = 'var(--color-primary)';
            msgEl.innerText = "Saving configuration...";
            
            try {
                const res = await fetch('/wifi_save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
                });
                
                if (res.ok) {
                    msgEl.style.color = 'var(--color-good)';
                    msgEl.innerHTML = "Save completed!<br/><span style='font-size:0.8rem;color:var(--text-muted);'>M5Cardputer is rebooting. Check display for local IP address.</span>";
                    showToast("Wi-Fi settings saved. Device rebooting...", "success");
                } else {
                    msgEl.style.color = 'var(--color-bad)';
                    msgEl.innerText = "Error during saving.";
                    showToast("Failed to save Wi-Fi configuration", "error");
                }
            } catch (e) {
                msgEl.style.color = 'var(--color-bad)';
                msgEl.innerText = "Connection lost or network error.";
                showToast("Connection lost. Device may already be rebooting.", "info");
            }
        }

        async function toggleMuteState() {
            const isChecked = document.getElementById('ctrl-mute-switch').checked;
            try {
                const res = await fetch('/mute', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: `muted=${isChecked}`
                });
                if (res.ok) {
                    const data = await res.json();
                    isMutedState = data.muted;
                    updateSpeakerUI(isMutedState);
                    showToast(isMutedState ? "Cardputer buzzer muted." : "Cardputer buzzer active.", "success");
                } else {
                    document.getElementById('ctrl-mute-switch').checked = !isChecked; // Revert
                    showToast("Failed to update speaker state", "error");
                }
            } catch (e) {
                document.getElementById('ctrl-mute-switch').checked = !isChecked; // Revert
                showToast("Error updating speaker. Try again.", "error");
            }
        }

        function updateSpeakerUI(muted) {
            const icon = document.getElementById('ctrl-speaker-icon');
            const label = document.getElementById('ctrl-sound-label');
            if (muted) {
                icon.innerHTML = '<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"></polygon><line x1="23" y1="9" x2="17" y2="15"></line><line x1="17" y1="9" x2="23" y2="15"></line>';
                icon.style.color = 'var(--color-bad)';
                label.innerHTML = 'Speaker Audio: <span style="color:var(--color-bad); font-weight:700;">MUTED</span>';
            } else {
                icon.innerHTML = '<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"></polygon><path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"></path>';
                icon.style.color = 'var(--color-good)';
                label.innerHTML = 'Speaker Audio: <span style="color:var(--color-good); font-weight:700;">ACTIVE</span>';
            }
        }

        async function triggerManualSync() {
            showToast("Syncing workouts database...", "info");
            await fetchData();
            showToast("Workouts successfully synchronized.", "success");
        }

        async function fetchData() {
            const startT = performance.now();
            try {
                const res = await fetch('/data');
                const data = await res.json();
                
                const pingTime = Math.round(performance.now() - startT);
                document.getElementById('ping-text').innerText = `Ping: ${pingTime}ms`;
                
                // Update header connection indicator
                document.getElementById('device-status-badge').className = 'status-badge';
                document.getElementById('device-status-text').innerText = 'Connected';
                
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
                            <td style="color:var(--text-muted); font-family: 'Segoe UI', Roboto, monospace;">${formatTime(ts)}</td>
                            <td><span class="badge badge-exercise">${exerciseName}</span></td>
                            <td style="font-family: 'Segoe UI', Roboto, monospace;"><strong style="color:#fff">${weight}</strong> kg</td>
                            <td style="font-family: 'Segoe UI', Roboto, monospace;"><strong style="color:#fff">${reps}</strong></td>
                            <td style="font-family: 'Segoe UI', Roboto, monospace;">${vbtStr}</td>
                            <td>${hrStr}</td>
                            <td style="color:var(--color-primary); font-family: 'Segoe UI', Roboto, monospace;">${volume} kg</td>
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
                // Update header connection indicator to disconnected
                document.getElementById('device-status-badge').className = 'status-badge disconnected';
                document.getElementById('device-status-text').innerText = 'Offline';
            }
        }

        async function fetchDeviceStatus() {
            try {
                const res = await fetch('/status');
                const data = await res.json();
                
                // 1. Heart Rate
                const valEl = document.getElementById('val-heartrate');
                const textEl = document.getElementById('hr-status-text');
                const heart = document.getElementById('heart-svg');
                
                if (data.connected && data.hr > 0) {
                    valEl.innerHTML = `${data.hr}<span class="card-unit">bpm</span>`;
                    valEl.style.color = '#ef4444';
                    textEl.innerText = 'Connected';
                    
                    // Set heart pulse speed dynamically!
                    const speed = (60 / data.hr).toFixed(2) + 's';
                    heart.style.setProperty('--pulse-speed', speed);
                } else {
                    valEl.innerHTML = `--<span class="card-unit">bpm</span>`;
                    valEl.style.color = 'var(--text-muted)';
                    textEl.innerText = data.connected ? 'Searching...' : 'Sensor disconnected';
                    heart.style.setProperty('--pulse-speed', '0s'); // static
                }

                // 2. Audio settings
                isMutedState = data.muted;
                document.getElementById('ctrl-mute-switch').checked = isMutedState;
                updateSpeakerUI(isMutedState);

                // 3. Battery telemetry
                const batt = data.battery || 0;
                document.getElementById('ctrl-batt-text').innerText = `${batt}%`;
                const fillEl = document.getElementById('ctrl-batt-fill');
                fillEl.style.width = `${batt}%`;
                if (batt > 50) {
                    fillEl.style.backgroundColor = 'var(--color-good)';
                } else if (batt > 20) {
                    fillEl.style.backgroundColor = 'var(--color-ok)';
                } else {
                    fillEl.style.backgroundColor = 'var(--color-bad)';
                }
                
                // Update header connection badge
                document.getElementById('device-status-badge').className = 'status-badge';
                document.getElementById('device-status-text').innerText = 'Connected';
                
            } catch (e) {
                console.error("Error fetching device status", e);
                // Update header connection indicator to disconnected
                document.getElementById('device-status-badge').className = 'status-badge disconnected';
                document.getElementById('device-status-text').innerText = 'Offline';
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
                const color = p.recovery ? '#06b6d4' : '#ef4444';
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
                            <stop offset="0%" stop-color="#06b6d4" stop-opacity="0.25"/>
                            <stop offset="100%" stop-color="#06b6d4" stop-opacity="0.0"/>
                        </linearGradient>
                    </defs>
                    <line x1="${paddingLeft}" y1="${height - paddingBottom}" x2="${width - paddingRight}" y2="${height - paddingBottom}" stroke="var(--border-color)" stroke-width="1" />
                    ${yGridLines}
                    ${xGridLines}
                    
                    ${hrActive.length > 0 ? `<polygon points="${activeAreaPointsStr}" fill="url(#hr-gradient-active)" />` : ''}
                     ${hrRecovery.length > 0 ? `<polygon points="${recoveryAreaPointsStr}" fill="url(#hr-gradient-recovery)" />` : ''}
                    
                    ${hrActive.length > 0 ? `<polyline fill="none" stroke="#ef4444" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" points="${activeLinePointsStr}" />` : ''}
                    ${hrRecovery.length > 0 ? `<polyline fill="none" stroke="#06b6d4" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" points="${recoveryLinePointsStr}" />` : ''}
                    
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
            
            const phase = isRecovery ? '<span style="color:#06b6d4; font-weight:800;">Recovery</span>' : '<span style="color:#ef4444; font-weight:800;">Active Set</span>';
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
                    
                    <rect x="${x}" y="${yAvg}" width="${barWidth}" height="${yZero - yAvg}" fill="rgba(6, 182, 212, 0.4)" rx="3" stroke="rgba(6, 182, 212, 0.6)" stroke-width="1" />
                    <text x="${x + barWidth/2}" y="${yAvg - 6}" fill="#22d3ee" font-size="9" font-weight="800" text-anchor="middle">${set.hr_avg}</text>
                    
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

        // Periodic update logic
        fetchData();
        setInterval(fetchData, 5000); // sync data every 5s
        
        fetchDeviceStatus();
        setInterval(fetchDeviceStatus, 1500); // sync status / heart rate every 1.5s
        
        // Sync time with the device on load
        fetch('/time?ts=' + Date.now(), {method: 'POST'}).catch(console.error);
    </script>
</body>
</html>
)rawliteral";
