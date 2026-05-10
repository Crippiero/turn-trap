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
  Serial.begin(57600);     // PC DEBUG + OUTPUT ARDUINO
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

void handleRoot() {
  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
          body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            text-align: center; 
            margin: 0; 
            padding: 20px; 
            background-color: #121212; /* Tema scuro elegante */
            color: #ffffff; 
          }
          h1 { color: #4CAF50; font-weight: 600; margin-bottom: 30px;}
          
          .btn { 
            padding: 12px 24px; 
            font-size: 16px; 
            margin: 8px; 
            cursor: pointer; 
            border-radius: 8px; 
            border: none; 
            font-weight: bold; 
            color: white;
            transition: background-color 0.3s, transform 0.1s;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
          }
          .btn:active { transform: translateY(2px); }
          .btn-start { background-color: #2e7d32; }
          .btn-start:hover { background-color: #1b5e20; }
          .btn-reset { background-color: #d32f2f; }
          .btn-reset:hover { background-color: #b71c1c; }
          .btn-debug { background-color: #0277bd; }
          .btn-debug:hover { background-color: #01579b; }
          
          .data-box { 
            margin: 20px auto; 
            padding: 15px; 
            width: 90%; 
            max-width: 400px; 
            background: #1e1e1e; 
            border-left: 5px solid #0277bd; 
            font-weight: 500; 
            display: none; 
            box-shadow: 0px 4px 10px rgba(0,0,0,0.5); 
            text-align: left; 
            white-space: pre-line;
            border-radius: 6px;
            color: #e0e0e0;
          }
          
          .maze-container { 
            margin: 20px auto; 
            background-color: #1e1e1e; 
            padding: 20px; 
            display: inline-block; 
            border-radius: 12px; 
            box-shadow: 0px 8px 16px rgba(0,0,0,0.6); 
          }
          .row { display: flex; justify-content: center; }
          .cell { 
            width: 24px; 
            height: 24px; 
            margin: 2px; 
            border-radius: 4px; 
            box-shadow: inset 0 0 3px rgba(0,0,0,0.4); /* Effetto profondità */
          }
          
          #debugSection {
            display: none; /* Nascosto di default */
            margin-top: 30px;
          }
          
          textarea { 
            width: 90%; 
            max-width: 500px; 
            height: 120px; 
            font-family: 'Courier New', monospace; 
            font-size: 13px; 
            padding: 12px; 
            border-radius: 8px; 
            border: 1px solid #333; 
            background-color: #000; 
            color: #0f0; /* Stile terminale verde su nero */
            resize: none;
            box-shadow: inset 0px 0px 8px rgba(0,255,0,0.1);
          }
        </style>
      </head>
      <body>
        <h1>Turn&Trap</h1>

        <div>
          <button class="btn btn-start" onclick="fetch('/start')">START</button>
          <button class="btn btn-reset" onclick="fetch('/reset')">RESET</button>
          <button class="btn btn-debug" onclick="toggleDebug()">DEBUG MODE</button>
        </div>

        <div id="dataBox" class="data-box">Nessun dato extra</div>

        <div id="mazeContainer" class="maze-container">
          <div style="color: #888; padding: 20px;">In attesa dei dati dalla seriale...</div>
        </div>

        <div id="debugSection">
          <textarea id="logBox" readonly placeholder="Log Seriale Grezzo..."></textarea>
        </div>

        <script>
          let fullLog = "";
          let lastRenderedMaze = "";

          // Funzione per mostrare/nascondere il terminale
          function toggleDebug() {
            let debugSec = document.getElementById('debugSection');
            if (debugSec.style.display === "none" || debugSec.style.display === "") {
              debugSec.style.display = "block";
            } else {
              debugSec.style.display = "none";
            }
          }

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
              
              // 🔥 NUOVA LOGICA: Riga vuota = inizio nuovo blocco. Resettiamo l'array temporaneo.
              if (line.length === 0) {
                if (tempMazeLines.length === 11) {
                  latestMazeLines = [...tempMazeLines]; // Salva se era completo
                }
                tempMazeLines = []; // Svuota per accogliere il nuovo labirinto
                continue;
              }
              
              if (/^[x#G\s]+$/.test(line)) {
                tempMazeLines.push(line);
                
                // Se arriviamo a 11 righe, il labirinto è completo!
                if (tempMazeLines.length === 11) {
                  latestMazeLines = [...tempMazeLines];
                }
              } else {
                extraDataLines.push(line);
              }
            }
            
            // Mostra labirinti parziali solo se non abbiamo ancora un labirinto completo valido
            if (latestMazeLines.length === 0 && tempMazeLines.length > 0) {
              latestMazeLines = [...tempMazeLines];
            }

            // Gestione dei dati testuali
            let dataBox = document.getElementById('dataBox');
            if (extraDataLines.length > 0) {
              let recentData = extraDataLines.slice(-8).join('\n');
              dataBox.innerText = recentData;
              dataBox.style.display = 'block';
            } else {
              dataBox.style.display = 'none';
            }

            // Aggiornamento grafico del labirinto
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
                
                // I colori rimangono invariati rispetto alla tua richiesta
                if (c === '#') cell.style.backgroundColor = '#ff4d4d';       // Rosso
                else if (c === 'x') cell.style.backgroundColor = '#ffffff';  // Bianco
                else if (c === 'G') cell.style.backgroundColor = '#9b59b6';  // Viola
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