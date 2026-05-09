#include <SoftwareSerial.h>

SoftwareSerial ledSerial(6, 7); //6=RX 7=TX

constexpr uint8_t CHANNEL = 11;
constexpr uint8_t ROWS = 11;

constexpr uint8_t UNPR_OFFSET = 2;

constexpr char WALL = '#';
constexpr char EMPTY = 'x';
constexpr char GOAL = 'G';
constexpr char UNPR_simbol = 'U';

constexpr uint8_t READINGS_DELAY = 1;
constexpr uint16_t RANGE = 50; 

constexpr uint16_t LOOP_DELAY = 10;
constexpr uint16_t PRESS_DELAY = 300;
constexpr uint16_t SETUP_DELAY = 30;

constexpr uint8_t SERIAL_START_BYTE = 0xAA;
constexpr uint8_t CMD_START = 0x01;
constexpr uint8_t CMD_RESET = 0x02;

unsigned long previousGameMillis = 0;
const unsigned long gameInterval = 200;

struct Podium { 
  int8_t x, y; 
  uint16_t intensity; 
};

struct Point { 
  int8_t x, y; 
};

Podium podiumValue[4];
char maze[ROWS][CHANNEL];
uint16_t sensorBaseline[ROWS][CHANNEL]; // Contiene il valore a riposo di ogni sensore per usarlo come punto zero

Point player1 = {0, 0};
Point player2 = {0, CHANNEL - 1};
Point player3 = {ROWS - 1, CHANNEL - 1};
Point player4 = {ROWS - 1, 0};
Point unpr = {-1, -1};

// --- FUNZIONI ---

// -- funzioni logiche --
bool isValid(int8_t row, int8_t col) { 
  return row > 0 && row < ROWS - 1 && col > 0 && col < CHANNEL - 1; 
}

bool inGame(int8_t row, int8_t col) { 
  return row >= 0 && row < ROWS && col >= 0 && col < CHANNEL; 
}

bool samePoint(Point a, Point b){ 
  return a.x == b.x && a.y == b.y; 
}

// -- funzioni comunicazione seriale --
void printUnpr(Point p, char colorCode) {
  if(p.x == -1 || p.y == -1) return;
  
  // Invia allo Slave LED
  ledSerial.print("<U,"); ledSerial.print(p.x); ledSerial.print(","); 
  ledSerial.print(p.y); ledSerial.print(","); ledSerial.print(colorCode); ledSerial.print(">");
  
  // Invia all'ESP (Web)
  Serial.print("<U,"); Serial.print(p.x); Serial.print(","); 
  Serial.print(p.y); Serial.print(","); Serial.print(colorCode); Serial.print(">");
}

void sendCommandToAll(char cmd) {
  // Invia allo Slave (LED)
  ledSerial.print('<'); ledSerial.print(cmd); ledSerial.print('>');
  // Invia all'ESP (Sito Web)
  Serial.print('<'); Serial.print(cmd); Serial.print('>');
}

//funzioni di generazione
void generateMazeDFS(int8_t row, int8_t col) {
  maze[row][col] = EMPTY;

  static const int8_t dirX[] = {-2, 2, 0, 0};
  static const int8_t dirY[] = {0, 0, -2, 2};

  //creo un array con gli index da mischiare invece di farlo con i le direzioni
  int8_t dirs[] = {0, 1, 2, 3};

  for (int8_t i = 0; i < 4; i++) {
    int8_t swapIdx = random(4);
    int8_t temp = dirs[i];

    dirs[i] = dirs[swapIdx];
    dirs[swapIdx] = temp;
  }

  // Proviamo tutte le direzioni nell'ordine modificato
  for (int8_t i = 0; i < 4; i++) {
    int8_t deltaRow = dirX[dirs[i]];
    int8_t deltaCol = dirY[dirs[i]];
    int8_t nextRow = row + deltaRow;
    int8_t nextCol = col + deltaCol;

    if (isValid(nextRow, nextCol) && maze[nextRow][nextCol] == WALL) {
      maze[row + deltaRow/2][col + deltaCol/2] = EMPTY;
      generateMazeDFS(nextRow, nextCol);
    }
  }
}

