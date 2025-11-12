#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

const char* ap_ssid = "ESP32_Control";
const char* ap_password = "12345678";

WebServer server(80);
WebSocketsServer webSocket(81);

// ------------------------
// HTML PAGE
// ------------------------
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Control</title>
</head>
<body>
  <h2>ESP32 Servo + Power Control</h2>

  <label>Horizontal (-100 to 100)</label>
  <input type="range" id="horizontal" min="-100" max="100" value="0" oninput="sendValue('horizontal', this.value)">

  <label>Vertical (-100 to 100)</label>
  <input type="range" id="vertical" min="-100" max="100" value="0" oninput="sendValue('vertical', this.value)">

  <label>Power (0 to 100)</label>
  <input type="range" id="power" min="0" max="100" value="0" oninput="sendValue('power', this.value)">

  <button onclick="sendFire()">FIRE</button>

  <script>
    const socket = new WebSocket(`ws://${window.location.hostname}:81`);
    function sendValue(axis, value) {
      socket.send(JSON.stringify({ axis: axis, value: parseInt(value) }));
    }
    function sendFire() {
      socket.send(JSON.stringify({ action: "fire" }));
    }
  </script>
</body>
</html>
)rawliteral";

// ------------------------
// Current values storage
// ------------------------
int horizontalValue = 0;  // -100..100
int verticalValue   = 0;  // -100..100
int powerValue      = 0;  // 0..100

// ------------------------
// WebSocket Event Handler
// ------------------------
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type != WStype_TEXT) return;

  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  // Axis updates
  if (doc.containsKey("axis")) {
    String axis = doc["axis"];
    int value  = doc["value"];

    if (axis == "horizontal") horizontalValue = value; // Pull for servo/controller
    else if (axis == "vertical") verticalValue = value; // Pull for servo/controller
    else if (axis == "power") powerValue = value;       // Pull for PWM/motor
  }

  // Fire event
  if (doc.containsKey("action") && doc["action"] == "fire") {
    // Trigger your "fire" routine here using current horizontalValue, verticalValue, powerValue
  }
}

// ------------------------
// Setup / Loop
// ------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting ESP32 Access Point...");

  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address: "); Serial.println(IP);

  server.on("/", []() { server.send_P(200, "text/html", htmlPage); });
  server.begin();
  Serial.println("HTTP server started on port 80");

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
