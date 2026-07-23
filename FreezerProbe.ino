/*
 * FreezerProbe - ESP32 Temperature Monitor with Prowl Notifications
 * 
 * Monitors temperature using DS18B20 sensor and sends alerts via Prowl API
 * when temperature goes outside configured thresholds.
 * 
 * Features:
 * - WiFi Manager (AP mode for initial setup)
 * - OTA Updates
 * - Temperature history and graphing
 * - Prowl push notifications
 * - Web interface with live updates
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ESP_Mail_Client.h>
#include <time.h>

// ===== Hardware Configuration =====
#define ONE_WIRE_BUS 4        // GPIO pin for DS18B20 data line (change as needed)
#define LED_PIN 2             // Built-in LED for status indication

// ===== Temperature Monitoring =====
#define TEMP_READ_INTERVAL 10000    // Read temperature every 10 seconds
#define ALERT_COOLDOWN 300000       // Wait 5 minutes between alerts (avoid spam)
#define HISTORY_SIZE 288            // Store 288 readings (48 hours at 10-second intervals)
#define ALERT_HISTORY_SIZE 50       // Store last 50 alerts
#define NTP_UPDATE_INTERVAL 300000  // Update NTP time every 5 minutes

// ===== Global Objects =====
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WebServer server(80);
Preferences preferences;
HTTPClient http;
WiFiManager wifiManager;
SMTPSession smtp;

// ===== Temperature History =====
struct TempReading {
  unsigned long timestamp;
  float temperature;
};

TempReading tempHistory[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;

// ===== Alert History =====
struct AlertRecord {
  unsigned long timestamp;
  String message;
  int priority;
  bool sentViaProwl;
  bool sentViaEmail;
};

AlertRecord alertHistory[ALERT_HISTORY_SIZE];
int alertHistoryIndex = 0;
int alertHistoryCount = 0;

// ===== Global Variables =====
String deviceName = "FreezerProbe";
float currentTemperature = 0.0;
float lowerThreshold = -20.0;
float upperThreshold = -10.0;
String prowlApiKey = "";
String otaPassword = "";
String timezone = "UTC0"; // Default to UTC

// Email settings
bool emailEnabled = false;
String emailSender = "";
String emailRecipient = "";
String smtpServer = "";
int smtpPort = 587;
String smtpUsername = "";
String smtpPassword = "";

bool sensorConnected = false;

unsigned long lastTempRead = 0;
unsigned long lastAlertTime = 0;
unsigned long lastNtpUpdate = 0;
bool lastAlertWasHigh = false;
bool lastAlertWasLow = false;
bool lastAlertWasSensorError = false;

// ===== Function Prototypes =====
void connectWiFi();
void setupOTA();
void setupWebServer();
void handleRoot();
void handleSettings();
void handleGetStatus();
void handleGetHistory();
void handleGetAlertHistory();
void readTemperature();
void addToAlertHistory(String message, int priority, bool prowl, bool email);
void configureTime();
time_t getCurrentTime();
void addToHistory(float temp);
void checkAlerts();
void sendProwlNotification(String message, int priority);
void sendEmailNotification(String subject, String message, int priority);
void smtpCallback(SMTP_Status status);
String getHTML();
void loadSettings();
void saveSettings();
String sanitizeHostname(String name);
bool isValidDeviceName(String name);
void updateHostname(String newHostname);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== FreezerProbe Starting ===");
  
  // Setup LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize preferences
  preferences.begin("freezerprobe", false);
  loadSettings();
  
  // Initialize DS18B20
  sensors.begin();
  int deviceCount = sensors.getDeviceCount();
  Serial.printf("Found %d DS18B20 device(s)\n", deviceCount);
  sensorConnected = (deviceCount > 0);
  
  if (sensorConnected) {
    sensors.setResolution(12); // 12-bit resolution (0.0625°C precision)
  } else {
    Serial.println("WARNING: No DS18B20 sensor detected!");
  }
  
  // Connect to WiFi using WiFiManager
  connectWiFi();
  
  // Configure NTP time synchronization
  configureTime();
  
  // Setup OTA updates
  setupOTA();
  
  // Setup mDNS with device name as hostname
  String hostname = sanitizeHostname(deviceName);
  if (MDNS.begin(hostname.c_str())) {
    Serial.printf("mDNS responder started: http://%s.local\n", hostname.c_str());
  } else {
    Serial.println("Error starting mDNS");
  }
  
  // Setup web server
  setupWebServer();
  
  // Initial temperature reading
  readTemperature();
  
  Serial.println("=== Setup Complete ===\n");
}

void loop() {
  ArduinoOTA.handle(); // Handle OTA updates
  server.handleClient();
  
  // Update NTP time periodically
  if (millis() - lastNtpUpdate >= NTP_UPDATE_INTERVAL) {
    configureTime();
    lastNtpUpdate = millis();
  }
  
  // Read temperature at intervals
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    readTemperature();
    checkAlerts();
    lastTempRead = millis();
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

// ===== WiFi Functions =====
void connectWiFi() {
  Serial.println("Configuring WiFi...");
  
  // Set WiFiManager to not block on connection
  wifiManager.setConfigPortalTimeout(180); // 3 minute timeout for config portal
  
  // Custom parameters can be added here if needed
  // WiFiManagerParameter custom_text("<p>FreezerProbe Configuration</p>");
  // wifiManager.addParameter(&custom_text);
  
  // Set custom AP name
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.print("Config AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    Serial.print("Config IP: ");
    Serial.println(WiFi.softAPIP());
    
    // Blink LED rapidly in config mode
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  });
  
  // Try to connect to saved WiFi or start config portal
  // If it cannot connect, it will start an AP named "FreezerProbe-Setup"
  if (!wifiManager.autoConnect("FreezerProbe-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    // Reset and try again
    ESP.restart();
    delay(1000);
  }
  
  // If we get here, we're connected
  digitalWrite(LED_PIN, HIGH); // LED on when connected
  Serial.println("\nWiFi Connected!");
  Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
}

void setupOTA() {
  // Port defaults to 3232
  ArduinoOTA.setPort(3232);
  
  // Set hostname based on device name
  String hostname = sanitizeHostname(deviceName);
  ArduinoOTA.setHostname(hostname.c_str());
  
  // Set password if configured
  if (otaPassword.length() > 0) {
    ArduinoOTA.setPassword(otaPassword.c_str());
  }
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    // Blink LED during update
    if (progress % 1000 == 0) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ===== Time Configuration Functions =====
void configureTime() {
  Serial.println("Configuring NTP time...");
  
  // Configure NTP with timezone string
  // Format: <std><offset><dst>,<dstStart>,<dstEnd>
  // Examples: "EST5EDT,M3.2.0,M11.1.0" for US Eastern
  //           "UTC0" for UTC
  //           "CET-1CEST,M3.5.0,M10.5.0/3" for Central Europe
  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Set timezone using POSIX TZ string
  setenv("TZ", timezone.c_str(), 1);
  tzset();
  
  // Wait for time to be set
  int retries = 0;
  time_t now;
  while ((now = time(nullptr)) < 8 * 3600 * 2 && retries < 15) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  
  if (now >= 8 * 3600 * 2) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.print("Time synchronized: ");
    Serial.println(asctime(&timeinfo));
  } else {
    Serial.println("Failed to synchronize time");
  }
}

time_t getCurrentTime() {
  return time(nullptr);
}

// ===== Web Server Functions =====
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/status", HTTP_GET, handleGetStatus);
  server.on("/history", HTTP_GET, handleGetHistory);
  server.on("/alerts", HTTP_GET, handleGetAlertHistory);
  server.on("/reset", HTTP_POST, []() {
    server.send(200, "text/plain", "Resetting WiFi settings...");
    delay(1000);
    wifiManager.resetSettings();
    ESP.restart();
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleSettings() {
  bool updated = false;
  bool hostnameChanged = false;
  String oldDeviceName = deviceName;
  
  if (server.hasArg("deviceName")) {
    String newName = server.arg("deviceName");
    
    // Validate device name for DNS compatibility
    if (newName.length() == 0) {
      newName = "FreezerProbe";
    }
    
    if (!isValidDeviceName(newName)) {
      server.send(400, "text/plain", "Error: Device name contains invalid characters. Use only letters, numbers, and hyphens. Cannot start or end with hyphen.");
      return;
    }
    
    if (newName != deviceName) {
      deviceName = newName;
      hostnameChanged = true;
    }
    updated = true;
  }
  
  if (server.hasArg("lowerThreshold")) {
    lowerThreshold = server.arg("lowerThreshold").toFloat();
    updated = true;
  }
  
  if (server.hasArg("upperThreshold")) {
    upperThreshold = server.arg("upperThreshold").toFloat();
    updated = true;
  }
  
  if (server.hasArg("prowlApiKey")) {
    prowlApiKey = server.arg("prowlApiKey");
    updated = true;
  }
  
  if (server.hasArg("otaPassword")) {
    otaPassword = server.arg("otaPassword");
    updated = true;
  }
  
  // Timezone setting
  bool timezoneChanged = false;
  if (server.hasArg("timezone")) {
    String newTimezone = server.arg("timezone");
    if (newTimezone != timezone) {
      timezone = newTimezone;
      timezoneChanged = true;
    }
    updated = true;
  }
  
  // Email settings
  if (server.hasArg("emailEnabled")) {
    emailEnabled = (server.arg("emailEnabled") == "true" || server.arg("emailEnabled") == "1");
    updated = true;
  } else if (server.hasArg("emailSender")) {
    // If any email field is present but emailEnabled isn't, assume it's a form submission
    emailEnabled = false;
  }
  
  if (server.hasArg("emailSender")) {
    emailSender = server.arg("emailSender");
    updated = true;
  }
  
  if (server.hasArg("emailRecipient")) {
    emailRecipient = server.arg("emailRecipient");
    updated = true;
  }
  
  if (server.hasArg("smtpServer")) {
    smtpServer = server.arg("smtpServer");
    updated = true;
  }
  
  if (server.hasArg("smtpPort")) {
    smtpPort = server.arg("smtpPort").toInt();
    if (smtpPort == 0) smtpPort = 587;
    updated = true;
  }
  
  if (server.hasArg("smtpUsername")) {
    smtpUsername = server.arg("smtpUsername");
    updated = true;
  }
  
  if (server.hasArg("smtpPassword")) {
    smtpPassword = server.arg("smtpPassword");
    updated = true;
  }
  
  if (updated) {
    // Validate thresholds
    if (lowerThreshold >= upperThreshold) {
      server.send(400, "text/plain", "Error: Lower threshold must be less than upper threshold");
      return;
    }
    
    // Validate email settings if enabled
    if (emailEnabled) {
      if (emailSender.length() == 0 || emailRecipient.length() == 0 || smtpServer.length() == 0) {
        server.send(400, "text/plain", "Error: Email enabled but required fields (sender, recipient, SMTP server) are missing");
        return;
      }
    }
    
    saveSettings();
    
    // Reset alert state when settings change
    lastAlertWasHigh = false;
    lastAlertWasLow = false;
    lastAlertWasSensorError = false;
    
    Serial.println("Settings updated:");
    Serial.printf("  Device Name: %s\n", deviceName.c_str());
    Serial.printf("  Lower Threshold: %.2f°C\n", lowerThreshold);
    Serial.printf("  Upper Threshold: %.2f°C\n", upperThreshold);
    Serial.printf("  Prowl API Key: %s\n", prowlApiKey.length() > 0 ? "SET" : "NOT SET");
    Serial.printf("  Email Enabled: %s\n", emailEnabled ? "YES" : "NO");
    if (emailEnabled) {
      Serial.printf("    Sender: %s\n", emailSender.c_str());
      Serial.printf("    Recipient: %s\n", emailRecipient.c_str());
      Serial.printf("    SMTP Server: %s:%d\n", smtpServer.c_str(), smtpPort);
    }
    
    // Update hostname if device name changed
    if (hostnameChanged) {
      String hostname = sanitizeHostname(deviceName);
      updateHostname(hostname);
      Serial.printf("Hostname updated to: %s.local\n", hostname.c_str());
    }
    
    // Reconfigure time if timezone changed
    if (timezoneChanged) {
      configureTime();
      Serial.printf("Timezone updated to: %s\n", timezone.c_str());
    }
    
    server.send(200, "text/plain", "Settings saved successfully");
  } else {
    server.send(400, "text/plain", "No settings provided");
  }
}

void handleGetStatus() {
  String json = "{";
  json += "\"deviceName\":\"" + deviceName + "\",";
  json += "\"timezone\":\"" + timezone + "\",";
  json += "\"temperature\":" + String(currentTemperature, 2) + ",";
  json += "\"lowerThreshold\":" + String(lowerThreshold, 2) + ",";
  json += "\"upperThreshold\":" + String(upperThreshold, 2) + ",";
  json += "\"prowlApiKeySet\":" + String(prowlApiKey.length() > 0 ? "true" : "false") + ",";
  json += "\"otaPasswordSet\":" + String(otaPassword.length() > 0 ? "true" : "false") + ",";
  json += "\"emailEnabled\":" + String(emailEnabled ? "true" : "false") + ",";
  json += "\"emailConfigured\":" + String((emailSender.length() > 0 && emailRecipient.length() > 0 && smtpServer.length() > 0) ? "true" : "false") + ",";
  json += "\"emailSender\":\"" + emailSender + "\",";
  json += "\"emailRecipient\":\"" + emailRecipient + "\",";
  json += "\"smtpServer\":\"" + smtpServer + "\",";
  json += "\"smtpPort\":" + String(smtpPort) + ",";
  json += "\"smtpUsername\":\"" + smtpUsername + "\",";
  json += "\"smtpPasswordSet\":" + String(smtpPassword.length() > 0 ? "true" : "false") + ",";
  json += "\"sensorConnected\":" + String(sensorConnected ? "true" : "false") + ",";
  json += "\"wifiRSSI\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"historyCount\":" + String(historyCount);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleGetHistory() {
  String json = "{\"readings\":[";
  
  // Send temperature history
  int count = min(historyCount, HISTORY_SIZE);
  for (int i = 0; i < count; i++) {
    // Calculate actual index (circular buffer)
    int idx = (historyIndex - count + i + HISTORY_SIZE) % HISTORY_SIZE;
    
    if (i > 0) json += ",";
    json += "{";
    json += "\"time\":" + String(tempHistory[idx].timestamp) + ",";
    json += "\"temp\":" + String(tempHistory[idx].temperature, 2);
    json += "}";
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleGetAlertHistory() {
  String json = "{\"alerts\":[";
  
  // Send alert history (most recent first)
  int count = min(alertHistoryCount, ALERT_HISTORY_SIZE);
  for (int i = count - 1; i >= 0; i--) {
    // Calculate actual index (circular buffer, reverse order)
    int idx = (alertHistoryIndex - 1 - (count - 1 - i) + ALERT_HISTORY_SIZE) % ALERT_HISTORY_SIZE;
    
    if (i < count - 1) json += ",";
    json += "{";
    json += "\"time\":" + String(alertHistory[idx].timestamp) + ",";
    json += "\"message\":\"" + alertHistory[idx].message + "\",";
    json += "\"priority\":" + String(alertHistory[idx].priority) + ",";
    json += "\"prowl\":" + String(alertHistory[idx].sentViaProwl ? "true" : "false") + ",";
    json += "\"email\":" + String(alertHistory[idx].sentViaEmail ? "true" : "false");
    json += "}";
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

// ===== Temperature Functions =====
void readTemperature() {
  if (!sensorConnected) {
    // Try to detect sensor again
    sensors.begin();
    int deviceCount = sensors.getDeviceCount();
    sensorConnected = (deviceCount > 0);
    
    if (!sensorConnected) {
      Serial.println("No sensor detected");
      currentTemperature = -999.0; // Error value
      return;
    }
  }
  
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  
  if (temp == DEVICE_DISCONNECTED_C || temp == 85.0) {
    // 85.0 is often a sensor error value
    Serial.println("Error reading temperature");
    sensorConnected = false;
    currentTemperature = -999.0;
  } else {
    currentTemperature = temp;
    addToHistory(temp); // Add to history
    Serial.printf("Temperature: %.2f°C\n", currentTemperature);
  }
}

void addToHistory(float temp) {
  tempHistory[historyIndex].timestamp = getCurrentTime(); // Store Unix timestamp
  tempHistory[historyIndex].temperature = temp;
  
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) {
    historyCount++;
  }
}

void addToAlertHistory(String message, int priority, bool prowl, bool email) {
  alertHistory[alertHistoryIndex].timestamp = getCurrentTime(); // Store Unix timestamp
  alertHistory[alertHistoryIndex].message = message;
  alertHistory[alertHistoryIndex].priority = priority;
  alertHistory[alertHistoryIndex].sentViaProwl = prowl;
  alertHistory[alertHistoryIndex].sentViaEmail = email;
  
  alertHistoryIndex = (alertHistoryIndex + 1) % ALERT_HISTORY_SIZE;
  if (alertHistoryCount < ALERT_HISTORY_SIZE) {
    alertHistoryCount++;
  }
}

void checkAlerts() {
  // Don't check alerts if no notification method configured
  bool notificationConfigured = (prowlApiKey.length() > 0) || (emailEnabled && emailSender.length() > 0);
  if (!notificationConfigured) {
    return;
  }
  
  // Check cooldown period
  if (millis() - lastAlertTime < ALERT_COOLDOWN) {
    return;
  }
  
  bool sendAlert = false;
  String alertMessage = "";
  int priority = 0;
  
  // Check for sensor disconnection or error
  if (!sensorConnected || currentTemperature == -999.0) {
    if (!lastAlertWasSensorError) {
      // Sensor just disconnected or error occurred
      sendAlert = true;
      lastAlertWasSensorError = true;
      lastAlertWasHigh = false;
      lastAlertWasLow = false;
      alertMessage = "[" + deviceName + "] ALERT: Temperature sensor disconnected or error reading sensor!";
      priority = 2; // High priority
      Serial.println(alertMessage);
    }
  }
  // Check for sensor recovery
  else if (lastAlertWasSensorError) {
    sendAlert = true;
    lastAlertWasSensorError = false;
    alertMessage = "[" + deviceName + "] Sensor reconnected. Current temperature: " + String(currentTemperature, 2) + "°C";
    priority = 0; // Normal priority
    Serial.println(alertMessage);
  }
  // Normal temperature threshold checks (only if sensor is working)
  else if (currentTemperature < lowerThreshold && !lastAlertWasLow) {
    sendAlert = true;
    lastAlertWasLow = true;
    lastAlertWasHigh = false;
    alertMessage = "[" + deviceName + "] ALERT: Temperature too LOW! Current: " + String(currentTemperature, 2) + "°C (Threshold: " + String(lowerThreshold, 2) + "°C)";
    priority = 2; // High priority
    Serial.println(alertMessage);
  } 
  else if (currentTemperature > upperThreshold && !lastAlertWasHigh) {
    sendAlert = true;
    lastAlertWasHigh = true;
    lastAlertWasLow = false;
    alertMessage = "[" + deviceName + "] ALERT: Temperature too HIGH! Current: " + String(currentTemperature, 2) + "°C (Threshold: " + String(upperThreshold, 2) + "°C)";
    priority = 2; // High priority
    Serial.println(alertMessage);
  }
  else if (currentTemperature >= lowerThreshold && currentTemperature <= upperThreshold) {
    // Temperature returned to normal range
    if (lastAlertWasLow || lastAlertWasHigh) {
      sendAlert = true;
      alertMessage = "[" + deviceName + "] Temperature returned to normal: " + String(currentTemperature, 2) + "°C";
      priority = 0; // Normal priority
      lastAlertWasLow = false;
      lastAlertWasHigh = false;
      Serial.println(alertMessage);
    }
  }
  
  if (sendAlert) {
    bool sentProwl = false;
    bool sentEmail = false;
    
    // Send Prowl notification if configured
    if (prowlApiKey.length() > 0) {
      sendProwlNotification(alertMessage, priority);
      sentProwl = true;
    }
    
    // Send Email notification if configured
    if (emailEnabled && emailSender.length() > 0) {
      String subject = deviceName + " Temperature Alert";
      if (lastAlertWasSensorError && priority == 2) {
        subject = deviceName + " Sensor Error";
      } else if (priority == 0) {
        subject = deviceName + " Status Normal";
      }
      sendEmailNotification(subject, alertMessage, priority);
      sentEmail = true;
    }
    
    // Record alert in history
    addToAlertHistory(alertMessage, priority, sentProwl, sentEmail);
    
    lastAlertTime = millis();
  }
}

void sendProwlNotification(String message, int priority) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send notification: WiFi not connected");
    return;
  }
  
  Serial.println("Sending Prowl notification...");
  
  http.begin("https://api.prowlapp.com/publicapi/add");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "apikey=" + prowlApiKey;
  postData += "&application=" + urlEncode(deviceName);
  postData += "&event=Temperature Alert";
  postData += "&description=" + urlEncode(message);
  postData += "&priority=" + String(priority);
  
  int httpCode = http.POST(postData);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.printf("Prowl response code: %d\n", httpCode);
    Serial.println(response);
    
    if (httpCode == 200) {
      Serial.println("Notification sent successfully");
    } else {
      Serial.println("Notification failed");
    }
  } else {
    Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void sendEmailNotification(String subject, String message, int priority) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send email: WiFi not connected");
    return;
  }
  
  if (!emailEnabled || emailSender.length() == 0 || emailRecipient.length() == 0 || smtpServer.length() == 0) {
    Serial.println("Email not configured properly");
    return;
  }
  
  Serial.println("Sending email notification...");
  
  // Set the callback function for SMTP session
  smtp.debug(1);
  smtp.callback(smtpCallback);
  
  // Declare the Session_Config for user defined session credentials
  Session_Config config;
  
  // Set the session config
  config.server.host_name = smtpServer.c_str();
  config.server.port = smtpPort;
  config.login.email = smtpUsername.length() > 0 ? smtpUsername.c_str() : emailSender.c_str();
  config.login.password = smtpPassword.c_str();
  
  // For library version 3.x only - comment out for older versions
  config.login.user_domain = "";
  
  // Set the NTP config time
  config.time.ntp_server = "pool.ntp.org,time.nist.gov";
  config.time.gmt_offset = 0;
  config.time.day_light_offset = 0;
  
  // Declare the message class
  SMTP_Message email;
  
  // Set the message headers
  email.sender.name = deviceName;
  email.sender.email = emailSender;
  email.subject = subject;
  email.addRecipient("User", emailRecipient);
  
  // Set the message content
  String htmlMsg = "<div style='font-family: Arial, sans-serif;'>";
  htmlMsg += "<h2 style='color: " + String(priority >= 2 ? "#d32f2f" : (priority == 0 ? "#388e3c" : "#f57c00")) + ";'>" + subject + "</h2>";
  htmlMsg += "<p>" + message + "</p>";
  htmlMsg += "<hr>";
  htmlMsg += "<p style='color: #666; font-size: 12px;'>Device: " + deviceName + "<br>";
  htmlMsg += "Time: " + String(millis() / 1000) + " seconds uptime<br>";
  htmlMsg += "WiFi Signal: " + String(WiFi.RSSI()) + " dBm</p>";
  htmlMsg += "</div>";
  
  email.html.content = htmlMsg.c_str();
  email.html.charSet = "utf-8";
  email.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  email.priority = priority >= 2 ? esp_mail_smtp_priority_high : (priority == 0 ? esp_mail_smtp_priority_normal : esp_mail_smtp_priority_normal);
  
  // Connect to the server
  if (!smtp.connect(&config)) {
    Serial.printf("Connection error: %s\n", smtp.errorReason().c_str());
    return;
  }
  
  // Start sending Email
  if (!MailClient.sendMail(&smtp, &email)) {
    Serial.printf("Error sending email: %s\n", smtp.errorReason().c_str());
  } else {
    Serial.println("Email sent successfully");
  }
  
  // Close the session
  smtp.closeSession();
}

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status) {
  // Print the current status
  Serial.println(status.info());
  
  // Print the sending result
  if (status.success()) {
    Serial.println("Email sent successfully");
  }
}

// ===== Settings Functions =====
void loadSettings() {
  deviceName = preferences.getString("deviceName", "FreezerProbe");
  lowerThreshold = preferences.getFloat("lowerThresh", -20.0);
  upperThreshold = preferences.getFloat("upperThresh", -10.0);
  prowlApiKey = preferences.getString("prowlKey", "");
  otaPassword = preferences.getString("otaPassword", "");
  timezone = preferences.getString("timezone", "UTC0");
  
  // Email settings
  emailEnabled = preferences.getBool("emailEnabled", false);
  emailSender = preferences.getString("emailSender", "");
  emailRecipient = preferences.getString("emailRecip", "");
  smtpServer = preferences.getString("smtpServer", "");
  smtpPort = preferences.getInt("smtpPort", 587);
  smtpUsername = preferences.getString("smtpUser", "");
  smtpPassword = preferences.getString("smtpPass", "");
  
  Serial.println("Settings loaded:");
  Serial.printf("  Device Name: %s\n", deviceName.c_str());
  Serial.printf("  Timezone: %s\n", timezone.c_str());
  Serial.printf("  Lower Threshold: %.2f°C\n", lowerThreshold);
  Serial.printf("  Upper Threshold: %.2f°C\n", upperThreshold);
  Serial.printf("  Prowl API Key: %s\n", prowlApiKey.length() > 0 ? "SET" : "NOT SET");
  Serial.printf("  Email Enabled: %s\n", emailEnabled ? "YES" : "NO");
}

void saveSettings() {
  preferences.putString("deviceName", deviceName);
  preferences.putFloat("lowerThresh", lowerThreshold);
  preferences.putFloat("upperThresh", upperThreshold);
  preferences.putString("prowlKey", prowlApiKey);
  preferences.putString("otaPassword", otaPassword);
  preferences.putString("timezone", timezone);
  
  // Email settings
  preferences.putBool("emailEnabled", emailEnabled);
  preferences.putString("emailSender", emailSender);
  preferences.putString("emailRecip", emailRecipient);
  preferences.putString("smtpServer", smtpServer);
  preferences.putInt("smtpPort", smtpPort);
  preferences.putString("smtpUser", smtpUsername);
  preferences.putString("smtpPass", smtpPassword);
  
  Serial.println("Settings saved to flash");
}

// ===== Utility Functions =====
String urlEncode(String str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// Sanitize device name to be DNS hostname compatible
String sanitizeHostname(String name) {
  String hostname = "";
  
  // Convert to lowercase and replace invalid characters
  for (int i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      hostname += tolower(c);
    } else if (c == ' ' || c == '_' || c == '.' || c == '-') {
      // Replace spaces and other separators with hyphen
      if (hostname.length() > 0 && hostname.charAt(hostname.length() - 1) != '-') {
        hostname += '-';
      }
    }
    // Skip other invalid characters
  }
  
  // Remove trailing hyphens
  while (hostname.length() > 0 && hostname.charAt(hostname.length() - 1) == '-') {
    hostname.remove(hostname.length() - 1);
  }
  
  // Remove leading hyphens
  while (hostname.length() > 0 && hostname.charAt(0) == '-') {
    hostname.remove(0, 1);
  }
  
  // Ensure hostname is not empty
  if (hostname.length() == 0) {
    hostname = "freezerprobe";
  }
  
  // Limit length to 63 characters (DNS label limit)
  if (hostname.length() > 63) {
    hostname = hostname.substring(0, 63);
  }
  
  return hostname;
}

// Validate device name contains only DNS-compatible characters
bool isValidDeviceName(String name) {
  if (name.length() == 0 || name.length() > 63) {
    return false;
  }
  
  // Cannot start or end with hyphen
  if (name.charAt(0) == '-' || name.charAt(name.length() - 1) == '-') {
    return false;
  }
  
  // Check each character
  for (int i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    bool valid = (c >= 'a' && c <= 'z') || 
                 (c >= 'A' && c <= 'Z') || 
                 (c >= '0' && c <= '9') || 
                 c == '-' || c == '_' || c == ' ' || c == '.';
    
    if (!valid) {
      return false;
    }
  }
  
  return true;
}

// Update mDNS and OTA hostname
void updateHostname(String newHostname) {
  // Update mDNS
  MDNS.end();
  if (MDNS.begin(newHostname.c_str())) {
    Serial.printf("mDNS hostname updated: http://%s.local\n", newHostname.c_str());
  } else {
    Serial.println("Failed to update mDNS hostname");
  }
  
  // Update OTA hostname
  ArduinoOTA.setHostname(newHostname.c_str());
}

// ===== HTML Generation =====
String getHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title id="pageTitle">FreezerProbe Monitor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
      background: white;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .header h1 {
      font-size: 2em;
      margin-bottom: 10px;
    }
    .temp-display {
      background: rgba(255,255,255,0.2);
      padding: 20px;
      border-radius: 15px;
      margin-top: 20px;
    }
    .temp-value {
      font-size: 3.5em;
      font-weight: bold;
      text-align: center;
    }
    .temp-label {
      text-align: center;
      opacity: 0.9;
      margin-top: 5px;
    }
    .content {
      padding: 30px;
    }
    .status {
      display: flex;
      justify-content: space-around;
      margin-bottom: 30px;
      padding: 15px;
      background: #f5f5f5;
      border-radius: 10px;
    }
    .status-item {
      text-align: center;
    }
    .status-label {
      font-size: 0.85em;
      color: #666;
      margin-bottom: 5px;
    }
    .status-value {
      font-weight: bold;
      color: #333;
    }
    .chart-container {
      position: relative;
      height: 300px;
      margin-bottom: 30px;
      background: #f9f9f9;
      padding: 20px;
      border-radius: 10px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      color: #333;
      font-weight: 500;
    }
    input {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      width: 100%;
      padding: 15px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s;
      margin-bottom: 10px;
    }
    button:hover {
      transform: translateY(-2px);
    }
    button:active {
      transform: translateY(0);
    }
    button.secondary {
      background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%);
    }
    .message {
      padding: 15px;
      border-radius: 8px;
      margin-top: 20px;
      display: none;
    }
    .message.success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .message.error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .info-box {
      background: #e3f2fd;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      font-size: 0.9em;
      color: #1565c0;
    }
    .sensor-error {
      background: #ffebee;
      color: #c62828;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      text-align: center;
      font-weight: 500;
    }
    .section-title {
      font-size: 1.3em;
      color: #333;
      margin: 30px 0 15px 0;
      padding-bottom: 10px;
      border-bottom: 2px solid #667eea;
    }
    .alert-table {
      width: 100%;
      border-collapse: collapse;
      background: white;
      border-radius: 8px;
      overflow: hidden;
      box-shadow: 0 2px 8px rgba(0,0,0,0.1);
      margin-bottom: 30px;
    }
    .alert-table th {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 12px;
      text-align: left;
      font-weight: 600;
      font-size: 0.9em;
    }
    .alert-table td {
      padding: 12px;
      border-bottom: 1px solid #e0e0e0;
      font-size: 0.9em;
    }
    .alert-table tr:last-child td {
      border-bottom: none;
    }
    .alert-table tr:hover {
      background: #f5f5f5;
    }
    .priority-badge {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 4px;
      font-size: 0.8em;
      font-weight: 600;
    }
    .priority-high {
      background: #ffebee;
      color: #c62828;
    }
    .priority-normal {
      background: #e8f5e9;
      color: #2e7d32;
    }
    .method-badge {
      display: inline-block;
      padding: 3px 6px;
      border-radius: 3px;
      font-size: 0.75em;
      margin-right: 4px;
      background: #e3f2fd;
      color: #1565c0;
    }
    .no-alerts {
      text-align: center;
      padding: 40px;
      color: #999;
      font-style: italic;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1 id="deviceHeader">❄️ FreezerProbe</h1>
      <p>Temperature Monitoring System</p>
      <div class="temp-display">
        <div class="temp-value" id="temperature">--</div>
        <div class="temp-label">Current Temperature</div>
      </div>
    </div>
    
    <div class="content">
      <div id="sensorError" class="sensor-error" style="display:none;">
        ⚠️ Sensor not connected or error reading temperature
      </div>
      
      <div class="status">
        <div class="status-item">
          <div class="status-label">Lower Limit</div>
          <div class="status-value" id="lowerLimit">--</div>
        </div>
        <div class="status-item">
          <div class="status-label">Upper Limit</div>
          <div class="status-value" id="upperLimit">--</div>
        </div>
        <div class="status-item">
          <div class="status-label">Alerts</div>
          <div class="status-value" id="alertStatus">--</div>
        </div>
        <div class="status-item">
          <div class="status-label">WiFi</div>
          <div class="status-value" id="wifiStatus">--</div>
        </div>
      </div>
      
      <h2 class="section-title">📊 Temperature History</h2>
      <div class="chart-container">
        <canvas id="tempChart"></canvas>
      </div>
      
      <h2 class="section-title">🔔 Alert History</h2>
      <div style="overflow-x: auto;">
        <table class="alert-table" id="alertTable">
          <thead>
            <tr>
              <th>Time</th>
              <th>Alert Message</th>
              <th>Priority</th>
              <th>Sent Via</th>
            </tr>
          </thead>
          <tbody id="alertTableBody">
            <tr>
              <td colspan="4" class="no-alerts">No alerts recorded yet</td>
            </tr>
          </tbody>
        </table>
      </div>
      
      <div class="info-box">
        ℹ️ Alerts will be sent via Prowl and/or Email when temperature goes outside the threshold range. 
        A 5-minute cooldown prevents notification spam. Configure at least one notification method below.
      </div>
      
      <h2 class="section-title">⚙️ Settings</h2>
      
      <form id="settingsForm">
        <div class="form-group">
          <label for="deviceName">Device Name</label>
          <input type="text" id="deviceName" name="deviceName" placeholder="FreezerProbe" required pattern="[A-Za-z0-9][A-Za-z0-9 ._-]*[A-Za-z0-9]|[A-Za-z0-9]" maxlength="63">
          <small style="color: #666; display: block; margin-top: 5px;">
            Used in notifications and as hostname (http://[name].local). Use only letters, numbers, spaces, hyphens, underscores, and periods. Cannot start/end with hyphen.
          </small>
        </div>
        
        <div class="form-group">
          <label for="timezone">Timezone</label>
          <select id="timezone" name="timezone" style="width: 100%; padding: 12px; border: 2px solid #e0e0e0; border-radius: 8px; font-size: 16px;">
            <option value="UTC0">UTC (GMT+0)</option>
            <option value="GMT0BST,M3.5.0/1,M10.5.0">UK (GMT/BST)</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe (CET/CEST)</option>
            <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe (EET/EEST)</option>
            <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern (EST/EDT)</option>
            <option value="CST6CDT,M3.2.0,M11.1.0">US Central (CST/CDT)</option>
            <option value="MST7MDT,M3.2.0,M11.1.0">US Mountain (MST/MDT)</option>
            <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific (PST/PDT)</option>
            <option value="AKST9AKDT,M3.2.0,M11.1.0">Alaska (AKST/AKDT)</option>
            <option value="HST10">Hawaii (HST)</option>
            <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia East (AEST/AEDT)</option>
            <option value="ACST-9:30ACDT,M10.1.0,M4.1.0/3">Australia Central (ACST/ACDT)</option>
            <option value="AWST-8">Australia West (AWST)</option>
            <option value="NZST-12NZDT,M9.5.0,M4.1.0/3">New Zealand (NZST/NZDT)</option>
            <option value="JST-9">Japan (JST)</option>
            <option value="KST-9">Korea (KST)</option>
            <option value="CST-8">China (CST)</option>
            <option value="IST-5:30">India (IST)</option>
            <option value="<+03>-3">Moscow (MSK)</option>
            <option value="WIB-7">Indonesia West (WIB)</option>
            <option value="WITA-8">Indonesia Central (WITA)</option>
            <option value="WIT-9">Indonesia East (WIT)</option>
          </select>
          <small style="color: #666; display: block; margin-top: 5px;">
            Select your local timezone. Time will be synchronized via NTP every 5 minutes.
          </small>
        </div>
        
        <div class="form-group">
          <label for="lowerThreshold">Lower Temperature Threshold (°C)</label>
          <input type="number" step="0.1" id="lowerThreshold" name="lowerThreshold" required>
        </div>
        
        <div class="form-group">
          <label for="upperThreshold">Upper Temperature Threshold (°C)</label>
          <input type="number" step="0.1" id="upperThreshold" name="upperThreshold" required>
        </div>
        
        <h3 style="margin-top: 30px; margin-bottom: 15px; color: #333;">📱 Prowl Notifications</h3>
        
        <div class="form-group">
          <label for="prowlApiKey">Prowl API Key (Optional)</label>
          <input type="text" id="prowlApiKey" name="prowlApiKey" placeholder="Enter your Prowl API key">
          <small style="color: #666; display: block; margin-top: 5px;">
            Get your API key from <a href="https://www.prowlapp.com" target="_blank">prowlapp.com</a>
          </small>
          <div id="prowlKeyHint" style="display: none; margin-top: 10px;">
            <small style="color: #388e3c;">✓ Prowl API key is set</small>
            <button type="button" onclick="clearProwlKey()" style="width: auto; padding: 8px 16px; font-size: 14px; margin-left: 10px; background: linear-gradient(135deg, #ff9800 0%, #f57c00 100%);">Change Key</button>
          </div>
        </div>
        
        <h3 style="margin-top: 30px; margin-bottom: 15px; color: #333;">� OTA Update Security</h3>
        
        <div class="form-group">
          <label for="otaPassword">OTA Password (Optional)</label>
          <input type="password" id="otaPassword" name="otaPassword" placeholder="Leave empty for no password protection">
          <small style="color: #666; display: block; margin-top: 5px;">
            Protect Over-The-Air firmware updates with a password. Leave empty to disable authentication.
          </small>
          <div id="otaPasswordHint" style="display: none; margin-top: 10px;">
            <small style="color: #388e3c;">✓ OTA password is set</small>
            <button type="button" onclick="clearOtaPassword()" style="width: auto; padding: 8px 16px; font-size: 14px; margin-left: 10px; background: linear-gradient(135deg, #ff9800 0%, #f57c00 100%);">Change Password</button>
          </div>
        </div>
        
        <h3 style="margin-top: 30px; margin-bottom: 15px; color: #333;">�📧 Email Notifications</h3>
        
        <div class="form-group">
          <label style="display: flex; align-items: center; cursor: pointer;">
            <input type="checkbox" id="emailEnabled" name="emailEnabled" style="width: auto; margin-right: 10px;">
            <span>Enable Email Notifications</span>
          </label>
        </div>
        
        <div id="emailFields" style="display: none;">
          <div class="form-group">
            <label for="emailSender">Sender Email Address *</label>
            <input type="email" id="emailSender" name="emailSender" placeholder="freezer@yourdomain.com">
          </div>
          
          <div class="form-group">
            <label for="emailRecipient">Recipient Email Address *</label>
            <input type="email" id="emailRecipient" name="emailRecipient" placeholder="you@example.com">
          </div>
          
          <div class="form-group">
            <label for="smtpServer">SMTP Server *</label>
            <input type="text" id="smtpServer" name="smtpServer" placeholder="smtp.gmail.com">
            <small style="color: #666; display: block; margin-top: 5px;">
              Gmail: smtp.gmail.com | Outlook: smtp-mail.outlook.com | Yahoo: smtp.mail.yahoo.com
            </small>
          </div>
          
          <div class="form-group">
            <label for="smtpPort">SMTP Port</label>
            <input type="number" id="smtpPort" name="smtpPort" placeholder="587" value="587">
            <small style="color: #666; display: block; margin-top: 5px;">
              Common ports: 587 (TLS), 465 (SSL), 25 (unsecured)
            </small>
          </div>
          
          <div class="form-group">
            <label for="smtpUsername">SMTP Username (Optional)</label>
            <input type="text" id="smtpUsername" name="smtpUsername" placeholder="Leave empty to use sender email">
          </div>
          
          <div class="form-group">
            <label for="smtpPassword">SMTP Password (Optional)</label>
            <input type="password" id="smtpPassword" name="smtpPassword" placeholder="Your SMTP password or app-specific password">
            <small style="color: #666; display: block; margin-top: 5px;">
              For Gmail, use an <a href="https://myaccount.google.com/apppasswords" target="_blank">App Password</a>. Leave blank to keep existing password.
            </small>
          </div>
        </div>
        
        <button type="submit">💾 Save Settings</button>
      </form>
      
      <button class="secondary" onclick="resetWiFi()">🔄 Reset WiFi Settings</button>
      
      <div id="message" class="message"></div>
    </div>
  </div>
  
  <script>
    let tempChart;
    let chartData = {
      labels: [],
      datasets: [{
        label: 'Temperature (°C)',
        data: [],
        borderColor: '#667eea',
        backgroundColor: 'rgba(102, 126, 234, 0.1)',
        borderWidth: 2,
        tension: 0.4,
        fill: true
      }]
    };
    
    // Track if form has been loaded and if Prowl key was originally set
    let formLoaded = false;
    let prowlKeyWasSet = false;
    let prowlKeyModified = false;
    let otaPasswordModified = false;
    let smtpPasswordModified = false;
    
    // Initialize chart
    function initChart() {
      const ctx = document.getElementById('tempChart').getContext('2d');
      tempChart = new Chart(ctx, {
        type: 'line',
        data: chartData,
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: {
              display: true,
              position: 'top'
            },
            tooltip: {
              mode: 'index',
              intersect: false
            }
          },
          scales: {
            x: {
              display: true,
              title: {
                display: true,
                text: 'Time'
              },
              ticks: {
                maxTicksLimit: 10
              }
            },
            y: {
              display: true,
              title: {
                display: true,
                text: 'Temperature (°C)'
              }
            }
          },
          interaction: {
            mode: 'nearest',
            axis: 'x',
            intersect: false
          }
        }
      });
    }
    
    // Update temperature history chart
    function updateHistory() {
      fetch('/history')
        .then(response => response.json())
        .then(data => {
          const readings = data.readings || [];
          
          chartData.labels = readings.map(r => {
            const date = new Date(r.time * 1000);
            return date.toLocaleTimeString();
          });
          
          chartData.datasets[0].data = readings.map(r => r.temp);
          
          if (tempChart) {
            tempChart.update();
          }
        })
        .catch(error => {
          console.error('Error fetching history:', error);
        });
    }
    
    // Update alert history table
    function updateAlertHistory() {
      fetch('/alerts')
        .then(response => response.json())
        .then(data => {
          const alerts = data.alerts || [];
          const tbody = document.getElementById('alertTableBody');
          
          if (alerts.length === 0) {
            tbody.innerHTML = '<tr><td colspan=\"4\" class=\"no-alerts\">No alerts recorded yet</td></tr>';
            return;
          }
          
          tbody.innerHTML = '';
          alerts.forEach(alert => {
            const row = document.createElement('tr');
            
            // Time column
            const date = new Date(alert.time * 1000);
            const timeCell = document.createElement('td');
            timeCell.textContent = date.toLocaleString();
            row.appendChild(timeCell);
            
            // Message column
            const msgCell = document.createElement('td');
            msgCell.textContent = alert.message;
            row.appendChild(msgCell);
            
            // Priority column
            const priorityCell = document.createElement('td');
            const priorityBadge = document.createElement('span');
            priorityBadge.className = 'priority-badge ' + (alert.priority >= 2 ? 'priority-high' : 'priority-normal');
            priorityBadge.textContent = alert.priority >= 2 ? 'HIGH' : 'NORMAL';
            priorityCell.appendChild(priorityBadge);
            row.appendChild(priorityCell);
            
            // Sent via column
            const methodCell = document.createElement('td');
            if (alert.prowl) {
              const prowlBadge = document.createElement('span');
              prowlBadge.className = 'method-badge';
              prowlBadge.textContent = '📱 Prowl';
              methodCell.appendChild(prowlBadge);
            }
            if (alert.email) {
              const emailBadge = document.createElement('span');
              emailBadge.className = 'method-badge';
              emailBadge.textContent = '📧 Email';
              methodCell.appendChild(emailBadge);
            }
            if (!alert.prowl && !alert.email) {
              methodCell.textContent = 'None';
              methodCell.style.color = '#999';
            }
            row.appendChild(methodCell);
            
            tbody.appendChild(row);
          });
        })
        .catch(error => {
          console.error('Error fetching alert history:', error);
        });
    }
    
    // Update status every 5 seconds
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          // Update temperature display
          if (data.sensorConnected && data.temperature !== -999) {
            document.getElementById('temperature').textContent = data.temperature.toFixed(2) + '°C';
            document.getElementById('sensorError').style.display = 'none';
            
            // Color code temperature
            const tempElement = document.getElementById('temperature');
            if (data.temperature < data.lowerThreshold) {
              tempElement.style.color = '#2196F3'; // Blue for too cold
            } else if (data.temperature > data.upperThreshold) {
              tempElement.style.color = '#F44336'; // Red for too hot
            } else {
              tempElement.style.color = '#4CAF50'; // Green for OK
            }
          } else {
            document.getElementById('temperature').textContent = 'ERROR';
            document.getElementById('sensorError').style.display = 'block';
          }
          
          // Update thresholds
          document.getElementById('lowerLimit').textContent = data.lowerThreshold.toFixed(1) + '°C';
          document.getElementById('upperLimit').textContent = data.upperThreshold.toFixed(1) + '°C';
          
          // Update alert status
          let alertMethods = [];
          if (data.prowlApiKeySet) alertMethods.push('Prowl');
          if (data.emailEnabled && data.emailConfigured) alertMethods.push('Email');
          const alertStatus = alertMethods.length > 0 ? '✅ ' + alertMethods.join(', ') : '❌ Disabled';
          document.getElementById('alertStatus').textContent = alertStatus;
          
          // Update WiFi status
          const wifiRSSI = data.wifiRSSI;
          let wifiIcon = '📶';
          if (wifiRSSI > -50) wifiIcon = '📶📶📶';
          else if (wifiRSSI > -70) wifiIcon = '📶📶';
          else wifiIcon = '📶';
          document.getElementById('wifiStatus').textContent = wifiIcon + ' ' + wifiRSSI + 'dBm';
          
          // Only populate form fields on first load
          if (!formLoaded) {
            document.getElementById('deviceName').value = data.deviceName || 'FreezerProbe';
            document.getElementById('timezone').value = data.timezone || 'UTC0';
            document.getElementById('lowerThreshold').value = data.lowerThreshold;
            document.getElementById('upperThreshold').value = data.upperThreshold;
            
            // Handle Prowl API key - show masked if set
            prowlKeyWasSet = data.prowlApiKeySet;
            if (prowlKeyWasSet) {
              document.getElementById('prowlApiKey').value = '••••••••••••••••';
              document.getElementById('prowlApiKey').disabled = true;
              document.getElementById('prowlKeyHint').style.display = 'block';
            }
            
            // Handle OTA password - show masked if set
            if (data.otaPasswordSet) {
              document.getElementById('otaPassword').value = '••••••••••••••••';
              document.getElementById('otaPassword').disabled = true;
              document.getElementById('otaPasswordHint').style.display = 'block';
            }
            
            // Email settings
            if (data.emailEnabled !== undefined) {
              document.getElementById('emailEnabled').checked = data.emailEnabled;
            }
            if (data.emailSender) {
              document.getElementById('emailSender').value = data.emailSender;
            }
            if (data.emailRecipient) {
              document.getElementById('emailRecipient').value = data.emailRecipient;
            }
            if (data.smtpServer) {
              document.getElementById('smtpServer').value = data.smtpServer;
            }
            if (data.smtpPort) {
              document.getElementById('smtpPort').value = data.smtpPort;
            }
            if (data.smtpUsername) {
              document.getElementById('smtpUsername').value = data.smtpUsername;
            }
            // Handle SMTP password - show masked if set
            if (data.smtpPasswordSet) {
              document.getElementById('smtpPassword').value = '••••••••••••••••';
              document.getElementById('smtpPassword').placeholder = 'Password is set (leave blank to keep)';
            }
            
            toggleEmailFields();
            formLoaded = true;
          }
          
          // Always update page title and header (not form fields)
          document.getElementById('pageTitle').textContent = (data.deviceName || 'FreezerProbe') + ' Monitor';
          document.getElementById('deviceHeader').textContent = '❄️ ' + (data.deviceName || 'FreezerProbe');
        })
        .catch(error => {
          console.error('Error fetching status:', error);
        });
    }
    
    // Toggle email fields visibility
    function toggleEmailFields() {
      const emailFields = document.getElementById('emailFields');
      const emailEnabled = document.getElementById('emailEnabled').checked;
      emailFields.style.display = emailEnabled ? 'block' : 'none';
    }
    
    // Handle email checkbox change
    document.getElementById('emailEnabled').addEventListener('change', toggleEmailFields);
    
    // Clear Prowl key to allow editing
    function clearProwlKey() {
      document.getElementById('prowlApiKey').value = '';
      document.getElementById('prowlApiKey').disabled = false;
      document.getElementById('prowlApiKey').placeholder = 'Enter new Prowl API key';
      document.getElementById('prowlKeyHint').style.display = 'none';
      prowlKeyModified = true;
      document.getElementById('prowlApiKey').focus();
    }
    
    // Clear OTA password to allow editing
    function clearOtaPassword() {
      document.getElementById('otaPassword').value = '';
      document.getElementById('otaPassword').disabled = false;
      document.getElementById('otaPassword').placeholder = 'Enter new OTA password';
      document.getElementById('otaPasswordHint').style.display = 'none';
      otaPasswordModified = true;
      document.getElementById('otaPassword').focus();
    }
    
    // Track if Prowl key field is modified
    document.getElementById('prowlApiKey').addEventListener('input', function() {
      if (!this.disabled) {
        prowlKeyModified = true;
      }
    });
    
    // Track if OTA password field is modified
    document.getElementById('otaPassword').addEventListener('input', function() {
      if (!this.disabled) {
        otaPasswordModified = true;
      }
    });
    
    // Track if SMTP password field is modified
    document.getElementById('smtpPassword').addEventListener('focus', function() {
      if (this.value === '••••••••••••••••') {
        this.value = '';
        this.placeholder = 'Enter new password or leave blank to keep current';
      }
    });
    
    document.getElementById('smtpPassword').addEventListener('input', function() {
      smtpPasswordModified = true;
    });
    
    // Handle form submission
    document.getElementById('settingsForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      const formData = new FormData(this);
      
      // Only include Prowl API key if it was modified
      if (!prowlKeyModified || document.getElementById('prowlApiKey').disabled) {
        formData.delete('prowlApiKey');
      }
      
      // Only include OTA password if it was modified
      if (!otaPasswordModified || document.getElementById('otaPassword').disabled) {
        formData.delete('otaPassword');
      }
      
      // Only include SMTP password if it was modified and not empty
      const smtpPasswordValue = document.getElementById('smtpPassword').value;
      if (!smtpPasswordModified || smtpPasswordValue === '' || smtpPasswordValue === '••••••••••••••••') {
        formData.delete('smtpPassword');
      }
      
      // Handle checkbox value
      if (document.getElementById('emailEnabled').checked) {
        formData.set('emailEnabled', 'true');
      } else {
        formData.set('emailEnabled', 'false');
      }
      
      const params = new URLSearchParams(formData);
      
      fetch('/settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params
      })
      .then(response => {
        if (response.ok) {
          return response.text();
        } else {
          return response.text().then(text => { throw new Error(text); });
        }
      })
      .then(data => {
        showMessage(data, 'success');
        
        // If Prowl key was updated, mark it as set
        if (prowlKeyModified && document.getElementById('prowlApiKey').value.length > 0) {
          prowlKeyWasSet = true;
          document.getElementById('prowlApiKey').value = '••••••••••••••••';
          document.getElementById('prowlApiKey').disabled = true;
          document.getElementById('prowlKeyHint').style.display = 'block';
        }
        
        // If OTA password was updated, show it as set
        if (otaPasswordModified && document.getElementById('otaPassword').value.length > 0) {
          document.getElementById('otaPassword').value = '••••••••••••••••';
          document.getElementById('otaPassword').disabled = true;
          document.getElementById('otaPasswordHint').style.display = 'block';
        }
        
        // If SMTP password was updated, show it as set
        if (smtpPasswordModified && document.getElementById('smtpPassword').value.length > 0) {
          document.getElementById('smtpPassword').value = '••••••••••••••••';
          document.getElementById('smtpPassword').placeholder = 'Password is set (leave blank to keep)';
        }
        
        prowlKeyModified = false;
        otaPasswordModified = false;
        smtpPasswordModified = false;
        updateStatus(); // Refresh status after saving
      })
      .catch(error => {
        showMessage('Error saving settings: ' + error.message, 'error');
      });
    });
    
    function resetWiFi() {
      if (confirm('This will reset WiFi settings and restart the device. Continue?')) {
        fetch('/reset', { method: 'POST' })
          .then(() => {
            showMessage('WiFi settings reset. Device is restarting...', 'success');
          })
          .catch(error => {
            showMessage('Error resetting WiFi: ' + error.message, 'error');
          });
      }
    }
    
    function showMessage(text, type) {
      const messageDiv = document.getElementById('message');
      messageDiv.textContent = text;
      messageDiv.className = 'message ' + type;
      messageDiv.style.display = 'block';
      
      setTimeout(() => {
        messageDiv.style.display = 'none';
      }, 5000);
    }
    
    // Initialize
    initChart();
    updateStatus();
    updateHistory();
    updateAlertHistory();
    
    // Update status every 5 seconds
    setInterval(updateStatus, 5000);
    
    // Update history every 30 seconds
    setInterval(updateHistory, 30000);
    
    // Update alert history every 30 seconds
    setInterval(updateAlertHistory, 30000);
  </script>
</body>
</html>
)rawliteral";
  
  return html;
}

