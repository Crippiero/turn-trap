#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#define START_BYTE 0xAA

// ESP <-> Arduino UNO
SoftwareSerial UNO(14, 12); // D5 = RX, D6 = TX

enum Command {
  CMD_START = 0x01,
  CMD_RESET = 0x02
};

const char* ssid = "ESP_CONTROL";
const char* password = "12345678";

ESP8266WebServer server(80);

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);   // PC DEBUG + OUTPUT ARDUINO
  UNO.begin(9600);        // COMUNICAZIONE CON UNO

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/reset", handleReset);

  server.begin();

  pinMode(5, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
}

// ---------------- LOOP ----------------

void loop() {
  server.handleClient();

  // Pulsanti fisici ESP
  if (digitalRead(5) == LOW) {
    handleStart();
    delay(200);
  }

  if (digitalRead(4) == LOW) {
    handleReset();
    delay(200);
  }

  // 🔥 FORWARD: Arduino UNO → PC
  while (UNO.available()) {
    Serial.write(UNO.read());
  }
}

// ---------------- WEB PAGE ----------------

void handleRoot() {
  String page = R"rawliteral(
    <html>
      <body style="font-family:Arial;text-align:center;margin-top:50px;">
        <h1>ESP Control</h1>

        <button onclick="fetch('/start')" 
          style="padding:20px;font-size:20px;">START</button>

        <br><br>

        <button onclick="fetch('/reset')" 
          style="padding:20px;font-size:20px;">RESET</button>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

// ---------------- COMMANDS ----------------

void handleStart() {
  sendPacket(CMD_START, nullptr, 0);
  server.send(200, "text/plain", "START sent");
}

void handleReset() {
  sendPacket(CMD_RESET, nullptr, 0);
  server.send(200, "text/plain", "RESET sent");
}

// ---------------- BINARY PROTOCOL ----------------

void sendPacket(uint8_t cmd, uint8_t* data, uint8_t len) {
  uint8_t checksum = 0;

  UNO.write(START_BYTE);   // ❗ ERA Serial → ora corretto
  UNO.write(cmd);
  UNO.write(len);

  checksum ^= cmd;
  checksum ^= len;

  for (int i = 0; i < len; i++) {
    UNO.write(data[i]);
    checksum ^= data[i];
  }

  UNO.write(checksum);
}