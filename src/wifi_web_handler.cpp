#include "wifi_web_handler.h"
#include "config.h"
#include "logger.h"
#include "espnow_handler.h"
#include "led_handler.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define NVS_NAMESPACE "espnow_gw"
#define NVS_DRY_RUN_KEY "dry_run"

// Global instances
static AsyncWebServer server(80);
static DeviceState currentState = STATE_WIFI;
static bool dryRunMode = false;
static unsigned long wifiTimeoutExpirationMs = 0;
static Preferences prefs;

// HTML Content embedded as raw string literal
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP-NOW Transmitter Configuration</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=Fira+Code&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary: #6366f1;
            --primary-hover: #4f46e5;
            --bg-gradient: linear-gradient(135deg, #0f172a 0%, #1e293b 100%);
            --card-bg: rgba(30, 41, 59, 0.7);
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
            --border-color: rgba(255, 255, 255, 0.1);
            --success: #10b981;
            --warning: #f59e0b;
            --error: #ef4444;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', sans-serif; }
        body { background: var(--bg-gradient); color: var(--text-primary); min-height: 100vh; padding: 2rem; background-attachment: fixed; }
        .container { max-width: 900px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 1.5rem; border-bottom: 1px solid var(--border-color); margin-bottom: 2rem; }
        h1 { font-size: 1.75rem; font-weight: 700; background: linear-gradient(to right, #818cf8, #c084fc); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        .github-link { color: var(--text-secondary); text-decoration: none; display: flex; align-items: center; gap: 0.4rem; font-size: 0.8rem; transition: color 0.2s; }
        .github-link:hover { color: var(--text-primary); }
        .github-link svg { width: 18px; height: 18px; fill: currentColor; flex-shrink: 0; }
        .api-link { font-size: 0.78rem; color: #818cf8; text-decoration: none; display: inline-flex; align-items: center; gap: 0.3rem; margin-top: 0.4rem; opacity: 0.85; transition: opacity 0.2s; }
        .api-link:hover { opacity: 1; text-decoration: underline; }
        .badge { padding: 0.25rem 0.75rem; border-radius: 9999px; font-size: 0.75rem; font-weight: 600; text-transform: uppercase; }
        .badge-wifi { background: rgba(99, 102, 241, 0.2); color: #818cf8; }
        .badge-espnow { background: rgba(16, 185, 129, 0.2); color: #10b981; }
        .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 1.5rem; margin-bottom: 2rem; }
        @media(max-width: 768px) { .grid { grid-template-columns: 1fr; } }
        .card { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 1rem; padding: 1.5rem; backdrop-filter: blur(12px); box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1); }
        .card-full { grid-column: 1 / -1; }
        /* OTA collapsible */
        details.ota-section { margin-bottom: 2rem; }
        details.ota-section summary { cursor: pointer; list-style: none; display: flex; align-items: center; gap: 0.5rem; color: var(--text-secondary); font-size: 0.85rem; padding: 0.5rem 0; user-select: none; }
        details.ota-section summary::before { content: '▶'; font-size: 0.65rem; transition: transform 0.2s; }
        details[open].ota-section summary::before { transform: rotate(90deg); }
        details.ota-section summary:hover { color: var(--text-primary); }
        details.ota-section .ota-inner { margin-top: 0.75rem; }
        /* JSON send textarea */
        .json-textarea { width: 100%; background: rgba(15,23,42,0.6); border: 1px solid var(--border-color); border-radius: 0.5rem; padding: 0.75rem; font-family: 'Fira Code', monospace; font-size: 0.8rem; color: var(--text-primary); resize: vertical; min-height: 120px; outline: none; transition: border-color 0.2s; }
        .json-textarea:focus { border-color: var(--primary); }
        .json-send-row { display: flex; gap: 0.75rem; align-items: center; margin-top: 0.75rem; flex-wrap: wrap; }
        .send-status { font-size: 0.8rem; font-weight: 600; }
        .card-title { font-size: 1.1rem; font-weight: 600; margin-bottom: 1.25rem; color: #e2e8f0; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 0.5rem; }
        .stat-row { display: flex; justify-content: space-between; padding: 0.5rem 0; font-size: 0.9rem; border-bottom: 1px solid rgba(255,255,255,0.02); }
        .stat-row:last-child { border-bottom: none; }
        .stat-label { color: var(--text-secondary); }
        .stat-val { font-family: 'Fira Code', monospace; font-weight: 600; }
        .btn { background: var(--primary); color: white; border: none; padding: 0.6rem 1.2rem; border-radius: 0.5rem; font-weight: 600; cursor: pointer; transition: all 0.2s; display: inline-flex; align-items: center; gap: 0.5rem; font-size: 0.85rem; }
        .btn:hover { background: var(--primary-hover); transform: translateY(-1px); }
        .btn-warning { background: var(--warning); }
        .btn-warning:hover { background: #d97706; }
        .btn-secondary { background: rgba(255, 255, 255, 0.1); border: 1px solid var(--border-color); }
        .btn-secondary:hover { background: rgba(255, 255, 255, 0.15); }
        .btn:disabled { opacity: 0.5; cursor: not-allowed; transform: none !important; }
        .actions { display: flex; gap: 0.75rem; flex-wrap: wrap; margin-top: 1rem; }
        .log-container { height: 350px; overflow-y: auto; background: rgba(15, 23, 42, 0.6); border-radius: 0.5rem; padding: 1rem; font-family: 'Fira Code', monospace; font-size: 0.8rem; border: 1px solid var(--border-color); }
        .log-entry { padding: 0.2rem 0; border-bottom: 1px solid rgba(255, 255, 255, 0.03); display: flex; gap: 0.75rem; }
        .log-time { color: #6ee7b7; white-space: nowrap; }
        .log-msg { word-break: break-all; }
        .progress-container { width: 100%; background-color: rgba(255,255,255,0.05); border-radius: 0.5rem; overflow: hidden; margin-top: 1rem; display: none; height: 1.25rem; border: 1px solid var(--border-color); }
        .progress-bar { height: 100%; background: linear-gradient(to right, #818cf8, #c084fc); width: 0%; transition: width 0.1s; display: flex; align-items: center; justify-content: center; font-size: 0.75rem; font-weight: bold; }
        /* Toggle Switch styling */
        .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: rgba(255,255,255,0.1); transition: .3s; border-radius: 24px; border: 1px solid var(--border-color); }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--primary); }
        input:checked + .slider:before { transform: translateX(20px); }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div>
                <h1 id="header-title">ESP-NOW GW Transmitter</h1>
                <p style="color: var(--text-secondary); font-size: 0.85rem; margin-top: 0.25rem;">Boot / Maintenance Mode</p>
            </div>
            <div style="display: flex; align-items: center; gap: 1rem;">
                <div id="status-badge" class="badge badge-wifi">Wi-Fi Mode</div>
                <a class="github-link" href="https://github.com/me2d13/esp-now-gw-transmitter" target="_blank" rel="noopener" title="GitHub Repository">
                    <svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12"/></svg>
                    GitHub
                </a>
            </div>
        </header>

        <main>
            <div class="grid">
                <div class="card" style="display: flex; flex-direction: column; justify-content: space-between;">
                    <div>
                        <div class="card-title">System Status</div>
                        <div class="stat-row">
                            <span class="stat-label">Device Name</span>
                            <span class="stat-val" id="name">-</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Firmware Version</span>
                            <span class="stat-val" id="version">-</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">MAC Address</span>
                            <span class="stat-val" id="mac">-</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Uptime</span>
                            <span class="stat-val" id="uptime">-</span>
                        </div>
                        <div class="stat-row">
                            <span class="stat-label">Free Heap</span>
                            <span class="stat-val" id="heap">-</span>
                        </div>
                    </div>
                    <div style="margin-top: 1.5rem; display: flex; justify-content: space-between; align-items: center; background: rgba(255,255,255,0.02); padding: 0.75rem; border-radius: 0.5rem; border: 1px solid rgba(255,255,255,0.05);">
                        <div>
                            <div style="font-weight: 600; font-size: 0.9rem;">Stay in Wi-Fi Mode</div>
                            <div style="font-size: 0.75rem; color: var(--text-secondary);">Pause timer to remain in setup/maintenance</div>
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="staywifi-toggle">
                            <span class="slider"></span>
                        </label>
                    </div>
                </div>

                <div class="card" style="display: flex; flex-direction: column; justify-content: space-between;">
                    <div>
                        <div class="card-title">Control & Timing</div>
                        <div style="text-align: center; margin: 1.5rem 0;">
                            <div style="font-size: 0.85rem; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 0.25rem;">Switching to ESP-NOW in</div>
                            <div style="font-size: 2.25rem; font-weight: 700; font-family: 'Fira Code', monospace; color: #818cf8;" id="timer">--:--</div>
                        </div>
                    </div>
                    <div class="actions">
                        <button class="btn" id="btn-prolong">Extend Wi-Fi (3 min)</button>
                        <button class="btn btn-warning" id="btn-switch">Switch to ESP-NOW</button>
                    </div>
                </div>
            </div>
            <!-- Full-width Logs -->
            <div class="card card-full" style="display: flex; flex-direction: column; margin-bottom: 2rem;">
                <div class="card-title" style="display: flex; justify-content: space-between; align-items: center;">
                    <span>Transmitter Logs</span>
                    <button class="btn btn-secondary" id="btn-clear-logs" style="padding: 0.25rem 0.5rem; font-size: 0.75rem;">Clear</button>
                </div>
                <div class="log-container" id="log-container" style="height: 300px;">
                    <div style="color: var(--text-secondary); text-align: center; padding-top: 5rem;">Loading logs...</div>
                </div>
            </div>

            <!-- Send JSON to Gateway (visible in Wi-Fi mode for debugging) -->
            <div class="card card-full" style="margin-bottom: 2rem;">
                <div class="card-title">Send JSON to Gateway</div>
                <p style="font-size: 0.8rem; color: var(--text-secondary); margin-bottom: 0.5rem;">Paste a raw JSON message to send directly to the MQTT gateway via serial (for debugging).</p>
                <a class="api-link" href="https://github.com/me2d13/esp-now-gw-transmitter/blob/master/API.md" target="_blank" rel="noopener">&#128196; API Reference (API.md)</a>
                <textarea class="json-textarea" id="json-payload" placeholder='{"to": "AABBCCDDEEFF", "message": {"key": "value"}}'></textarea>
                <div class="json-send-row">
                    <button class="btn" id="btn-send-json">&#9658; Send</button>
                    <span class="send-status" id="send-status"></span>
                </div>
            </div>

            <!-- OTA Firmware Upload (collapsible) -->
            <details class="ota-section">
                <summary>OTA Firmware Upload</summary>
                <div class="ota-inner card">
                    <p style="font-size: 0.8rem; color: var(--text-secondary); margin-bottom: 1.25rem;">Select compiled <code>firmware.bin</code> to update transmitter.</p>
                    <div style="display: flex; flex-direction: column; gap: 0.75rem;">
                        <input type="file" id="ota-file" accept=".bin" style="font-size: 0.85rem; background: rgba(15,23,42,0.3); border: 1px solid var(--border-color); padding: 0.5rem; border-radius: 0.25rem; width: 100%; color: var(--text-secondary);">
                        <button class="btn" id="btn-upload" style="justify-content: center;" disabled>Upload &amp; Flash</button>
                        <div class="progress-container" id="prog-container">
                            <div class="progress-bar" id="prog-bar">0%</div>
                        </div>
                        <div id="upload-status" style="font-size: 0.85rem; font-weight: 600; text-align: center; margin-top: 0.25rem;"></div>
                    </div>
                </div>
            </details>
        </main>
    </div>

    <script>
        const nameEl = document.getElementById('name');
        const versionEl = document.getElementById('version');
        const macEl = document.getElementById('mac');
        const uptimeEl = document.getElementById('uptime');
        const heapEl = document.getElementById('heap');
        const timerEl = document.getElementById('timer');
        const staywifiToggle = document.getElementById('staywifi-toggle');
        const btnProlong = document.getElementById('btn-prolong');
        const btnSwitch = document.getElementById('btn-switch');
        const btnClearLogs = document.getElementById('btn-clear-logs');
        const fileInput = document.getElementById('ota-file');
        const btnUpload = document.getElementById('btn-upload');
        const progContainer = document.getElementById('prog-container');
        const progBar = document.getElementById('prog-bar');
        const uploadStatus = document.getElementById('upload-status');
        const logContainer = document.getElementById('log-container');

        let dryRun = false;
        let logsArray = [];

        function formatTime(sec) {
            if (sec < 0) return "PAUSED (Dry Run)";
            const m = Math.floor(sec / 60);
            const s = sec % 60;
            return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
        }

        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                if (!res.ok) throw new Error('Status offline');
                const data = await res.json();
                
                nameEl.textContent = data.name;
                versionEl.textContent = data.version;
                macEl.textContent = data.mac;
                uptimeEl.textContent = `${Math.floor(data.uptime / 60)}m ${data.uptime % 60}s`;
                heapEl.textContent = `${(data.free_heap / 1024).toFixed(1)} KB`;
                
                dryRun = data.dry_run;
                staywifiToggle.checked = dryRun;
                
                timerEl.textContent = formatTime(data.wifi_time_remaining_sec);
                if (data.wifi_time_remaining_sec < 0) {
                    timerEl.style.color = '#f59e0b';
                } else {
                    timerEl.style.color = '#818cf8';
                }
            } catch (err) {
                console.error(err);
                timerEl.textContent = "OFFLINE / ESP-NOW";
                timerEl.style.color = '#ef4444';
            }
        }

        async function fetchLogs() {
            try {
                const res = await fetch('/api/log');
                if (!res.ok) throw new Error('Log fetch failed');
                const logs = await res.json();
                
                if (logs.length === 0) {
                    logContainer.innerHTML = '<div style="color: var(--text-secondary); text-align: center; padding-top: 5rem;">No logs.</div>';
                    return;
                }
                
                // Show newest at the bottom and scroll to bottom
                const isAtBottom = logContainer.scrollHeight - logContainer.clientHeight <= logContainer.scrollTop + 20;
                
                logContainer.innerHTML = logs.map(log => `
                    <div class="log-entry">
                        <span class="log-time">[${log.timestamp}]</span>
                        <span class="log-msg">${escapeHtml(log.message)}</span>
                    </div>
                `).join('');
                
                if (isAtBottom) {
                    logContainer.scrollTop = logContainer.scrollHeight;
                }
            } catch (err) {
                console.error(err);
            }
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        staywifiToggle.addEventListener('change', async () => {
            const enabled = staywifiToggle.checked;
            try {
                const res = await fetch('/api/dry-run', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({enabled})
                });
                if (!res.ok) throw new Error();
                fetchStatus();
            } catch (err) {
                staywifiToggle.checked = !enabled;
                alert('Failed to update Stay in Wi-Fi Mode setting');
            }
        });

        btnProlong.addEventListener('click', async () => {
            try {
                const res = await fetch('/api/prolong', {method: 'POST'});
                if (res.ok) fetchStatus();
            } catch (err) {
                alert('Connection error');
            }
        });

        btnSwitch.addEventListener('click', async () => {
            if (confirm('Disconnect Wi-Fi and switch to ESP-NOW mode? The Web UI will be disabled.')) {
                try {
                    await fetch('/api/switch-now', {method: 'POST'});
                    alert('Switching to ESP-NOW. Web page will disconnect.');
                    setTimeout(() => window.location.reload(), 2000);
                } catch (err) {
                    alert('Connection error');
                }
            }
        });

        fileInput.addEventListener('change', () => {
            btnUpload.disabled = !fileInput.files.length;
        });

        btnUpload.addEventListener('click', () => {
            const file = fileInput.files[0];
            if (!file) return;

            btnUpload.disabled = true;
            fileInput.disabled = true;
            progContainer.style.display = 'block';
            uploadStatus.textContent = 'Uploading firmware...';
            uploadStatus.style.color = 'var(--text-secondary)';

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/update', true);

            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const pct = Math.round((e.loaded / e.total) * 100);
                    progBar.style.width = `${pct}%`;
                    progBar.textContent = `${pct}%`;
                }
            });

            xhr.onload = () => {
                if (xhr.status === 200) {
                    uploadStatus.textContent = '✓ Success! Rebooting device...';
                    uploadStatus.style.color = 'var(--success)';
                    progBar.style.backgroundColor = 'var(--success)';
                } else {
                    uploadStatus.textContent = '✗ Error: ' + xhr.responseText;
                    uploadStatus.style.color = 'var(--error)';
                    btnUpload.disabled = false;
                    fileInput.disabled = false;
                }
            };

            xhr.onerror = () => {
                uploadStatus.textContent = '✗ Network error occurred';
                uploadStatus.style.color = 'var(--error)';
                btnUpload.disabled = false;
                fileInput.disabled = false;
            };

            const formData = new FormData();
            formData.append('update', file);
            xhr.send(formData);
        });

        btnClearLogs.addEventListener('click', async () => {
            try {
                const res = await fetch('/api/clear-logs', {method: 'POST'});
                if (res.ok) fetchLogs();
            } catch (err) {
                alert('Connection error');
            }
        });

        // Send JSON to Gateway
        const btnSendJson = document.getElementById('btn-send-json');
        const jsonPayload = document.getElementById('json-payload');
        const sendStatus = document.getElementById('send-status');

        btnSendJson.addEventListener('click', async () => {
            const raw = jsonPayload.value.trim();
            if (!raw) return;
            // Validate JSON first
            let parsed;
            try {
                parsed = JSON.parse(raw);
            } catch (e) {
                sendStatus.textContent = '\u2717 Invalid JSON: ' + e.message;
                sendStatus.style.color = 'var(--error)';
                return;
            }
            btnSendJson.disabled = true;
            sendStatus.textContent = 'Sending...';
            sendStatus.style.color = 'var(--text-secondary)';
            try {
                const res = await fetch('/api/send-raw', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(parsed)
                });
                if (res.ok) {
                    sendStatus.textContent = '\u2713 Sent!';
                    sendStatus.style.color = 'var(--success)';
                } else {
                    const txt = await res.text();
                    sendStatus.textContent = '\u2717 Error: ' + txt;
                    sendStatus.style.color = 'var(--error)';
                }
            } catch (err) {
                sendStatus.textContent = '\u2717 Network error';
                sendStatus.style.color = 'var(--error)';
            }
            btnSendJson.disabled = false;
            setTimeout(() => { sendStatus.textContent = ''; }, 4000);
        });

        // Periodic updates
        fetchStatus();
        fetchLogs();
        setInterval(fetchStatus, 2000);
        setInterval(fetchLogs, 2000);
    </script>
