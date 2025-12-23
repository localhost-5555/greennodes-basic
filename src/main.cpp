#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// GPIO pins
const int sensorPower = D2;
const int analogPin = A0;
const int relayPin = D3;

// Default configuration
struct Config {
  int dryValue = 1025;
  int wetValue = 555;
  int dryThreshold = 40;
  char ssid[32] = "Greennodes";
  char password[32] = "greennodes123";
  bool autoMode = false;
};

Config config;

// Create instances
ESP8266WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// Sensor values
float moisture = 72.0;
bool irrigation = false;

// Auto mode timing
unsigned long lastAutoCheck = 0;
const unsigned long autoCheckInterval = 10000;

// Forward declarations
void loadConfig();
void saveConfig();

// File system functions
void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
  loadConfig();
}

void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, configFile);
      configFile.close();
      
      if (!error) {
        config.dryValue = doc["dryValue"] | 1025;
        config.wetValue = doc["wetValue"] | 555;
        config.dryThreshold = doc["dryThreshold"] | 40;
        
        Serial.print("Config loaded - Dry Threshold: ");
        Serial.println(config.dryThreshold);
      }
    }
  } else {
    saveConfig();
  }
}

void saveConfig() {
  JsonDocument doc;
  doc["dryValue"] = config.dryValue;
  doc["wetValue"] = config.wetValue;
  doc["dryThreshold"] = config.dryThreshold;
  
  File configFile = LittleFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("Config saved");
  }
}

// Read moisture sensor
void updateMoistureSensor() {
  digitalWrite(sensorPower, HIGH);
  delay(10);
  int rawValue = analogRead(analogPin);
  digitalWrite(sensorPower, LOW);
  
  moisture = map(rawValue, config.dryValue, config.wetValue, 0, 100);
  moisture = constrain(moisture, 0, 100);
  
  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print(" | Moisture: ");
  Serial.print(moisture);
  Serial.println("%");
}

// Auto mode logic
void handleAutoMode() {
  if (!config.autoMode) return;
  
  unsigned long currentTime = millis();
  if (currentTime - lastAutoCheck >= autoCheckInterval) {
    lastAutoCheck = currentTime;
    
    Serial.print("Auto check - Moisture: ");
    Serial.print(moisture);
    Serial.print("% - Threshold: ");
    Serial.println(config.dryThreshold);
    
    if (moisture < config.dryThreshold) {
      if (!irrigation) {
        irrigation = true;
        digitalWrite(relayPin, HIGH);
        Serial.println("Auto: Irrigation ON");
      }
    } else {
      if (irrigation) {
        irrigation = false;
        digitalWrite(relayPin, LOW);
        Serial.println("Auto: Irrigation OFF");
      }
    }
  }
}

