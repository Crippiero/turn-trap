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
  Serial.begin(115200);   // PC DEBUG
  UNO.begin(9600);        // COMUNICAZIONE CON UNO

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/reset", handleReset);
  server.on("/getLog", handleGetLog);

  server.begin();

  pinMode(5, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  
  Serial.println("\nESP Pronto. WiFi creato: ESP_CONTROL");
}

// ---------------- LOOP ----------------

void loop() {
  server.handleClient();

  // Pulsanti fisici ESP
  if (digitalRead(5) == LOW) { handleStart(); delay(200); }
  if (digitalRead(4) == LOW) { handleReset(); delay(200); }

  // 🔥 FORWARD: Lettura massiva per evitare frammentazione RAM
  if (UNO.available()) {
    String chunk = UNO.readString(); // Legge tutto quello che c'è nel buffer in un colpo solo
    Serial.print(chunk);       
    serialBuffer += chunk;     
  }

  // Limite di sicurezza
  if (serialBuffer.length() > 2000) {
    serialBuffer = serialBuffer.substring(serialBuffer.length() - 1000);
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
          
          .data-box { margin: 20px auto; padding: 15px; width: 90%; max-width: 400px; background: #fff; border-left: 5px solid #2196F3; font-weight: bold; box-shadow: 0px 2px 5px rgba(0,0,0,0.1); text-align: left; }
          
          .maze-container { margin: 20px auto; background-color: #222; padding: 15px; display: inline-block; border-radius: 8px; box-shadow: 0px 4px 10px rgba(0,0,0,0.3); }
          .row { display: flex; justify-content: center; }
          .cell { width: 22px; height: 22px; margin: 1px; border-radius: 3px; position: relative; }
          
          textarea { width: 90%; max-width: 500px; height: 100px; font-family: monospace; font-size: 12px; margin-top: 20px; padding: 10px; border-radius: 5px; border: 1px solid #ccc; }
        </style>
      </head>
      <body>
        <h1>ESP Control Panel</h1>

        <div>
          <button class="btn btn-start" onclick="fetch('/start')">START</button>
          <button class="btn btn-reset" onclick="fetch('/reset')">RESET</button>
        </div>

        <div id="dataBox" class="data-box">Attendere rilevamento magneti...</div>

        <div id="mazeContainer" class="maze-container">
          <div style="color: #aaa; padding: 20px;">In attesa dei dati...</div>
        </div>

        <br>
        <textarea id="logBox" readonly placeholder="Log Seriale Grezzo..."></textarea>

        <script>
          let packetBuffer = ""; // Buffer Javascript per ricostruire i pacchetti
          let mazeData = [];
          let magnetsInfo = ["", "", "", ""];
          let rows = 11; // Imposta le righe totali previste

          // Inizializza array labirinto vuoto
          for(let i=0; i<rows; i++) mazeData.push(""); 

          setInterval(function() {
            fetch('/getLog')
              .then(response => response.text())
              .then(data => {
                if(data.length > 0) {
                  // Aggiungo al box dei log
                  let logBox = document.getElementById('logBox');
                  logBox.value += data;
                  if(logBox.value.length > 5000) logBox.value = logBox.value.substring(logBox.value.length - 2000);
                  logBox.scrollTop = logBox.scrollHeight;
                  
                  // Analizzo i pacchetti
                  packetBuffer += data;
                  parsePackets();
                }
              });
          }, 500); // 500ms per un aggiornamento più fluido

          function parsePackets() {
            // Cerca pacchetti racchiusi tra < e >
            while(true) {
              let startIdx = packetBuffer.indexOf('<');
              let endIdx = packetBuffer.indexOf('>');
              
              if (startIdx !== -1 && endIdx !== -1 && endIdx > startIdx) {
                // Estraggo il pacchetto senza le parentesi
                let packet = packetBuffer.substring(startIdx + 1, endIdx);
                processCommand(packet);
                
                // Rimuovo il pacchetto processato dal buffer
                packetBuffer = packetBuffer.substring(endIdx + 1);
              } else if (startIdx !== -1 && endIdx === -1) {
                // Ho un pacchetto a metà, aspetto il prossimo ciclo di fetch
                break;
              } else {
                // Nessun pacchetto trovato, pulisco la spazzatura eventuale
                packetBuffer = "";
                break;
              }
            }
          }

          function processCommand(cmdStr) {
            let parts = cmdStr.split(',');
            let type = parts[0];

            if (type === 'R' && parts.length >= 3) {
              // Riga: <R, riga, caratteri>
              let rIdx = parseInt(parts[1]);
              mazeData[rIdx] = parts[2];
            } 
            else if (type === 'M' && parts.length >= 5) {
              // Magnete: <M, id, x, y, forza>
              let id = parseInt(parts[1]);
              magnetsInfo[id] = `Magnete ${id}: Pos [${parts[2]}, ${parts[3]}] - Forza: ${parts[4]}`;
              updateMagnetBox();
            }
            else if (type === 'C') {
              // Clear
              mazeData = [];
              for(let i=0; i<rows; i++) mazeData.push("");
              magnetsInfo = ["", "", "", ""];
              updateMagnetBox();
            }
            else if (type === 'S') {
              // Show: Renderizza il labirinto
              renderMaze();
            }
          }

          function updateMagnetBox() {
            let box = document.getElementById('dataBox');
            let txt = magnetsInfo.filter(m => m !== "").join('<br>');
            box.innerHTML = txt === "" ? "Nessun magnete rilevato." : txt;
          }

          function renderMaze() {
            let container = document.getElementById('mazeContainer');
            container.innerHTML = '';
            
            mazeData.forEach(lineStr => {
              if(!lineStr) return; // Salta righe vuote
              
              let rowDiv = document.createElement('div');
              rowDiv.className = 'row';
              
              let chars = lineStr.split('');
              chars.forEach(c => {
                let cell = document.createElement('div');
                cell.className = 'cell';
                
                // COLORI
                if (c === '#') cell.style.backgroundColor = '#ff4d4d'; // Rosso
                else if (c === 'x' || c === 'E') cell.style.backgroundColor = '#ffffff'; // Bianco
                else if (c === 'G') cell.style.backgroundColor = '#9b59b6'; // Viola
                else if (c === 'U' || c === 'B') cell.style.backgroundColor = '#f1c40f'; // Giallo/Bonus
                else if (c === 'M' || c === 'W') cell.style.backgroundColor = '#e67e22'; // Arancio/Malus
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
  serialBuffer = ""; // Svuota il buffer dopo l'invio
}

void handleStart() {
  sendPacket(CMD_START, nullptr, 0);
  server.send(200, "text/plain", "START sent");
}

void handleReset() {
  sendPacket(CMD_RESET, nullptr, 0);
  server.send(200, "text/plain", "RESET sent");
}

// ---------------- BINARY PROTOCOL (Web -> Master) ----------------

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