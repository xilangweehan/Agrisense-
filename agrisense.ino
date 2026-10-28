#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

// ================= USER CONFIG =================
// Two WiFi networks (2.4GHz only). The ESP32 joins whichever one is in range
// (strongest wins if both are), and falls back to the other if one drops.
const char* WIFI1_SSID = "Naumans-2G_EXT";
const char* WIFI1_PASS = "construction2018";
const char* WIFI2_SSID = "YOUR_SECOND_SSID";   // <-- change me
const char* WIFI2_PASS = "YOUR_SECOND_PASS";   // <-- change me

// NTP / timezone. Abu Dhabi = UTC+4, no daylight saving.
const long   GMT_OFFSET_SEC      = 4 * 3600;
const int    DAYLIGHT_OFFSET_SEC = 0;
const char*  NTP_SERVER_1 = "pool.ntp.org";
const char*  NTP_SERVER_2 = "time.google.com";

// ================= PIN DEFINITIONS (unchanged) =================
LiquidCrystal_I2C lcd(0x27, 16, 2);   // I2C: SDA=GPIO0, SCL=GPIO1

#define DHTPIN 9
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int soilDigitalPin = 6;   // Soil DO (unused for logic, kept for reference)
const int soilAnalogPin  = 4;   // Soil AO -> soil %
const int ldrAnalogPin   = 2;   // LDR AO -> light %

const int pumpPin = 23;  // pump driver transistor
const int ledPin  = 10;  // LED
const int fanPin  = 18;  // fan driver transistor

const int touchPin = 5;  // capacitive touch module output -> scrolls LCD screens
const bool TOUCH_ACTIVE_HIGH = true;  // TTP223-style modules go HIGH when touched

// Soil raw ADC calibration
const int SOIL_RAW_DRY = 4095;
const int SOIL_RAW_WET = 1500;

// Light raw ADC calibration - placeholders, see Serial Monitor to tune
int LIGHT_RAW_DARK   = 4095;
int LIGHT_RAW_BRIGHT = 500;

// ================= FIXED CONTROL THRESHOLDS (no longer adjustable) =================
const float PUMP_ON_BELOW  = 30;  // soil %
const float PUMP_OFF_ABOVE = 35;
const float LED_ON_ABOVE   = 70;  // light %
const float LED_OFF_BELOW  = 60;
const float FAN_ON_ABOVE   = 70;  // humidity %
const float FAN_OFF_BELOW  = 60;

// ================= RESERVOIR ESTIMATE =================
// There is no level sensor, so water used is estimated from pump run-time.
// CALIBRATE: run the pump into a measuring cup for 10 s, divide the ml by 10.
const float RESERVOIR_ML    = 500.0;
const float PUMP_ML_PER_SEC = 10.0;   // <-- placeholder, measure yours

// ================= STATE =================
bool wifiConnected = false;
bool timeSynced    = false;

unsigned long previousSensorMillis    = 0;
unsigned long previousLogMillis       = 0;
unsigned long previousWifiCheckMillis = 0;
unsigned long previousDrawMillis      = 0;
unsigned long lastPrefsSave           = 0;

const int NUM_SCREENS = 5;
int  displayScreen = 0;

float currentTemp = 26.0;
float currentHum  = 50.0;
int   soilPercent = 0;
int   lightPercent = 0;

bool pumpOn = false;
bool ledOn  = false;
bool fanOn  = false;

unsigned long pumpMsSinceRefill = 0;   // persisted
unsigned long pumpMsSaved       = 0;

bool touchLast = false;
unsigned long lastTouchMs = 0;

Preferences prefs;
WiFiMulti wifiMulti;
WebServer server(80);

float reservoirMl() {
  float used = (pumpMsSinceRefill / 1000.0f) * PUMP_ML_PER_SEC;
  float left = RESERVOIR_ML - used;
  return left < 0 ? 0 : left;
}

// ================= LOGGING / HISTORY =================
const char* LOG_PATH     = "/log.csv";
const char* LOG_OLD_PATH = "/log_old.csv";
const char* CSV_HEADER   = "Date,Time,Temp_C,Humidity_pct,Soil_pct,Light_pct,Pump,LED,Fan,Reservoir_ml";
const unsigned long LOG_INTERVAL_MS = 60000; // 1 row/minute

