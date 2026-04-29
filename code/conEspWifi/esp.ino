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

// Buffer per accumulare i messaggi dalla seriale
String serialBuffer = "";

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);   // PC DEBUG + OUTPUT ARDUINO
  UNO.begin(9600);        // COMUNICAZIONE CON UNO

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
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; background-color: #eef2f3; }
          .btn { padding: 15px 30px; font-size: 18px; margin: 10px; cursor: pointer; border-radius: 5px; border: none; font-weight: bold; }
          .btn-start { background-color: #4CAF50; color: white; }
          .btn-reset { background-color: #f44336; color: white; }
          
          /* Stile per i Dati Extra */
          .data-box { margin: 20px auto; padding: 15px; width: 90%; max-width: 400px; background: #fff; border-left: 5px solid #2196F3; font-weight: bold; display: none; box-shadow: 0px 2px 5px rgba(0,0,0,0.1); text-align: left; white-space: pre-line;}
          
          /* Stile per il Labirinto */
          .maze-container { margin: 20px auto; background-color: #333; padding: 15px; display: inline-block; border-radius: 8px; box-shadow: 0px 4px 10px rgba(0,0,0,0.3); }
          .row { display: flex; justify-content: center; }
          .cell { width: 22px; height: 22px; margin: 1px; border-radius: 3px; }
          
          /* Stile per i Log grezzi */
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
                  
                  // Aggiorna il box dei log testuali
                  let logBox = document.getElementById('logBox');
                  logBox.value += data;
                  logBox.scrollTop = logBox.scrollHeight;
                  
                  // Evita che la variabile Javascript esploda in RAM
                  if(fullLog.length > 15000) fullLog = fullLog.substring(fullLog.length - 5000);
                  
                  parseMazeAndData();
                }
              });
          }, 1000);

          // Funzione "Magica" che interpreta il testo
          function parseMazeAndData() {
            let marker1 = "%------------%";
            let marker2 = "--- NUOVO LABIRINTO ---";
            
            // Cerca l'ultimo aggiornamento ricevuto (partendo dalla fine)
            let idx1 = fullLog.lastIndexOf(marker1);
            if (idx1 !== -1) {
              let idx2 = fullLog.indexOf(marker2, idx1);
              if (idx2 !== -1) {
                
                // 1. ESTRAI I DATI EXTRA
                // Prende tutto ciò che c'è tra "%----%" e "--- NUOVO LABIRINTO ---"
                let rawData = fullLog.substring(idx1 + marker1.length, idx2).trim();
                let dataBox = document.getElementById('dataBox');
                
                if (rawData.length > 0) {
                  dataBox.innerText = rawData;
                  dataBox.style.display = 'block';
                } else {
                  dataBox.style.display = 'none';
                }

                // 2. ESTRAI E DISEGNA IL LABIRINTO
                let mazeStr = fullLog.substring(idx2 + marker2.length).trim();
                let lines = mazeStr.split('\n');
                let mazeLines = [];
                
                // Filtra solo le righe che contengono i caratteri del labirinto
                for(let i = 0; i < lines.length; i++) {
                  let line = lines[i].trim();
                  if (/^[x#G\s]+$/.test(line) && line.length > 0) {
                    mazeLines.push(line);
                  } else if (mazeLines.length > 0) {
                    // Appena trova una riga che non c'entra niente, ferma la lettura del labirinto
                    break;
                  }
                }
                
                // Se c'è un labirinto ed è diverso dal precedente, lo disegna (evita sfarfallii)
                let currentMazeStr = mazeLines.join('|');
                if (mazeLines.length > 0 && currentMazeStr !== lastRenderedMaze) {
                  lastRenderedMaze = currentMazeStr;
                  renderMaze(mazeLines);
                }
              }
            }
          }

          function renderMaze(lines) {
            let container = document.getElementById('mazeContainer');
            container.innerHTML = ''; // Pulisce il contenitore
            
            lines.forEach(line => {
              let rowDiv = document.createElement('div');
              rowDiv.className = 'row';
              
              // Rimuove gli spazi e crea un array di caratteri
              let chars = line.replace(/\s/g, '').split('');
              
              chars.forEach(c => {
                let cell = document.createElement('div');
                cell.className = 'cell';
                
                // ASSEGNAZIONE COLORI
                if (c === '#') cell.style.backgroundColor = '#ff4d4d'; // Rosso
                else if (c === 'x') cell.style.backgroundColor = '#ffffff'; // Bianco
                else if (c === 'G') cell.style.backgroundColor = '#9b59b6'; // Viola
                else cell.style.backgroundColor = 'transparent'; // Sicurezza
                
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