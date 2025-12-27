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

// Serve static files from LittleFS
void handleFileRequest() {
  String path = server.uri();
  
  // Default to index.html for root path
  if (path == "/") {
    path = "/index.html";
  }

  // Determine content type
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".json")) contentType = "application/json";

  // Check if file exists
  if (!LittleFS.exists(path)) {
    Serial.print("File not found: ");
    Serial.println(path);
    server.send(404, "text/plain", "File not found");
    return;
  }

  // Open and serve the file
  File file = LittleFS.open(path, "r");
  server.streamFile(file, contentType);
  file.close();
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
  // Serve static files
  server.onNotFound(handleFileRequest);
  
  // API endpoints
  server.on("/api/sensors", handleSensors);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/control", HTTP_POST, handleControl);

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