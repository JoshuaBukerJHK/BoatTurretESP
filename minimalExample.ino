#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// =========================== DATA STRUCTURES ==========================

struct Motor {
  int PIN_1;
  int PIN_2;
  int EN;

  Motor(int PIN_1_, int PIN_2_, int EN_)
    : PIN_1(PIN_1_), PIN_2(PIN_2_), EN(EN_) {}
};  // <-- semicolon

// ============================ CONSTANT VALUES ==========================

const int HORIZONTAL_SERVO_PIN = 13;
const int HORIZONTAL_SERVO_MIN = 500;
const int HORIZONTAL_SERVO_MAX = 2400;

const int VERTICAL_SERVO_PIN = 14;
const int VERTICAL_SERVO_MIN = 500;
const int VERTICAL_SERVO_MAX = 2400;

const int LOADING_SERVO_PIN = 15;
const int LOADING_SERVO_MIN = 500;
const int LOADING_SERVO_MAX = 2400;

// PWM for DC motor enable pins
const int MOTOR_FREQ = 30000;         // 30 kHz
const int MOTOR_RESOLUTION = 8;       // 8-bit (0..255)
const int TOP_MOTOR_CH = 0;
const int BOTTOM_MOTOR_CH = 1;

const char* AP_SSID = "ESP32_Control";
const char* AP_PASSWORD = "12345678";

const char HTML_PAGE[] PROGMEM = R"rawliteral(
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

// ============================ HELPER FUNCTIONS ==========================

int mapToServoAngle(int input) {                // input: -100..100
  return constrain(map(input, -100, 100, 0, 180), 0, 180);
}

int mapToPWM(int input) {                       // input: 0..100
  return constrain(map(input, 0, 100, 0, 255), 0, 255);
}

bool inDeadband(int previous, int current) {
  return abs(current - previous) <= 2;
}

// ============================ LOOP VARIABLES ==========================

WebServer server(80);
WebSocketsServer webSocket(81);

Servo horizontal_servo;
Servo vertical_servo;
Servo loading_servo;

// Construct with pins at declaration (simpler/safer)
Motor top_motor(27, 26, 16);
// NOTE: Verify GPIO 24 is valid on your board; consider using 33 instead of 24 if 24 is unavailable.
Motor bottom_motor(25, 24, 17);

int current_horizontal_value = 0;  // -100..100
int current_vertical_value   = 0;  // -100..100

int previous_horizontal_value = 0;
int previous_vertical_value   = 0;

int powerValue      = 0;  // 0..100

bool fireEventTrigger = false;
bool fireEventActive = false;

// ============================ WEBSOCKET CONTROL ==========================

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type != WStype_TEXT) return;

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  if (doc.containsKey("axis")) {
    String axis = doc["axis"];
    int value   = doc["value"];

    if (axis == "horizontal") {
      previous_horizontal_value = current_horizontal_value;
      current_horizontal_value  = value;
    } else if (axis == "vertical") {
      previous_vertical_value = current_vertical_value;
      current_vertical_value  = value;
    } else if (axis == "power") {
      powerValue = constrain(value, 0, 100);
    }
  }

  if (doc.containsKey("action") && doc["action"] == "fire") {
    fireEventTrigger = true;
  }
}

// ============================ SETUP FUNCTIONS ==========================

void setupNetworking() {
  Serial.println("\nStarting ESP32 Access Point...");

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address: "); Serial.println(IP);

  server.on("/", []() { server.send_P(200, "text/html", HTML_PAGE); });
  server.begin();
  Serial.println("HTTP server started on port 80");

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void setupServos() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  horizontal_servo.setPeriodHertz(50);
  horizontal_servo.attach(HORIZONTAL_SERVO_PIN, HORIZONTAL_SERVO_MIN, HORIZONTAL_SERVO_MAX);

  vertical_servo.setPeriodHertz(50);
  vertical_servo.attach(VERTICAL_SERVO_PIN, VERTICAL_SERVO_MIN, VERTICAL_SERVO_MAX);

  loading_servo.setPeriodHertz(50);
  loading_servo.attach(LOADING_SERVO_PIN, LOADING_SERVO_MIN, LOADING_SERVO_MAX);
}

void setupMotorPins(const Motor& motor, uint8_t channel) {
  pinMode(motor.PIN_1, OUTPUT);
  pinMode(motor.PIN_2, OUTPUT);
  pinMode(motor.EN, OUTPUT);

  ledcSetup(channel, MOTOR_FREQ, MOTOR_RESOLUTION);
  ledcAttachPin(motor.EN, channel);
  ledcWrite(channel, 0); // start disabled
}

void setupSpeedController() {
  setupMotorPins(top_motor, TOP_MOTOR_CH);
  setupMotorPins(bottom_motor, BOTTOM_MOTOR_CH);
}

// ============================ MAIN PROGRAM ==========================

void setup() {
  Serial.begin(115200);
  setupNetworking();
  setupServos();
  setupSpeedController();
}

void loop() {
  server.handleClient();
  webSocket.loop();

  // Map -100..100 to 0..180 and honor deadband
  int hAngle = mapToServoAngle(current_horizontal_value);
  int vAngle = mapToServoAngle(current_vertical_value);

  if (!inDeadband(mapToServoAngle(previous_horizontal_value), hAngle)) {
    horizontal_servo.write(hAngle);
  }
  if (!inDeadband(mapToServoAngle(previous_vertical_value), vAngle)) {
    vertical_servo.write(vAngle);
  }

  // Apply power to both motors via EN PWM (direction logic is up to you)
  int duty = mapToPWM(powerValue);             // 0..255 for 8-bit
  ledcWrite(TOP_MOTOR_CH, duty);
  ledcWrite(BOTTOM_MOTOR_CH, duty);

  // TODO: Set motor direction pins (PIN_1/PIN_2) based on desired direction
  // digitalWrite(top_motor.PIN_1, LOW);
  // digitalWrite(top_motor.PIN_2, HIGH);
}
