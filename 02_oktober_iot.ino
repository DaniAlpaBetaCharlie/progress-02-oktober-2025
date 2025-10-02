#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AntaresESPMQTT.h>
#include "DHT.h"

// ===================== KONFIGURASI =====================
#define ACCESSKEY     "1bb28641a37dc321:57c4c66930293a68"
#define PROJECT_NAME  "melnrh"
#define DEVICE_NAME   "nrh"

const char* ssid     = "m";
const char* password = "12345678";

// DHT di D2 (GPIO4) + DHT11
#define DHTPIN        D2
#define DHTTYPE       DHT11

#define PIR_PIN       D5
#define LED_PIN       D0   // LED eksternal

const unsigned long PUBLISH_INTERVAL = 5000;  // kirim ke Antares tiap 5 dtk
const unsigned long SENSOR_INTERVAL  = 3000;  // baca DHT tiap ≥3 dtk (lebih stabil)

// === Aturan LED berdasar suhu ===
const float TEMP_THRESHOLD = 30.0;            // ambang
const unsigned long BLINK_INTERVAL_MS = 500;  // interval kedip
// =======================================================

// ===================== OBJEK ===========================
AntaresESPMQTT antares(ACCESSKEY);
ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastPublish     = 0;
unsigned long lastSensorRead  = 0;
unsigned long lastBlinkToggle = 0;

int   motion_state = LOW;
float temperature  = NAN;   // nilai terakhir valid
float humidity     = NAN;
bool  ledState     = LOW;   // keadaan LED saat ini

// ===================== UTIL / WEB ======================
String htmlPage() {
  String page = F(
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>ESP8266 Status</title>"
    "<style>body{font-family:Helvetica;text-align:center;margin-top:40px;}"
    ".data{font-size:20px;margin:10px;}.on{color:green;font-weight:bold;}"
    ".off{color:red;font-weight:bold;}</style></head><body>"
    "<h1>NodeMCU Status</h1>"
  );
  page += "<p class='data'>WiFi: <b>" + String(ssid) + "</b></p>";
  page += "<p class='data'>IP Address: " + WiFi.localIP().toString() + "</p>";
  page += F(
    "<p class='data'>Koneksi: <span id='wifi'></span></p>"
    "<p class='data'>Suhu: <span id='temp'></span> °C</p>"
    "<p class='data'>Kelembapan: <span id='hum'></span> %</p>"
    "<p class='data'>PIR: <span id='pir'></span></p>"
    "<p class='data'>LED: <span id='led'></span></p>"
    "<script>"
    "async function update(){"
      "let res=await fetch('/status');"
      "let d=await res.json();"
      "document.getElementById('wifi').innerHTML=d.wifi;"
      "document.getElementById('temp').innerHTML=d.temp;"
      "document.getElementById('hum').innerHTML=d.hum;"
      "document.getElementById('pir').innerHTML=d.motion?'Gerakan Terdeteksi':'Tidak Ada Gerakan';"
      "document.getElementById('pir').className=d.motion?'on':'off';"
      "document.getElementById('led').innerHTML=d.led?'ON':'OFF';"
      "document.getElementById('led').className=d.led?'on':'off';"
    "}"
    "setInterval(update,1000);update();"
    "</script></body></html>"
  );
  return page;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleStatus() {
  String tempStr = isnan(temperature) ? String("NaN") : String(temperature, 1);
  String humStr  = isnan(humidity)    ? String("NaN") : String(humidity, 1);

  String json = "{";
  json += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "Terhubung" : "Terputus") + "\",";
  json += "\"temp\":" + tempStr + ",";
  json += "\"hum\":"  + humStr  + ",";
  json += "\"motion\":" + String(motion_state ? 1 : 0) + ",";
  json += "\"led\":" + String(ledState ? 1 : 0);
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ===================== WIFI ===========================
void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("\n[WiFi] Connecting to %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  uint8_t attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 60) { // ~12 detik
    delay(200);
    Serial.print('.');
    yield();
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected");
    Serial.print("[WiFi] IP   : "); Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI : "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  } else {
    Serial.println("\n[WiFi] Timeout (akan dicoba lagi otomatis).");
  }
}

// ===================== SETUP ==========================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== ESP8266 + Antares MQTT + DHT + PIR + Web ===");

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();
  pinMode(DHTPIN, INPUT_PULLUP); // bantu pull-up (tetap gunakan resistor 10k jika perlu)
  delay(2500);                   // waktu bangun sensor

  connectToWiFi();

  // Antares MQTT
  antares.setDebug(true);
  antares.setMqttServer();   // server default Antares

  // Web server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
}

// ===================== LOOP ===========================
void loop() {
  server.handleClient();                 // layani web UI
  antares.checkMqttConnection();         // jaga koneksi MQTT
  if (WiFi.status() != WL_CONNECTED) {   // auto reconnect WiFi
    connectToWiFi();
  }

  // Baca PIR (boleh sering)
  motion_state = digitalRead(PIR_PIN);

  // Baca DHT tiap >= SENSOR_INTERVAL
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
      // Serial.printf("DHT OK: %.1f C, %.1f %%\n", temperature, humidity);
    } else {
      Serial.println("⚠ DHT: gagal baca (akan coba lagi)");
    }
  }

  // === Kontrol LED berdasar suhu ===
  if (isnan(temperature)) {
    // Jika belum ada data valid: LED OFF
    if (ledState) { ledState = false; digitalWrite(LED_PIN, LOW); }
  } else {
    if (temperature >= TEMP_THRESHOLD) {
      // Suhu ≥ 25°C → LED OFF solid
      if (ledState) { ledState = false; digitalWrite(LED_PIN, LOW); }
    } else {
      
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
    }
  }

  // Kirim ke Antares tiap 5 detik (hanya publish suhu/kelembapan jika valid)
  if (millis() - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = millis();

    if (!isnan(temperature)) antares.add("temperature", temperature);
    if (!isnan(humidity))    antares.add("humidity",    humidity);
    antares.add("motion", motion_state);
    antares.add("led",    ledState);

    antares.publish(PROJECT_NAME, DEVICE_NAME);
    Serial.println("✅ Data terkirim ke Antares");
  }

  delay(10); // kecil agar loop responsif
}