#define HISTORY_SIZE 120   // 2 hours at 1 row/min
struct Reading {
  char ts[20];
  float temp, hum;
  int soil, light, ml;
  bool pump, led, fan;
};
Reading history[HISTORY_SIZE];
int historyCount = 0;
int historyIndex = 0;

void pushHistory(const char* ts) {
  Reading &r = history[historyIndex];
  strncpy(r.ts, ts, sizeof(r.ts) - 1);
  r.ts[sizeof(r.ts) - 1] = 0;
  r.temp = currentTemp; r.hum = currentHum;
  r.soil = soilPercent; r.light = lightPercent;
  r.ml = (int)reservoirMl();
  r.pump = pumpOn; r.led = ledOn; r.fan = fanOn;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
}

void createLogFile() {
  File f = LittleFS.open(LOG_PATH, "w");
  if (f) { f.println(CSV_HEADER); f.close(); }
}

// If an old-format log exists, keep it as /log_old.csv and start a clean new one.
void ensureLogFile() {
  if (LittleFS.exists(LOG_PATH)) {
    File f = LittleFS.open(LOG_PATH, "r");
    String first = f ? f.readStringUntil('\n') : String("");
    if (f) f.close();
    first.trim();
    if (first == CSV_HEADER) return;
    if (LittleFS.exists(LOG_OLD_PATH)) LittleFS.remove(LOG_OLD_PATH);
    LittleFS.rename(LOG_PATH, LOG_OLD_PATH);
  }
  createLogFile();
}

void logReading() {
  if (!timeSynced) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) return;
  char ts[20], d[11], t[9];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &timeinfo);
  strftime(d, sizeof(d), "%Y-%m-%d", &timeinfo);
  strftime(t, sizeof(t), "%H:%M:%S", &timeinfo);

  // stop writing if storage is nearly full (use the Flush button on the page)
  if (LittleFS.totalBytes() > 0 && LittleFS.usedBytes() > LittleFS.totalBytes() * 0.95) {
    Serial.println("Storage almost full - not logging");
  } else {
    File f = LittleFS.open(LOG_PATH, "a");
    if (f) {
      char line[128];
      snprintf(line, sizeof(line), "%s,%s,%.1f,%.1f,%d,%d,%d,%d,%d,%d",
               d, t, currentTemp, currentHum, soilPercent, lightPercent,
               pumpOn ? 1 : 0, ledOn ? 1 : 0, fanOn ? 1 : 0, (int)reservoirMl());
      f.println(line);
      f.close();
    } else {
      Serial.println("Log write failed");
    }
  }
  pushHistory(ts);
}

// ================= WEB PAGE =================
#include "page.h"   // web page lives in its own file (see page.h)

// ================= WEB HANDLERS =================
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleData() {
  char ts[20] = "unsynced";
  struct tm timeinfo;
  if (timeSynced && getLocalTime(&timeinfo, 100)) {
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }

  size_t logBytes = 0;
  File f = LittleFS.open(LOG_PATH, "r");
  if (f) { logBytes = f.size(); f.close(); }

  String json = "{";
  json += "\"temp\":" + String(currentTemp, 1) + ",";
  json += "\"hum\":" + String(currentHum, 1) + ",";
  json += "\"soil\":" + String(soilPercent) + ",";
  json += "\"light\":" + String(lightPercent) + ",";
  json += "\"pump\":" + String(pumpOn ? "true" : "false") + ",";
  json += "\"led\":" + String(ledOn ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanOn ? "true" : "false") + ",";
  json += "\"time\":\"" + String(ts) + "\",";
  json += "\"ssid\":\"" + (wifiConnected ? WiFi.SSID() : String("none")) + "\",";
  json += "\"rssi\":" + String(wifiConnected ? WiFi.RSSI() : 0) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"ml\":" + String((int)reservoirMl()) + ",";
  json += "\"pumpSec\":" + String(pumpMsSinceRefill / 1000) + ",";
  json += "\"logBytes\":" + String((unsigned long)logBytes) + ",";
  json += "\"fsUsed\":" + String((unsigned long)LittleFS.usedBytes()) + ",";
  json += "\"fsTotal\":" + String((unsigned long)LittleFS.totalBytes()) + ",";
  json += "\"cfg\":{";
  json += "\"cap\":" + String(RESERVOIR_ML, 0) + ",";
  json += "\"mlPerSec\":" + String(PUMP_ML_PER_SEC, 1) + ",";
  json += "\"pOn\":" + String(PUMP_ON_BELOW, 0) + ",";
  json += "\"pOff\":" + String(PUMP_OFF_ABOVE, 0) + ",";
  json += "\"lOn\":" + String(LED_ON_ABOVE, 0) + ",";
  json += "\"lOff\":" + String(LED_OFF_BELOW, 0) + ",";
  json += "\"fOn\":" + String(FAN_ON_ABOVE, 0) + ",";
  json += "\"fOff\":" + String(FAN_OFF_BELOW, 0);
  json += "}}";
  server.send(200, "application/json", json);
}