</body>
</html>
)rawliteral";

void setupWifiWeb() {
  logPrintln("[TRANS] Starting Wi-Fi Setup Mode...");
  
  setLedPattern(LED_WIFI_MODE);
  
  // Set timeout timer
  wifiTimeoutExpirationMs = millis() + WIFI_TIME_MS;

  // Configure Wi-Fi STA mode (preserve custom MAC)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  logPrintf("[TRANS] Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);
  
  // Non-blocking connection wait (up to 15 seconds)
  unsigned long startConnect = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 15000) {
    delay(500);
    logPrint(".");
  }
  logPrintln();

  if (WiFi.status() == WL_CONNECTED) {
    logPrintln("[TRANS] Wi-Fi connected successfully!");
    logPrintf("[TRANS] IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    logPrintln("[TRANS] WARNING: Wi-Fi connection failed/timed out. Continuing in offline setup mode.");
  }
  // Setup Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getLogsJson());
  });

  server.on("/api/clear-logs", HTTP_POST, [](AsyncWebServerRequest *request) {
    clearLogBuffer();
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["state"] = (currentState == STATE_WIFI) ? "WIFI" : "ESPNOW";
    doc["dry_run"] = dryRunMode;
    doc["wifi_time_remaining_sec"] = getRemainingWifiTimeSec();
    doc["uptime"] = millis() / 1000;
    doc["mac"] = WiFi.macAddress();
    doc["peers"] = getEspNowPeerCount();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["version"] = SW_VERSION;
    doc["name"] = WHO_AM_I;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/api/prolong", HTTP_POST, [](AsyncWebServerRequest *request) {
    prolongWifiTime(WIFI_TIME_MS);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API Endpoint to Toggle Dry Run Mode
  server.on("/api/dry-run", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      if (jsonDoc["enabled"].isNull()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'enabled' parameter\"}");
        return;
      }
      
      bool enable = jsonDoc["enabled"];
      setDryRunMode(enable);
      request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.on("/api/switch-now", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"ok\"}");
    // Let response send before disconnecting
    request->onDisconnect([]() {
      transitionToEspNow();
    });
  });

  // Raw JSON send endpoint – writes the POSTed JSON body directly to UART2
  server.on("/api/send-raw", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Validate it's parseable JSON
      JsonDocument jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      // Re-serialize (compact) and write to UART2
      String out;
      serializeJson(jsonDoc, out);
      getUART2().println(out);
      logPrintf("[TRANS] Web -> UART2 raw send: %s\n", out.c_str());
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  // Web OTA Handler
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
    if (shouldReboot) {
      logPrintln("[TRANS] OTA firmware flash successful. Rebooting...");
      delay(500);
      ESP.restart();
    }
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    feedWatchdog();
    if (!index) {
      setLedPattern(LED_OTA_BLINK);
      logPrintf("[TRANS] OTA Web Update Started: %s\n", filename.c_str());
      // Shut down watchdog or feed it during update
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        logPrintf("[TRANS] OTA Web Update Completed: %u bytes\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  logPrintln("[TRANS] HTTP server started on port 80");

  // Setup ArduinoOTA (for network flashing via IDE/PlatformIO)
  ArduinoOTA.setHostname(WHO_AM_I);
  ArduinoOTA.onStart([]() {
    feedWatchdog();
    setLedPattern(LED_OTA_BLINK);
    logPrintln("[TRANS] ArduinoOTA Update Start");
  });
  ArduinoOTA.onEnd([]() {
    feedWatchdog();
    logPrintln("\n[TRANS] ArduinoOTA Update End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    feedWatchdog();
    logPrintf("[TRANS] ArduinoOTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logPrintf("[TRANS] ArduinoOTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) logPrintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) logPrintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) logPrintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) logPrintln("Receive Failed");
    else if (error == OTA_END_ERROR) logPrintln("End Failed");
  });
  ArduinoOTA.begin();
  logPrintln("[TRANS] ArduinoOTA listener started");
}