void addOpenBorders() {
  //percentuali di probabilità che si apta un muro interno o un muro sul bordo
  int8_t chanceToOpenInner = 15; 
  int8_t chanceToOpenBorder = 20;

  for (int8_t r = 1; r < ROWS - 1; r++) {
    for (int8_t c = 1; c < CHANNEL - 1; c++) {
      if (maze[r][c] == WALL) {
        bool horizontalPass = (maze[r][c-1] == EMPTY && maze[r][c+1] == EMPTY);
        bool verticalPass = (maze[r-1][c] == EMPTY && maze[r+1][c] == EMPTY);

        if ((horizontalPass || verticalPass) && random(100) < chanceToOpenInner) {
          maze[r][c] = EMPTY;
        }
      }
    }
  }

  //muri esterni aperti
  for (int8_t r = 0; r < ROWS; r++) {
    if (random(100) < chanceToOpenBorder) maze[r][0] = EMPTY;        
    if (random(100) < chanceToOpenBorder) maze[r][CHANNEL-1] = EMPTY;   
  }
  for (int8_t c = 0; c < CHANNEL; c++) {
    if (random(100) < chanceToOpenBorder) maze[0][c] = EMPTY;        
    if (random(100) < chanceToOpenBorder) maze[ROWS-1][c] = EMPTY;   
  }
}

void connectPlayerToMaze(int8_t posX, int8_t posY) {
  maze[posX][posY] = EMPTY;

  static const int8_t deltaX[] = {-1, 1, 0, 0};
  static const int8_t deltaY[] = {0, 0, -1, 1};
  bool connected = false;

  //controllo se gia connesso
  for(int8_t i=0; i<4; i++) {
    int8_t neighborX = posX + deltaX[i];
    int8_t neighborY = posY + deltaY[i];

    if (neighborX >= 0 && neighborX < ROWS && neighborY >= 0 && neighborY < CHANNEL) {
      if (maze[neighborX][neighborY] != WALL) connected = true;
    }
  }

  if (!connected) {
    int8_t centerX = ROWS / 2;
    int8_t centerY = CHANNEL / 2;

    int8_t stepX = (centerX > posX) ? 1 : ((centerX < posX) ? -1 : 0);
    int8_t stepY = (centerY > posY) ? 1 : ((centerY < posY) ? -1 : 0);

    if (posX + stepX >= 0 && posX + stepX < ROWS){ 
      maze[posX + stepX][posY] = EMPTY;
    }
    else if (posY + stepY >= 0 && posY + stepY < CHANNEL) {
      maze[posX][posY + stepY] = EMPTY;
    }
  }
}

Point generateUnprPoint(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  Point p = {-1, -1};

  if (pl1.x == -1 && pl2.x == -1 && pl3.x == -1 && pl4.x == -1) return p; //così evito un loop infinito

  uint8_t genTry=0;

  while (genTry < 50){
    int8_t x = random(-UNPR_OFFSET, UNPR_OFFSET+ 1);
    int8_t y = random(-UNPR_OFFSET, UNPR_OFFSET+ 1);
    
    switch (random(4)){
      case 0: 
        if (pl1.x != -1 && pl1.y != -1) { 
          p.x = pl1.x + x; p.y = pl1.y + y; 
          if (inGame(p.x, p.y)) return p; 
        } 
        break;
      case 1: 
        if (pl2.x != -1 && pl2.y != -1) { 
          p.x = pl2.x + x; p.y = pl2.y + y; 
          if (inGame(p.x, p.y)) return p; 
        } 
        break;
      case 2: 
        if (pl3.x != -1 && pl3.y != -1) { 
          p.x = pl3.x + x; p.y = pl3.y + y; 
          if (inGame(p.x, p.y)) return p; 
        } 
        break;
      case 3: 
        if (pl4.x != -1 && pl4.y != -1) { 
          p.x = pl4.x + x; p.y = pl4.y + y; 
          if (inGame(p.x, p.y)) return p; 
        } 
        break;
    }
    genTry++;
  }
  return {-1, -1};
}