void handleHistory() {
  String json;
  json.reserve(14000);
  json = "[";
  char buf[192];
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    Reading &r = history[idx];
    snprintf(buf, sizeof(buf),
             "%s{\"ts\":\"%s\",\"temp\":%.1f,\"hum\":%.1f,\"soil\":%d,\"light\":%d,\"pump\":%d,\"led\":%d,\"fan\":%d,\"ml\":%d}",
             i ? "," : "", r.ts, r.temp, r.hum, r.soil, r.light,
             r.pump ? 1 : 0, r.led ? 1 : 0, r.fan ? 1 : 0, r.ml);
    json += buf;
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleDownload() {
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) {
    server.send(404, "text/plain", "No log yet - wait for the first reading to be written.");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=agrisense_log.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

void handleFlush() {
  createLogFile();   // overwrites with just the header
  server.send(200, "text/plain", "flushed");
}

void handleRefilled() {
  pumpMsSinceRefill = 0;
  pumpMsSaved = 0;
  prefs.putULong("pumpMs", 0);
  server.send(200, "text/plain", "ok");
}

// ================= WIFI + NTP =================
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI1_SSID, WIFI1_PASS);
  wifiMulti.addAP(WIFI2_SSID, WIFI2_PASS);

  int dots = 0;
  while (wifiMulti.run(8000) != WL_CONNECTED) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    for (int i = 0; i <= dots % 4; i++) lcd.print(".");
    dots++;
    Serial.print(".");
  }
  wifiConnected = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.SSID().substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  Serial.println();
  Serial.print("Connected to ");
  Serial.print(WiFi.SSID());
  Serial.print("  IP: ");
  Serial.println(WiFi.localIP());
  delay(2500);
}

void syncTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Syncing time...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo, 1000)) {
    attempts++;
    lcd.setCursor(0, 1);
    lcd.print("Attempt ");
    lcd.print(attempts);
    if (attempts > 30) {
      lcd.setCursor(0, 1);
      lcd.print("Retrying NTP... ");
    }
  }
  timeSynced = true;

  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time Synced");
  lcd.setCursor(0, 1);
  lcd.print(buf);
  delay(2000);
}

void checkWiFiAndReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("WiFi dropped, trying known networks...");
    if (wifiMulti.run(3000) == WL_CONNECTED) wifiConnected = true;
  } else {
    wifiConnected = true;
  }
}

// ================= LCD + TOUCH =================
void printLine(uint8_t row, const char* s) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", s);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void drawScreen() {
  char l0[40], l1[40];
  switch (displayScreen) {
    case 0:
      snprintf(l0, sizeof(l0), "Temp: %.1f C", currentTemp);
      snprintf(l1, sizeof(l1), "Hum: %.1f %%", currentHum);
      break;
    case 1:
      snprintf(l0, sizeof(l0), "Soil: %d%%", soilPercent);
      snprintf(l1, sizeof(l1), "Light: %d%%", lightPercent);
      break;
    case 2:
      snprintf(l0, sizeof(l0), "Pump:%s LED:%s", pumpOn ? "ON " : "OFF", ledOn ? "ON" : "OFF");
      snprintf(l1, sizeof(l1), "Fan: %s", fanOn ? "ON" : "OFF");
      break;
    case 3:
      snprintf(l0, sizeof(l0), "Tank: %d ml", (int)reservoirMl());
      snprintf(l1, sizeof(l1), "%d%% left", (int)(reservoirMl() * 100.0f / RESERVOIR_ML));
      break;
    default:
      if (wifiConnected) snprintf(l0, sizeof(l0), "%s", WiFi.SSID().c_str());
      else snprintf(l0, sizeof(l0), "WiFi: DOWN");
      snprintf(l1, sizeof(l1), "%s", WiFi.localIP().toString().c_str());
      break;
  }
  printLine(0, l0);
  printLine(1, l1);
}

