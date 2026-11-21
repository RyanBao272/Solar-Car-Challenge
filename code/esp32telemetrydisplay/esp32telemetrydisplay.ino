// http://192.168.4.1

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <U8g2lib.h>
#include <SPI.h>

const char* AP_SSID = "MotoTelemetry";
const char* AP_PASSWORD = "lol_nice_try";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 5, 4);
HardwareSerial TeensySerial(2);
String latestJSON = "{}";

void notifyClients() {
  ws.textAll(latestJSON);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(latestJSON);
  }
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Telemetry</title>
<style>
  body { background:#0b0d0f; color:#e8e6e1; font-family:-apple-system,sans-serif;
         text-align:center; padding-top:32px; margin:0; }
  .card { max-width:320px; margin:0 auto 16px; background:#14171a; border:1px solid #24282c;
          border-radius:12px; padding:16px 24px; display:flex; justify-content:space-between;
          align-items:baseline; }
  .label { font-size:13px; color:#7a8086; text-transform:uppercase; letter-spacing:0.04em; }
  .value { font-family:monospace; font-size:32px; color:#ff8a3d; }
</style>
</head>
<body>
  <div class="card"><span class="label">Motor speed</span><span class="value" id="rpm">-</span></div>
  <div class="card"><span class="label">Vehicle speed</span><span class="value" id="speed">-</span></div>
  <div class="card"><span class="label">Battery voltage</span><span class="value" id="voltage">-</span></div>
  <div class="card"><span class="label">Line current</span><span class="value" id="current">-</span></div>
  <div class="card"><span class="label">Battery percentage (est.)</span><span class="value" id="soc">-</span></div>
<script>
  var ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onmessage = function(event) {
    var d = JSON.parse(event.data);
    document.getElementById('rpm').textContent = Math.round(d.rpm || 0);
    document.getElementById('speed').textContent = (d.speed_kmh || 0).toFixed(0);
    document.getElementById('voltage').textContent = (d.voltage || 0).toFixed(1);
    document.getElementById('current').textContent = (d.line_current || 0).toFixed(1);
    document.getElementById('soc').textContent = (d.soc || 0).toFixed(0) + '%';
  };
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  TeensySerial.begin(115200, SERIAL_8N1, 16, 17);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("Access point IP: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.begin();

  u8g2.begin();
}

void loop() {
  static String buf = "";
  while (TeensySerial.available()) {
    char c = TeensySerial.read();
    if (c == '\n') {
      if (buf.length() > 5) {
        latestJSON = buf;
        notifyClients();
        updateLCD(buf);
      }
      buf = "";
    } else {
      buf += c;
    }
  }
  ws.cleanupClients();
}

void updateLCD(const String &json) {
  float rpm = extractFloat(json, "rpm");
  float voltage = extractFloat(json, "voltage");
  float current = extractFloat(json, "line_current");
  float speed = extractFloat(json, "speed_kmh");
  float soc = extractFloat(json, "soc");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.setCursor(0, 14);  u8g2.print("RPM  "); u8g2.print(rpm, 0);
  u8g2.setCursor(0, 32);  u8g2.print("Volt "); u8g2.print(voltage, 1);
  u8g2.setCursor(0, 50);  u8g2.print("Amp  "); u8g2.print(current, 1);
  u8g2.setCursor(70, 14); u8g2.print(speed, 0); u8g2.print("km/h");
  u8g2.setCursor(70, 32); u8g2.print("SOC "); u8g2.print(soc, 0); u8g2.print("%");
  u8g2.sendBuffer();
}

float extractFloat(const String &json, const String &key) {
  int idx = json.indexOf("\"" + key + "\":");
  if (idx == -1) return 0;
  idx = json.indexOf(':', idx) + 1;
  int end = json.indexOf(',', idx);
  if (end == -1) end = json.indexOf('}', idx);
  return json.substring(idx, end).toFloat();
}
