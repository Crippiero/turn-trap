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

const char* ssid = "Turn&Trap";
const char* password = "12345678";

ESP8266WebServer server(80);

// Buffer per accumulare i messaggi dalla seriale
String serialBuffer = "";

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(57600);   // PC DEBUG + OUTPUT ARDUINO
  UNO.begin(57600);        // COMUNICAZIONE CON UNO

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/reset", handleReset);
  server.on("/getLog", handleGetLog);

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

  // 🔥 FORWARD: Arduino UNO → PC e accumulo nel buffer Web
  while (UNO.available()) {
    char c = UNO.read();
    Serial.write(c);       
    serialBuffer += c;     
  }

  // Limite di sicurezza per non riempire la RAM dell'ESP
  if (serialBuffer.length() > 1000) {
    serialBuffer = serialBuffer.substring(serialBuffer.length() - 500);
  }
}

// ---------------- WEB PAGE ----------------

// ---------------- WEB PAGE ----------------

void handleRoot() {
  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; background-color: #eef2f3; }
          .btn { padding: 15px 30px; font-size: 18px; margin: 10px; cursor: pointer; border-radius: 5px; border: none; font-weight: bold; }
          .btn-start { background-color: #4CAF50; color: white; }
          .btn-reset { background-color: #f44336; color: white; }
          
          .data-box { margin: 20px auto; padding: 15px; width: 90%; max-width: 400px; background: #fff; border-left: 5px solid #2196F3; font-weight: bold; display: none; box-shadow: 0px 2px 5px rgba(0,0,0,0.1); text-align: left; white-space: pre-line;}
          
          .maze-container { margin: 20px auto; background-color: #333; padding: 15px; display: inline-block; border-radius: 8px; box-shadow: 0px 4px 10px rgba(0,0,0,0.3); }
          .row { display: flex; justify-content: center; }
          .cell { width: 22px; height: 22px; margin: 1px; border-radius: 3px; }
          
          textarea { width: 90%; max-width: 500px; height: 100px; font-family: monospace; font-size: 12px; margin-top: 20px; padding: 10px; border-radius: 5px; border: 1px solid #ccc; }
        </style>
      </head>
      <body>
        <h1>ESP Control Panel</h1>

        <div>
          <button class="btn btn-start" onclick="fetch('/start')">START</button>
          <button class="btn btn-reset" onclick="fetch('/reset')">RESET</button>
        </div>

        <div id="dataBox" class="data-box">Nessun dato extra</div>

        <div id="mazeContainer" class="maze-container">
          <div style="color: #aaa; padding: 20px;">In attesa dei dati dalla seriale...</div>
        </div>

        <br>
        <textarea id="logBox" readonly placeholder="Log Seriale Grezzo..."></textarea>

        <script>
          let fullLog = "";
          let lastRenderedMaze = "";

          setInterval(function() {
            fetch('/getLog')
              .then(response => response.text())
              .then(data => {
                if(data.length > 0) {
                  fullLog += data;
                  
                  let logBox = document.getElementById('logBox');
                  logBox.value += data;
                  logBox.scrollTop = logBox.scrollHeight;
                  
                  if(fullLog.length > 15000) fullLog = fullLog.substring(fullLog.length - 5000);
                  
                  parseMazeAndData();
                }
              });
          }, 1000);

          function parseMazeAndData() {
            let lines = fullLog.split('\n');
            let latestMazeLines = [];
            let tempMazeLines = [];
            let extraDataLines = [];
            
            for(let i = 0; i < lines.length; i++) {
              let line = lines[i].trim();
              if (line.length === 0) continue;
              
              if (/^[x#G\s]+$/.test(line)) {
                tempMazeLines.push(line);
                
                // 🔥 NUOVA REGOLA: Se arriviamo a 11 righe, il labirinto è completo!
                if (tempMazeLines.length === 11) {
                  latestMazeLines = [...tempMazeLines]; // Salviamo il labirinto
                  tempMazeLines = [];                   // Resettiamo per il prossimo
                }
              } else {
                if (tempMazeLines.length > 0) {
                  latestMazeLines = [...tempMazeLines];
                  tempMazeLines = [];
                }
                extraDataLines.push(line);
              }
            }
            
            // Mostra anche labirinti a metà (se sta ancora caricando le righe 1-10)
            if (tempMazeLines.length > 0) {
              latestMazeLines = [...tempMazeLines];
            }

            let dataBox = document.getElementById('dataBox');
            if (extraDataLines.length > 0) {
              let recentData = extraDataLines.slice(-8).join('\n');
              dataBox.innerText = recentData;
              dataBox.style.display = 'block';
            } else {
              dataBox.style.display = 'none';
            }

            let currentMazeStr = latestMazeLines.join('|');
            if (latestMazeLines.length > 0 && currentMazeStr !== lastRenderedMaze) {
              lastRenderedMaze = currentMazeStr;
              renderMaze(latestMazeLines);
            }
          }

          function renderMaze(lines) {
            let container = document.getElementById('mazeContainer');
            container.innerHTML = ''; 
            
            lines.forEach(line => {
              let rowDiv = document.createElement('div');
              rowDiv.className = 'row';
              
              let chars = line.replace(/\s/g, '').split('');
              
              chars.forEach(c => {
                let cell = document.createElement('div');
                cell.className = 'cell';
                
                if (c === '#') cell.style.backgroundColor = '#ff4d4d'; 
                else if (c === 'x') cell.style.backgroundColor = '#ffffff'; 
                else if (c === 'G') cell.style.backgroundColor = '#9b59b6'; 
                else cell.style.backgroundColor = 'transparent'; 
                
                rowDiv.appendChild(cell);
              });
              container.appendChild(rowDiv);
            });
          }
        </script>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

// ---------------- COMMANDS E LOGS ----------------

void handleGetLog() {
  server.send(200, "text/plain", serialBuffer);
  serialBuffer = ""; 
}

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

  UNO.write(START_BYTE);
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