void handleTouch() {
  bool t = (digitalRead(touchPin) == (TOUCH_ACTIVE_HIGH ? HIGH : LOW));
  if (t && !touchLast && millis() - lastTouchMs > 250) {
    lastTouchMs = millis();
    displayScreen = (displayScreen + 1) % NUM_SCREENS;
    lcd.clear();
    drawScreen();
  }
  touchLast = t;
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(0, 1);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AgriSense Boot");
  delay(1000);

  dht.begin();
  pinMode(soilDigitalPin, INPUT_PULLUP);
  pinMode(soilAnalogPin, INPUT);
  pinMode(ldrAnalogPin, INPUT);
  pinMode(touchPin, TOUCH_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);

  pinMode(pumpPin, OUTPUT); digitalWrite(pumpPin, LOW);
  pinMode(ledPin, OUTPUT);  digitalWrite(ledPin, LOW);
  pinMode(fanPin, OUTPUT);  digitalWrite(fanPin, LOW);

  prefs.begin("agrisense", false);
  pumpMsSinceRefill = prefs.getULong("pumpMs", 0);
  pumpMsSaved = pumpMsSinceRefill;

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
  } else {
    ensureLogFile();
  }

  connectWiFi();
  syncTime();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/download", handleDownload);
  server.on("/flush", HTTP_POST, handleFlush);
  server.on("/refilled", HTTP_POST, handleRefilled);
  server.enableCORS(true);
  server.begin();

  lcd.clear();
  drawScreen();
}

void loop() {
  server.handleClient();
  handleTouch();

  unsigned long currentMillis = millis();

  // WiFi health check every 10s
  if (currentMillis - previousWifiCheckMillis >= 10000) {
    previousWifiCheckMillis = currentMillis;
    checkWiFiAndReconnect();
  }

  // Sensor reads + actuator control, every 2s
  if (currentMillis - previousSensorMillis >= 2000) {
    unsigned long dt = currentMillis - previousSensorMillis;
    previousSensorMillis = currentMillis;

    if (pumpOn) pumpMsSinceRefill += dt;   // pump state from the previous cycle

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t;
    if (!isnan(h)) currentHum = h;

    int rawSoil = analogRead(soilAnalogPin);
    soilPercent = map(rawSoil, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);

    int rawLight = analogRead(ldrAnalogPin);
    lightPercent = map(rawLight, LIGHT_RAW_DARK, LIGHT_RAW_BRIGHT, 0, 100);
    lightPercent = constrain(lightPercent, 0, 100);

    if (soilPercent < PUMP_ON_BELOW) pumpOn = true;
    else if (soilPercent > PUMP_OFF_ABOVE) pumpOn = false;
    digitalWrite(pumpPin, pumpOn ? HIGH : LOW);

    if (lightPercent > LED_ON_ABOVE) ledOn = true;
    else if (lightPercent < LED_OFF_BELOW) ledOn = false;
    digitalWrite(ledPin, ledOn ? HIGH : LOW);

    if (currentHum > FAN_ON_ABOVE) fanOn = true;
    else if (currentHum < FAN_OFF_BELOW) fanOn = false;
    digitalWrite(fanPin, fanOn ? HIGH : LOW);
  }

  // CSV logging every 60s, independent of any browser
  if (currentMillis - previousLogMillis >= LOG_INTERVAL_MS) {
    previousLogMillis = currentMillis;
    logReading();
  }

  // Save pump run-time every 5 min (only if changed) to limit flash writes
  if (pumpMsSinceRefill != pumpMsSaved && currentMillis - lastPrefsSave >= 300000) {
    lastPrefsSave = currentMillis;
    prefs.putULong("pumpMs", pumpMsSinceRefill);
    pumpMsSaved = pumpMsSinceRefill;
  }

  // LCD refresh (screen changes only via touch sensor)
  if (currentMillis - previousDrawMillis >= 250) {
    previousDrawMillis = currentMillis;
    drawScreen();
  }

  delay(5);
}