void handleWifiWeb() {
  if (currentState != STATE_WIFI) return;

  // Handle ArduinoOTA
  ArduinoOTA.handle();

  // If dry run is active, timer is disabled
  if (!dryRunMode) {
    if (millis() >= wifiTimeoutExpirationMs) {
      logPrintln("[TRANS] Wi-Fi Mode timeout expired. Auto-switching to ESP-NOW...");
      transitionToEspNow();
    }
  }
}

void stopWifiWeb() {
  logPrintln("[TRANS] Stopping Web Server & OTA...");
  server.end();
  // ArduinoOTA does not have a clean end() method on ESP32, but we stop calling handle()
}

void transitionToEspNow() {
  if (currentState == STATE_ESPNOW) return;

  stopWifiWeb();

  logPrintln("[TRANS] Resetting Wi-Fi stack for ESP-NOW...");
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // Re-apply custom MAC address (forces WIFI_STA mode inside loadCustomMacAddress)
  loadCustomMacAddress();
  delay(100);

  // Initialize ESP-NOW
  setupEspNow();
  
  setLedPattern(LED_ESPNOW_MODE);
  
  currentState = STATE_ESPNOW;
  logPrintln("[TRANS] Transited to ESP-NOW mode successfully.");
}

DeviceState getCurrentState() {
  return currentState;
}

bool isDryRunEnabled() {
  return dryRunMode;
}

void prolongWifiTime(uint32_t ms) {
  wifiTimeoutExpirationMs = millis() + ms;
  logPrintf("[TRANS] Wi-Fi Mode prolonged by %u seconds.\n", ms / 1000);
}

void setDryRunMode(bool enable) {
  dryRunMode = enable;
  logPrintf("[TRANS] Dry Run Mode set to: %s\n", dryRunMode ? "ENABLED" : "DISABLED");
}

int32_t getRemainingWifiTimeSec() {
  if (dryRunMode || currentState != STATE_WIFI) {
    return -1;
  }
  long remainingMs = (long)wifiTimeoutExpirationMs - (long)millis();
  if (remainingMs < 0) return 0;
  return remainingMs / 1000;
}
