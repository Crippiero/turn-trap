#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#define START_BYTE 0xAA

// ESP <-> Arduino UNO
SoftwareSerial UNO(14, 12); // D5 = RX, D6 = TX

enum Command {
  CMD_START = 0x01,
  CMD_RESET = 0x02,
  CMD_START_RESET = 0x03, // 🔥 Nuovo comando Arduino -> ESP
  CMD_END_RESET = 0x04    // 🔥 Nuovo comando Arduino -> ESP
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

  // 🔥 MACCHINA A STATI: Rilevamento pacchetti binari o testo ASCII
  static enum { RX_IDLE, RX_CMD, RX_LEN, RX_DATA, RX_CHK } rxState = RX_IDLE;
  static uint8_t rxCmd = 0;
  static uint8_t rxLen = 0;
  static uint8_t rxCalcChk = 0;
  static uint8_t rxIndex = 0;

  while (UNO.available()) {
    uint8_t c = UNO.read();

    if (rxState == RX_IDLE) {
      if (c == START_BYTE) {
        // Trovato l'inizio di un pacchetto binario
        rxState = RX_CMD;
        rxCalcChk = 0;
      } else {
        // Non è un pacchetto, trattalo come normale output testuale (Labirinto/Log)
        Serial.write((char)c);       
        serialBuffer += (char)c;     
      }
    } 
    else if (rxState == RX_CMD) {
      rxCmd = c;
      rxCalcChk ^= c;
      rxState = RX_LEN;
    } 
    else if (rxState == RX_LEN) {
      rxLen = c;
      rxCalcChk ^= c;
      rxIndex = 0;
      if (rxLen > 0) rxState = RX_DATA;
      else rxState = RX_CHK;
    } 
    else if (rxState == RX_DATA) {
      // Consuma i byte di dati (anche se nel tuo caso la len è 0, lo mettiamo per robustezza)
      rxCalcChk ^= c;
      rxIndex++;
      if (rxIndex >= rxLen) rxState = RX_CHK;
    } 
    else if (rxState == RX_CHK) {
      if (c == rxCalcChk) {
        // PACCHETTO RICEVUTO CORRETTAMENTE DA ARDUINO!
        if (rxCmd == CMD_START_RESET) {
          serialBuffer += "\n[[START_RESET]]\n"; // Segnale per la WebPage
          Serial.println("\n[DEBUG] Ricevuto CMD_START_RESET da UNO");
        } else if (rxCmd == CMD_END_RESET) {
          serialBuffer += "\n[[END_RESET]]\n";   // Segnale per la WebPage
          Serial.println("\n[DEBUG] Ricevuto CMD_END_RESET da UNO");
        }
      }
      rxState = RX_IDLE; // Torna in ascolto
    }
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
            background-color: #121212; 
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
            box-shadow: inset 0 0 3px rgba(0,0,0,0.4); 
          }
          
          #debugSection {
            display: none; 
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
            color: #0f0; 
            resize: none;
            box-shadow: inset 0px 0px 8px rgba(0,255,0,0.1);
          }

          /* 🔥 STILI PER L'OVERLAY DI AVVISO */
          .overlay {
            position: fixed; 
            top: 0; left: 0; 
            width: 100%; height: 100%;
            background: rgba(0, 0, 0, 0.85); 
            z-index: 9999;
            display: none; /* Nascosto di default */
            justify-content: center; 
            align-items: center;
            text-align: center; 
            color: white;
            backdrop-filter: blur(5px);
          }
          .overlay-content {
            background: #d32f2f; 
            padding: 40px; 
            border-radius: 15px;
            border: 4px solid #fff; 
            max-width: 80%;
            box-shadow: 0 0 30px rgba(255, 0, 0, 0.5);
            animation: pulse 1.5s infinite alternate;
          }
          .overlay h2 { margin-top: 0; font-size: 32px; color: #fff;}
          .overlay p { font-size: 22px; font-weight: bold; margin-bottom: 10px;}
          
          @keyframes pulse {
            from { transform: scale(1); }
            to { transform: scale(1.02); }
          }
        </style>
      </head>
      <body>
        
        <div id="resetOverlay" class="overlay">
           <div class="overlay-content">
             <h2>⚠️ ATTENZIONE ⚠️</h2>
             <p>Macchina in fase di movimento e ripristino...</p>
             <p>Rimuovere immediatamente le pedine!</p>
             <p style="color: #ffeb3b; font-size: 26px;">NON TOCCARE IL LABIRINTO</p>
           </div>
        </div>

        <h1>ESP Control Panel</h1>

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
              
              // 🔥 GESTIONE SEGNALI BINARI CONVERTITI
              if (line === "[[START_RESET]]") {
                document.getElementById('resetOverlay').style.display = 'flex';
                continue;
              }
              if (line === "[[END_RESET]]") {
                document.getElementById('resetOverlay').style.display = 'none';
                continue;
              }
              
              // Logica Labirinto e dati
              if (line.length === 0) {
                if (tempMazeLines.length === 11) {
                  latestMazeLines = [...tempMazeLines]; 
                }
                tempMazeLines = []; 
                continue;
              }
              
              if (/^[x#G\s]+$/.test(line)) {
                tempMazeLines.push(line);
                if (tempMazeLines.length === 11) {
                  latestMazeLines = [...tempMazeLines];
                }
              } else {
                extraDataLines.push(line);
              }
            }
            
            if (latestMazeLines.length === 0 && tempMazeLines.length > 0) {
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