// Web server handlers
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Greennodes Dashboard</title>
    <style>
        :root {
            --main-color: #34D399; /* Global variable */
            --btn-control-highlight: #38bdf8;
        }
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #f3f4f6;
            padding: 20px;
            min-height: 100vh;
            background-color: #ecf6e8;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        .header {
            text-align: left;
            margin-bottom: 30px;
        }

        .header h1 {
            font-size: 2rem;
            color: #4b5563;
            font-weight: bold;
        }

        .nav-tabs {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            border-bottom: 2px solid #e5e7eb;
        }

        .nav-tab {
            padding: 12px 20px;
            background: none;
            border: none;
            color: #4b5563;
            font-weight: 600;
            font-size: large; 
            cursor: pointer;
            border-bottom: 3px solid transparent;
            transition: all 0.3s ease;
        }

        .nav-tab.active {
            color: var(--main-color);
            border-bottom-color: var(--main-color);
        }

        .nav-tab:hover {
            color: var(--main-color);
        }

        .tab-content {
            display: none;
        }

        .tab-content.active {
            display: block;
        }

        .section-title {
            font-size: 1.5rem;
            font-weight: bold;
            color: #4b5563;
            margin-bottom: 15px;
            margin-top: 30px;
        }

        .values-container {
            display: flex;
            gap: 15px;
            margin-bottom: 30px;
            overflow-x: auto;
            padding-bottom: 10px;
        }

        .value-card {
            background: white;
            border: 2px solid var(--main-color);
            border-radius: 8px;
            padding: 16px;
            min-width: 200px;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
            transition: all 0.3s ease;
        }

        .value-label {
            font-size: 0.875rem;
            font-weight: 500;
            color: #4b5563;
            margin-bottom: 8px;
        }

        .value-display {
            font-size: 2.5rem;
            font-weight: bold;
            color: #1f2937;
            display: flex;
            align-items: baseline;
            gap: 8px;
        }

        .value-unit {
            font-size: 0.875rem;
            color: #9ca3af;
        }

        .controls-container {
            display: flex;
            flex-direction: column;
            background: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
            gap: 10px;
        }

        .control-buttons {
            display: flex;
            flex-wrap: wrap;
        }

        .btn-mode {
            padding: 12px 20px;
            border: 1px solid var(--main-color);
            border-radius: 8px;
            background: white;
            color: #4b5563;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            font-size: 1rem;
            min-width: 150px;
            text-align: center;
        }

        .btn-mode:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(16, 185, 129, 0.3);
        }

        .btn-mode.active {
            background: #d1fae5;
            color: var(--main-color);
            border: 2px solid var(--main-color);
        }

        .grid-controls {
            display: grid;
            grid-template-columns: 1fr;
            gap: 12px;
        }

        .btn-control {
            background: white;
            color: #4b5563;
            border: 1px solid var(--btn-control-highlight);
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 12px;
            text-align: center;
            min-height: 80px;
            max-width: 150px;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s ease;
            font-weight: 600;
            font-size: 1.1rem;
        }

        .btn-control.active {
            background:#f0f9ff;
            border: 2px solid var(--btn-control-highlight);
            color: var(--btn-control-highlight);
            box-shadow: 0 4px 12px rgba(14, 165, 233, 0.3);
        }

        .btn-control:hover:not(.disabled) {
            border-width: 2px;
            transform: translateY(-2px);
        }

        .btn-control.disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }

        .control-status {
            font-size: 0.875rem;
        }

        /* Settings Form */
        .settings-form {
            background: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
            max-width: 600px;
        }

        .form-group {
            margin-bottom: 20px;
        }

        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #4b5563;
        }

        .form-group input {
            width: 100%;
            padding: 10px;
            border: 1px solid #d1d5db;
            border-radius: 6px;
            font-size: 1rem;
        }

        .form-group input:focus {
            outline: none;
            border-color: var(--main-color);
            box-shadow: 0 0 0 3px rgba(16, 185, 129, 0.1);
        }

        .form-description {
            font-size: 0.875rem;
            color: #6b7280;
            margin-top: 4px;
        }

        .btn-save {
            background: var(--main-color);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 6px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            width: 100%;
        }

        .btn-save:hover {
            background: #059669;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(16, 185, 129, 0.3);
        }

        .success-message {
            background: #d1fae5;
            color: #047857;
            padding: 12px;
            border-radius: 6px;
            margin-bottom: 20px;
            display: none;
        }

        .success-message.show {
            display: block;
        }

        @media (max-width: 768px) {
            body {
                padding: 12px;
            }

            .header h1 {
                font-size: 1.5rem;
            }

            .nav-tabs {
                flex-wrap: wrap;
            }

            .value-display {
                font-size: 2rem;
            }

            .btn-mode {
                flex: 1;
                min-width: 140px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Greennodes Dashboard 🌿</h1>
        </div>

        <!-- Navigation Tabs -->
        <div class="nav-tabs">
            <button class="nav-tab active" onclick="switchTab('dashboard')">Dashboard</button>
            <button class="nav-tab" onclick="switchTab('settings')">Settings</button>
        </div>

        <!-- Dashboard Tab -->
        <div id="dashboard" class="tab-content active">
            <h2 class="section-title">Sensor Values</h2>
            <div class="values-container">
                <div class="value-card">
                    <div class="value-label">Soil Moisture</div>
                    <div class="value-display">
                        <span id="moisture">72</span><span class="value-unit">%</span>
                    </div>
                </div>
            </div>

            <h2 class="section-title">Controls</h2>
            <div class="controls-container">
                <div class="control-buttons">
                    <button class="btn-mode" id="modeBtn" onclick="toggleMode(this)">
                        Auto: <span id="modeStatus">Off</span>
                    </button>
                </div>

                <div class="grid-controls">
                    <button class="btn-control" id="irrigationBtn" onclick="toggleIrrigation(this)">
                        <span>Irrigation</span>
                        <span class="control-status">OFF</span>
                    </button>
                </div>
            </div>
        </div>

        <!-- Settings Tab -->
        <div id="settings" class="tab-content">
            <div class="settings-form">
                <h2 class="section-title">Configuration</h2>
                <div class="success-message" id="successMsg">Settings saved successfully!</div>

                <div class="form-group">
                    <label for="dryValue">Dry Value (ADC reading)</label>
                    <input type="number" id="dryValue" min="0" max="1023" placeholder="1025">
                    <div class="form-description">ADC value when soil is completely dry</div>
                </div>

                <div class="form-group">
                    <label for="wetValue">Wet Value (ADC reading)</label>
                    <input type="number" id="wetValue" min="0" max="1023" placeholder="555">
                    <div class="form-description">ADC value when soil is completely wet</div>
                </div>

                <div class="form-group">
                    <label for="threshold">Dry Threshold (%)</label>
                    <input type="number" id="threshold" min="0" max="100" placeholder="40">
                    <div class="form-description">Moisture percentage at which irrigation turns ON in auto mode</div>
                </div>

                <button class="btn-save" onclick="saveSettings()">Save Settings</button>
            </div>
        </div>
    </div>

    <script>
        let isAutoMode = false;

        function switchTab(tabName) {
            // Hide all tabs
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            document.querySelectorAll('.nav-tab').forEach(tab => {
                tab.classList.remove('active');
            });

            // Show selected tab
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');

            if (tabName === 'settings') {
                loadSettings();
            }
        }

        // Load current settings
        function loadSettings() {
            fetch('/api/config')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('dryValue').value = data.dryValue;
                    document.getElementById('wetValue').value = data.wetValue;
                    document.getElementById('threshold').value = data.dryThreshold;
                })
                .catch(err => console.log('Error loading settings:', err));
        }

        // Save settings
        function saveSettings() {
            const dryValue = parseInt(document.getElementById('dryValue').value);
            const wetValue = parseInt(document.getElementById('wetValue').value);
            const threshold = parseInt(document.getElementById('threshold').value);

            fetch('/api/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    dryValue: dryValue,
                    wetValue: wetValue,
                    dryThreshold: threshold
                })
            })
            .then(res => res.json())
            .then(data => {
                const msg = document.getElementById('successMsg');
                msg.classList.add('show');
                setTimeout(() => msg.classList.remove('show'), 3000);
            })
            .catch(err => console.log('Error saving settings:', err));
        }

        // Update sensor readings every 2 seconds
        setInterval(() => {
            fetch('/api/sensors')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('moisture').textContent = Math.round(data.moisture);

                    if (isAutoMode && data.irrigation !== undefined) {
                        updateIrrigationButtonState(data.irrigation);
                    }
                })
                .catch(err => console.log('Error fetching sensors:', err));
        }, 2000);

        function toggleMode(button) {
            button.classList.toggle('active');
            isAutoMode = button.classList.contains('active');
            modeStatus = document.getElementById('modeStatus');

            const modeInfo = document.getElementById('modeInfo');
            const irrigationBtn = document.getElementById('irrigationBtn');

            if (isAutoMode) {
                irrigationBtn.classList.add('disabled');
                modeStatus.innerHTML = 'ON'
            } else {
                irrigationBtn.classList.remove('disabled');
                modeStatus.innerHTML = 'OFF'
            }

            fetch('/api/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({device: 'auto', state: isAutoMode})
            }).catch(err => console.log('Error:', err));
        }

        function toggleIrrigation(button) {
            if (isAutoMode) return;

            button.classList.toggle('active');
            const status = button.querySelector('.control-status');
            const isActive = button.classList.contains('active');
            status.textContent = isActive ? 'ON' : 'OFF';

            fetch('/api/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({device: 'irrigation', state: isActive})
            }).catch(err => console.log('Error:', err));
        }

        function updateIrrigationButtonState(isActive) {
            const irrigationBtn = document.getElementById('irrigationBtn');
            const status = irrigationBtn.querySelector('.control-status');

            if (isActive) {
                irrigationBtn.classList.add('active');
                status.textContent = 'ON';
            } else {
                irrigationBtn.classList.remove('active');
                status.textContent = 'OFF';
            }
        }
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSensors() {
  JsonDocument doc;
  doc["moisture"] = moisture;
  doc["irrigation"] = irrigation;
  doc["autoMode"] = config.autoMode;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleConfig() {
  if (server.method() == HTTP_GET) {
    JsonDocument doc;
    doc["dryValue"] = config.dryValue;
    doc["wetValue"] = config.wetValue;
    doc["dryThreshold"] = config.dryThreshold;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  } 
  else if (server.method() == HTTP_POST) {
    if (server.hasArg("plain")) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, server.arg("plain"));

      if (!error) {
        config.dryValue = doc["dryValue"] | config.dryValue;
        config.wetValue = doc["wetValue"] | config.wetValue;
        config.dryThreshold = doc["dryThreshold"] | config.dryThreshold;

        saveConfig();

        Serial.print("Config updated - New dry threshold: ");
        Serial.println(config.dryThreshold);

        server.send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"error\"}");
      }
    }
  }
}