void renderNewMaze(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}) {   
  // Reset e generazione
  for (uint8_t i = 0; i < ROWS; i++) {
      for (uint8_t j = 0; j < CHANNEL; j++){ 
        maze[i][j] = WALL; 
      }
  }

  //partenza dal centro
  int8_t startX = (ROWS - 1) / 2;
  int8_t startY = (CHANNEL - 1) / 2;

  generateMazeDFS(startX, startY); //chiamo la funzione che genera il labirinto
  addOpenBorders(); //lo rendo piu "aperto"
  maze[startX][startY] = GOAL;

  if (pl1.x != -1 && pl1.y != -1) connectPlayerToMaze(pl1.x, pl1.y);
  if (pl2.x != -1 && pl2.y != -1) connectPlayerToMaze(pl2.x, pl2.y);
  if (pl3.x != -1 && pl3.y != -1) connectPlayerToMaze(pl3.x, pl3.y);
  if (pl4.x != -1 && pl4.y != -1) connectPlayerToMaze(pl4.x, pl4.y);

  // Trasmissione (LED + ESP)
  for(uint8_t i = 0; i < ROWS; i++) {
    // Intestazione pacchetto
    ledSerial.print("<R,"); ledSerial.print(i); ledSerial.print(",");
    Serial.print("<R,"); Serial.print(i); Serial.print(",");
    
    // Contenuto riga
    for(uint8_t j = 0; j < CHANNEL; j++) {
      ledSerial.print(maze[i][j]);
      Serial.print(maze[i][j]);
    }
    
    // Chiusura pacchetto
    ledSerial.print(">");
    Serial.print(">");
    
    delay(10); // Pausa per dare tempo ai buffer seriali (Arduino Slave e ESP) di svuotarsi
  }
  
  sendCommandToAll('S'); // Mostra a schermo e sui LED
}

// --- funzioni di gioco ---
void getUnpr(Point unpr, Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  switch (random(4)){
    case 0: //doppio turno
      printUnpr(unpr, 'B'); 
      sendCommandToAll('S'); 
      delay(1000);
      printUnpr(unpr, 'E'); 
      sendCommandToAll('S');
      break;
    case 1://sopostamento casuale
      Point p; 
      bool check = false;

      do{
        p.x = random(CHANNEL); 
        p.y = random(ROWS);

        if(inGame(p.x, p.y) && !(p.x == (ROWS - 1) / 2 && p.y == (CHANNEL - 1) / 2)) {
          if (!samePoint(pl1, p) && !samePoint(pl2, p) && !samePoint(pl3, p) && !samePoint(pl4, p)) {
            check = true;
          }
        }
      } while (!check);
      
      printUnpr(p, 'M'); 
      sendCommandToAll('S'); 
      delay(1000);
      printUnpr(p, 'E'); 
      sendCommandToAll('S');
      break;
    case 2: //blocca il turno a un 
      printUnpr(pl1, 'W'); 
      sendCommandToAll('S'); 
      delay(1000);
      printUnpr(pl1, 'E');
      sendCommandToAll('S');
      break;
    case 3:
      printUnpr(unpr, 'M');
      sendCommandToAll('S'); 
      delay(1000);
      printUnpr(unpr, 'E');
      sendCommandToAll('S');
      break;
  }
}

void checkUnpr(Point unpr, Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  if(samePoint(pl1, unpr) || samePoint(pl2, unpr) || samePoint(pl3, unpr) || samePoint(pl4, unpr)){
    getUnpr(unpr, pl1, pl2, pl3, pl4);
  }
}

