#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// --- CONFIGURAZIONE WIFI ---
const char* ssid = "Turn&Trap";
const char* password = "LCS";

// --- CONFIGURAZIONE PIN ---
// D1 (GPIO 5) e D2 (GPIO 4) per i pulsanti fisici
#define BTN_START 5 
#define BTN_RESET 4

// ESP <-> Arduino Master
// Rx = D5 (GPIO 14), Tx = D6 (GPIO 12)
SoftwareSerial UNO(14, 12); 

ESP8266WebServer server(80);

String serialBuffer = "";
unsigned long lastDebounceTime = 0;

// --- PAGINA HTML + CSS + JS ---
const char htmlPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Labirinto Magnetico - Regia</title>
  <style>
    :root {
      --bg-color: #0d0e15; --panel-bg: #1a1c29; --text-color: #ffffff;
      --accent: #00ffff; --wall: #ff2a2a; --empty: #2a2d3e;
      --goal: #b026ff; --bonus: #ffff00; --malus: #ff8c00; --player: #00e5ff;
    }
    body {
      margin: 0; padding: 20px; background-color: var(--bg-color); color: var(--text-color);
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      display: flex; flex-direction: column; align-items: center;
    }
    
    .header {
      width: 100%; max-width: 600px; display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 15px; border-bottom: 2px solid var(--panel-bg); padding-bottom: 10px;
    }
    .header h1 { margin: 0; font-size: 1.5rem; text-transform: uppercase; letter-spacing: 2px; }
    
    .switch-container { display: flex; align-items: center; gap: 10px; font-size: 0.9rem; color: #888; }
    .switch { position: relative; display: inline-block; width: 40px; height: 20px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #333; transition: .4s; border-radius: 20px; }
    .slider:before { position: absolute; content: ""; height: 14px; width: 14px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--accent); }
    input:checked + .slider:before { transform: translateX(20px); }

    .controls { width: 100%; max-width: 600px; display: flex; justify-content: center; gap: 15px; margin-bottom: 20px; }
    .btn { background: var(--panel-bg); color: var(--accent); border: 2px solid var(--accent); padding: 10px 20px; border-radius: 8px; cursor: pointer; text-transform: uppercase; font-weight: bold; letter-spacing: 1px; transition: all 0.3s ease; flex: 1; }
    .btn:hover { background: var(--accent); color: var(--bg-color); box-shadow: 0 0 15px var(--accent); }
    .btn-reset { border-color: var(--wall); color: var(--wall); }
    .btn-reset:hover { background: var(--wall); color: var(--text-color); box-shadow: 0 0 15px var(--wall); }

    #board-container { background: var(--panel-bg); padding: 15px; border-radius: 10px; box-shadow: 0 0 20px rgba(0,0,0,0.5); }
    #game-board { display: grid; grid-template-columns: repeat(11, 1fr); grid-template-rows: repeat(11, 1fr); gap: 2px; width: 90vw; max-width: 500px; aspect-ratio: 1/1; }
    .cell { background-color: var(--empty); border-radius: 3px; position: relative; transition: background-color 0.2s; }
    .cell.wall { background-color: var(--wall); box-shadow: 0 0 5px var(--wall); }
    .cell.goal { background-color: var(--goal); box-shadow: 0 0 10px var(--goal); }
    .cell.bonus { background-color: var(--bonus); box-shadow: 0 0 10px var(--bonus); }
    .cell.malus { background-color: var(--malus); box-shadow: 0 0 10px var(--malus); }
    
    .cell.player::after { content: ''; position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 50%; height: 50%; background-color: var(--player); border-radius: 50%; box-shadow: 0 0 8px var(--player); z-index: 10; }

    #debug-terminal { display: none; width: 100%; max-width: 600px; height: 150px; margin-top: 20px; padding: 10px; box-sizing: border-box; background-color: #000; color: #0f0; font-family: monospace; font-size: 0.8rem; border: 1px solid #333; border-radius: 5px; overflow-y: auto; }
    
    #toast-overlay { position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; background: rgba(0, 0, 0, 0.7); backdrop-filter: blur(3px); display: flex; justify-content: center; align-items: center; opacity: 0; pointer-events: none; transition: opacity 0.4s ease; z-index: 100; }
    #toast-overlay.show { opacity: 1; pointer-events: all;}
    #toast-box { background: var(--panel-bg); padding: 20px 40px; border-radius: 10px; border: 2px solid var(--accent); box-shadow: 0 0 30px rgba(0, 255, 255, 0.3); text-align: center; font-size: 1.5rem; text-transform: uppercase; letter-spacing: 1px; transform: scale(0.8); transition: transform 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275); }
    #toast-overlay.show #toast-box { transform: scale(1); }
  </style>
</head>
<body>

  <div class="header">
    <h1>Labyrinth OS</h1>
    <div class="switch-container">
      <span>Debug</span>
      <label class="switch">
        <input type="checkbox" id="debugToggle" onchange="toggleDebug()">
        <span class="slider"></span>
      </label>
    </div>
  </div>

  <div class="controls">
    <button class="btn" onclick="sendCommand('start')">Start Game</button>
    <button class="btn btn-reset" onclick="sendCommand('reset')">Reset Sensori</button>
  </div>

  <div id="board-container">
    <div id="game-board"></div>
  </div>

  <div id="debug-terminal"></div>

  <div id="toast-overlay">
    <div id="toast-box">Attendere...</div>
  </div>

  <script>
    const ROWS = 11; const COLS = 11;
    const board = document.getElementById('game-board');
    const terminal = document.getElementById('debug-terminal');
    const toastOverlay = document.getElementById('toast-overlay');
    const toastBox = document.getElementById('toast-box');
    let cells = []; let players = {}; let toastTimeout;
    let streamBuffer = ""; // Accumula i dati testuali in arrivo

    for (let i = 0; i < ROWS * COLS; i++) {
      let cell = document.createElement('div');
      cell.className = 'cell';
      board.appendChild(cell);
      cells.push(cell);
    }

    function toggleDebug() { terminal.style.display = document.getElementById('debugToggle').checked ? 'block' : 'none'; }
    function logDebug(msg) { if(document.getElementById('debugToggle').checked) { terminal.innerHTML += msg + '<br>'; terminal.scrollTop = terminal.scrollHeight; } }

    function showToast(message) {
      toastBox.innerText = message;
      toastOverlay.classList.add('show');
      clearTimeout(toastTimeout);
      toastTimeout = setTimeout(() => { toastOverlay.classList.remove('show'); }, 3000);
    }

    function getCell(x, y) { if (x < 0 || x >= ROWS || y < 0 || y >= COLS) return null; return cells[x * COLS + y]; }

    function updatePlayersOnBoard() {
      cells.forEach(c => c.classList.remove('player'));
      for (let id in players) {
        let p = players[id];
        let cell = getCell(p.x, p.y);
        if (cell) cell.classList.add('player');
      }
    }

    function sendCommand(endpoint) {
      fetch('/' + endpoint).then(() => logDebug("Comando " + endpoint + " inviato."));
    }

    function processPacket(payload) {
      logDebug("Ric: <" + payload + ">");
      let parts = payload.split(',');
      let cmd = parts[0];

      if (cmd === 'R') {
        let rIdx = parseInt(parts[1]); let rowData = parts[2];
        for (let c = 0; c < rowData.length; c++) {
          let cell = getCell(rIdx, c); if (!cell) continue;
          cell.className = 'cell'; 
          let char = rowData[c];
          if (char === '#') cell.classList.add('wall');
          else if (char === 'G') cell.classList.add('goal');
        }
      } 
      else if (cmd === 'U') {
        let x = parseInt(parts[1]); let y = parseInt(parts[2]); let color = parts[3];
        let cell = getCell(x, y);
        if (cell) {
          cell.classList.remove('bonus', 'malus');
          if (color === 'B' || color === 'U') cell.classList.add('bonus');
          else if (color === 'M' || color === 'W') cell.classList.add('malus');
          else if (color === 'E' || color === 'x') cell.className = 'cell';
        }
      } 
      else if (cmd === 'M') {
        let id = parts[1]; let x = parseInt(parts[2]); let y = parseInt(parts[3]); let intensity = parseInt(parts[4]);
        if (intensity > 0 && x !== -1) players[id] = {x: x, y: y};
        else delete players[id]; 
      }
      else if (cmd === 'T') { showToast(payload.substring(2)); }
      else if (cmd === 'C') { cells.forEach(c => c.className = 'cell'); players = {}; }
      else if (cmd === 'S') { updatePlayersOnBoard(); }
    }

    // --- CICLO DI LETTURA (POLLING COME NEL VECCHIO CODICE) ---
    // Legge ogni 250ms per avere un gioco fluido senza bloccare il browser
    setInterval(function() {
      fetch('/getLog')
        .then(response => response.text())
        .then(data => {
          if(data.length > 0) {
             streamBuffer += data;
             
             // Estrae e processa tutti i pacchetti completi compresi tra < e >
             let endIdx;
             while((endIdx = streamBuffer.indexOf('>')) !== -1) {
                let startIdx = streamBuffer.indexOf('<');
                if (startIdx !== -1 && startIdx < endIdx) {
                   let payload = streamBuffer.substring(startIdx + 1, endIdx);
                   processPacket(payload);
                }
                // Rimuove la parte processata dal buffer
                streamBuffer = streamBuffer.substring(endIdx + 1);
             }
             
             // Sicurezza: se il buffer si sporca troppo lo svuota
             if (streamBuffer.length > 500) streamBuffer = "";
          }
        })
        .catch(err => console.log("Errore Fetch", err));
    }, 250); 
  </script>
</body>
</html>
)=====";


// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200); // Debug PC (opzionale)
  UNO.begin(9600);      // Comunicazione via D5/D6 con Master
  
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);

  WiFi.softAP(ssid, password);

  // Endpoint del Server
  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  
  // Endpoint dei Pulsanti Web
  server.on("/start", []() {
    UNO.print("<START>");
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/reset", []() {
    UNO.print("<RESET>");
    server.send(200, "text/plain", "OK");
  });
  
  // Endpoint per leggere i dati ricevuti da Arduino Master
  server.on("/getLog", []() {
    server.send(200, "text/plain", serialBuffer);
    serialBuffer = ""; // Svuota il buffer dell'ESP dopo averlo spedito al Web
  });

  server.begin();
}


// ---------------- LOOP ----------------
void loop() {
  server.handleClient();

  // Lettura Pulsanti Fisici ESP
  if (millis() - lastDebounceTime > 200) { 
    if (digitalRead(BTN_START) == LOW) {
      UNO.print("<START>");
      lastDebounceTime = millis();
    }
    if (digitalRead(BTN_RESET) == LOW) {
      UNO.print("<RESET>");
      lastDebounceTime = millis();
    }
  }

  // Ricezione Seriale dal Master e accumulo per il sito Web
  while (UNO.available()) {
    char c = UNO.read();
    Serial.write(c);   // Opzionale: stampa anche sul Monitor Seriale del PC     
    serialBuffer += c; // Accumula per la pagina web
  }

  // Limite di sicurezza RAM
  if (serialBuffer.length() > 1000) {
    serialBuffer = serialBuffer.substring(serialBuffer.length() - 500);
  }
}