void handleControl() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (!error) {
      String device = doc["device"];
      bool state = doc["state"];

      if (device == "irrigation" && !config.autoMode) {
        irrigation = state;
        digitalWrite(relayPin, state ? HIGH : LOW);
        Serial.print("Irrigation turned ");
        Serial.println(state ? "ON (Manual)" : "OFF (Manual)");
      }

      if (device == "auto") {
        config.autoMode = state;
        Serial.print("Auto mode turned ");
        Serial.println(state ? "ON" : "OFF");

        if (!state && irrigation) {
          irrigation = false;
          digitalWrite(relayPin, LOW);
        }
      }

      server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\"}");
    }
  }
}

// Captive portal redirect
void handleNotFound() {
  server.sendHeader("Location", String("http://") + apIP.toString(), true);
  server.send(302, "text/plain", "");
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(config.ssid, config.password);

  Serial.println("\nAP Mode Started");
  Serial.print("SSID: ");
  Serial.println(config.ssid);
  Serial.print("IP: ");
  Serial.println(apIP);

  dnsServer.start(DNS_PORT, "*", apIP);
  
  // Start mDNS
  if (MDNS.begin("greennodes")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder started");
    Serial.println("Access the dashboard at: http://greennodes.local");
  } else {
    Serial.println("Error setting up mDNS responder!");
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/sensors", handleSensors);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/control", HTTP_POST, handleControl);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(sensorPower, OUTPUT);
  pinMode(relayPin, OUTPUT);

  digitalWrite(sensorPower, LOW);
  digitalWrite(relayPin, LOW);

  Serial.println("\n\nGreennodes Irrigation System Starting...");

  initLittleFS();
  setupAP();
  setupWebServer();

  Serial.println("System Ready!");
  Serial.println("Connect to WiFi SSID: Greennodes");
  Serial.println("Open browser: http://192.168.4.1");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update();

  // Update sensors every 5 seconds
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    updateMoistureSensor();
    lastUpdate = millis();
  }

  // Handle auto mode
  handleAutoMode();
}