// --- LETTURE SENSORI E MULTIPLEXER (Rimaste invariate) ---
void magnetDetection(){
  for(uint8_t k = 0; k < 4; k++) { podiumValue[k] = {-1, -1, 0}; }
  uint16_t reading;
  
  for(byte j = 0; j < ROWS; j++){
    PORTB &= ~0b00001111; PORTB |= j;
    for(byte i = 0; i < CHANNEL; i++){
      PORTD &= ~0b00111100; PORTD |= (i << 2);
      delayMicroseconds(READINGS_DELAY);
      analogRead(A0); reading = analogRead(A0);
      uint16_t baseline = sensorBaseline[j][i];
      uint16_t intensity = (reading > baseline) ? (reading - baseline) : (baseline - reading);
      
      if(intensity > RANGE) {
        if (intensity > podiumValue[3].intensity) {
          int8_t pos = 3;
          while (pos > 0 && intensity > podiumValue[pos - 1].intensity) { pos--; }
          for (int8_t k = 3; k > pos; k--) { podiumValue[k] = podiumValue[k - 1]; }
          podiumValue[pos] = {(int8_t)j, (int8_t)i, intensity};
        }
      }
    }
  }

  // --- TRASMISSIONE MAGNETI ALL'ESP (WEB) ---
  for (uint8_t k = 0; k < 4; k++) {
    if (podiumValue[k].intensity > 0) {
      Serial.print("<M,"); Serial.print(k); Serial.print(",");
      Serial.print(podiumValue[k].x); Serial.print(",");
      Serial.print(podiumValue[k].y); Serial.print(",");
      Serial.print(podiumValue[k].intensity); Serial.print(">");
    } 
  }
}

void calibrateSensors() {
  for(byte j = 0; j < ROWS; j++) {
    PORTB &= ~0b00001111; PORTB |= j;
    for(byte i = 0; i < CHANNEL; i++) {
      PORTD &= ~0b00111100; PORTD |= (i << 2);
      delayMicroseconds(READINGS_DELAY);
      uint32_t sum = 0;
      for(byte n = 0; n < 4; n++) { analogRead(A0); sum += analogRead(A0); }
      sensorBaseline[j][i] = sum / 4; 
    }
  }
}

void resetCalibration (){
  sendCommandToAll('C'); // Clear striscia led e Web
  sendCommandToAll('S'); // Aggiorna visiva
  calibrateSensors();
}

void gamePlay(){
  delay(200);
  magnetDetection(); 
  player1 = {-1, -1}; player2 = {-1, -1}; player3 = {-1, -1}; player4 = {-1, -1};
  if (podiumValue[0].intensity > 0) player1 = {podiumValue[0].x, podiumValue[0].y};
  if (podiumValue[1].intensity > 0) player2 = {podiumValue[1].x, podiumValue[1].y};
  if (podiumValue[2].intensity > 0) player3 = {podiumValue[2].x, podiumValue[2].y};
  if (podiumValue[3].intensity > 0) player4 = {podiumValue[3].x, podiumValue[3].y};
}

void processSerialCommands() {
  if (Serial.available() < 4) return;
  if (Serial.peek() != SERIAL_START_BYTE) { Serial.read(); return; }
  Serial.read(); 
  uint8_t cmd = Serial.read();
  uint8_t len = Serial.read();
  uint8_t chk = Serial.read();
  if (chk != (cmd ^ len)) return; 

  switch (cmd) {
    case CMD_START:
      renderNewMaze(player1, player2, player3, player4);
      unpr = generateUnprPoint(player1, player2, player3, player4);
      delay(PRESS_DELAY);
      break;
    case CMD_RESET:
      resetCalibration();
      delay(PRESS_DELAY);
      break;
  }
}

void setup() {
    Serial.begin(9600);
    ledSerial.begin(38400); // Seriale veloce per non far laggare il gioco

    delay(SETUP_DELAY);
    randomSeed(analogRead(A2));

    DDRD |= 0b00111100;
    DDRB |= 0b00001111;
    DDRC &= ~0b00000001;

    calibrateSensors();
}

void loop() {
  processSerialCommands();
  unsigned long currentMillis = millis();
  if (currentMillis - previousGameMillis >= gameInterval) {
    previousGameMillis = currentMillis; 
    gamePlay();
    
    printUnpr(unpr, 'U');
    sendCommandToAll('S'); // Mostra su led
    
    checkUnpr(unpr, player1, player2, player3, player4);
  }
}