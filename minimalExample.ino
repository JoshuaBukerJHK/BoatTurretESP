#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
// #include <ESP32Servo.h>  // Uncomment when ready to use servos

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
  <title>ESP32 Axis Control</title>
  <style>
    body {
      font-family: sans-serif; text-align: center;
      margin-top: 30px; background: #f0f0f0;
    }
    h2 { margin-bottom: 20px; }
    .slider-container { margin: 20px auto; width: 80%; max-width: 400px; }
    label { display: block; font-size: 18px; margin-bottom: 8px; }
    input[type=range] {
      width: 100%; height: 12px; background: #ddd; border-radius: 6px; outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none;
      width: 24px; height: 24px; background: #2196F3; border-radius: 50%; cursor: pointer;
    }
    button {
      font-size: 18px; padding: 15px 40px; margin-top: 30px;
      background: #E53935; color: white; border: none; border-radius: 10px;
      cursor: pointer; box-shadow: 0 4px #b71c1c;
    }
    button:active { transform: translateY(2px); box-shadow: 0 2px #b71c1c; }
    #status { margin-top: 30px; font-weight: bold; }
  </style>
</head>
<body>
  <h2>ESP32 Servo + Power Control</h2>

  <div class="slider-container">
    <label for="horizontal">Horizontal (-100 to 100)</label>
    <input type="range" id="horizontal" min="-100" max="100" value="0" oninput="sendValue('horizontal', this.value)">
  </div>

  <div class="slider-container">
    <label for="vertical">Vertical (-100 to 100)</label>
    <input type="range" id="vertical" min="-100" max="100" value="0" oninput="sendValue('vertical', this.value)">
  </div>

  <div class="slider-container">
    <label for="power">Power (0 to 100)</label>
    <input type="range" id="power" min="0" max="100" value="0" oninput="sendValue('power', this.value)">
  </div>

  <button onclick="sendFire()">🔥 FIRE 🔥</button>

  <p id="status">Connecting...</p>

  <script>
    const statusText = document.getElementById("status");
    const socket = new WebSocket(`ws://${window.location.hostname}:81`);

    socket.onopen = () => statusText.innerText = "Connected ✅";
    socket.onclose = () => statusText.innerText = "Disconnected ❌";
    socket.onerror = () => statusText.innerText = "Error ⚠️";
    socket.onmessage = (e) => console.log("ESP32:", e.data);

    function sendValue(axis, value) {
      const data = JSON.stringify({ axis: axis, value: parseInt(value) });
      socket.send(data);
      console.log("Sent:", data);
    }

    function sendFire() {
      const data = JSON.stringify({ action: "fire" });
      socket.send(data);
      console.log("Sent:", data);
    }
  </script>
</body>
</html>
)rawliteral";

// ------------------------
// Servo / PWM Setup
// ------------------------

// #include <ESP32Servo.h>
// Servo servoH, servoV;
// const int servoHPin = 13;
// const int servoVPin = 12;
// const int powerPin = 14; // PWM pin for speed controller
// const int pwmChannel = 0;

// Current values
int horizontalValue = 0;
int verticalValue = 0;
int powerValue = 0;

// ------------------------
// Helper Functions
// ------------------------
int mapToServoAngle(int input) {
  return constrain(map(input, -100, 100, 0, 180), 0, 180);
}

int mapToPWM(int input) {
  return constrain(map(input, 0, 100, 0, 255), 0, 255);
}

void handleFireEvent() {
  Serial.println("🔥 FIRE command received! 🔥");
  Serial.printf("Current H: %d | V: %d | Power: %d\n", horizontalValue, verticalValue, powerValue);

  // --- Placeholder for your future actuator logic ---
  /*
  int hAngle = mapToServoAngle(horizontalValue);
  int vAngle = mapToServoAngle(verticalValue);
  int pwmVal = mapToPWM(powerValue);

  servoH.write(hAngle);
  servoV.write(vAngle);
  ledcWrite(pwmChannel, pwmVal);

  Serial.printf("Servos set -> H: %d°, V: %d°, Power PWM: %d\n", hAngle, vAngle, pwmVal);
  */
}

// ------------------------
// WebSocket Event Handler
// ------------------------
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("Client [%u] connected\n", num);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] disconnected\n", num);
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);
      Serial.printf("Received from [%u]: %s\n", num, msg.c_str());

      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, msg);
      if (err) return;

      if (doc.containsKey("axis")) {
        String axis = doc["axis"];
        int value = doc["value"];

        if (axis == "horizontal") {
          horizontalValue = value;
          Serial.printf("Horizontal set to %d\n", horizontalValue);
        } 
        else if (axis == "vertical") {
          verticalValue = value;
          Serial.printf("Vertical set to %d\n", verticalValue);
        } 
        else if (axis == "power") {
          powerValue = value;
          Serial.printf("Power set to %d\n", powerValue);
        }
      }

      if (doc.containsKey("action") && doc["action"] == "fire") {
        handleFireEvent();
      }
      break;
    }

    default:
      break;
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

  server.on("/", []() {
    server.send_P(200, "text/html", htmlPage);
  });
  server.begin();
  Serial.println("HTTP server started on port 80");

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");

  // --- Uncomment when ready to use hardware ---
  /*
  servoH.attach(servoHPin);
  servoV.attach(servoVPin);
  ledcSetup(pwmChannel, 5000, 8); // 5kHz, 8-bit resolution
  ledcAttachPin(powerPin, pwmChannel);
  */
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
