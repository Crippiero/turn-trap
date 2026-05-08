constexpr uint8_t CHANNEL = 11;
constexpr uint8_t ROWS = 11; 

constexpr uint8_t UNPR_OFFSET = 2;

constexpr int8_t RESOLUTION = 2; //quanti led di fila hanno lo stell colore, per le PCB serve che sia a 2
constexpr uint16_t PHYS_COLS = CHANNEL * RESOLUTION; // Le colonne fisiche sulla striscia LED diventano i canali moltiplicati per la risoluzione

constexpr uint8_t READINGS_DELAY = 1;
constexpr uint16_t RANGE = 50; //valore necessario per scartare il rumore

constexpr char WALL = '#';
constexpr char EMPTY = 'x';
constexpr char GOAL = 'G';
constexpr char UNPR_simbol = 'U';

constexpr uint16_t LOOP_DELAY = 10;
constexpr uint16_t PRESS_DELAY = 300;
constexpr uint16_t SETUP_DELAY = 30;

//comandi su seriale
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

// Variabili per pre-calcolare i colori (risparmia molta CPU nel loop)
uint32_t colorWall;
uint32_t colorEmpty;
uint32_t colorGoal;
uint32_t colorUnpr;
uint32_t colorMalus;
uint32_t colorBonus;

// --- FUNZIONI ---
bool isValid(int8_t row, int8_t col) {
  return row > 0 && row < ROWS - 1 && col > 0 && col < CHANNEL - 1;
}

bool isValidinUnpr(int8_t row, int8_t col) {
  // Ritorna vero se la coordinata è all'interno della mappa (inclusi i bordi 0 e max-1)
  return row >= 0 && row < ROWS && col >= 0 && col < CHANNEL;
}

bool samePoint(Point a, Point b){
    return a.x == b.x && a.y == b.y;
}

//riempie la matrice maze
void generateMazeDFS(int8_t row, int8_t col) {
  maze[row][col] = EMPTY;

  static const int8_t dirX[] = {-2, 2, 0, 0};
  static const int8_t dirY[] = {0, 0, -2, 2};
  
  // Solo l'array degli indici deve essere locale per poter essere mischiato
  int8_t dirs[] = {0, 1, 2, 3};

  // Mescoliamo le direzioni
  for (int8_t i = 0; i < 4; i++) {
    int8_t swapIdx = random(4);
    int8_t temp = dirs[i];
    dirs[i] = dirs[swapIdx];
    dirs[swapIdx] = temp;
  }

  // Proviamo tutte le direzioni nell'ordine nuovo
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

//rende il labiritno piu aperto, con piu opzioni
void addOpenBorders() {
  int8_t chanceToOpenInner = 15; // 15% di probabilità di rompere un muro interno
  int8_t chanceToOpenBorder = 20; // 20% di probabilità di aprire un muro sul bordo

  // --- Strade in piu ---
  for (int8_t r = 1; r < ROWS - 1; r++) {
    for (int8_t c = 1; c < CHANNEL - 1; c++) {
      if (maze[r][c] == WALL) {
        // Controlliamo se questo muro separa due celle vuote (in orizzontale o verticale)
        bool horizontalPass = (maze[r][c-1] == EMPTY && maze[r][c+1] == EMPTY);
        bool verticalPass = (maze[r-1][c] == EMPTY && maze[r+1][c] == EMPTY);
        
        // Se il muro divide due corridoi, lo rompiamo casualmente
        if ((horizontalPass || verticalPass) && random(100) < chanceToOpenInner) {
          maze[r][c] = EMPTY;
        }
      }
    }
  }

  // --- EFFETTO MURI ESTERNI APERTI ---
  for (int8_t r = 0; r < ROWS; r++) {
    if (random(100) < chanceToOpenBorder) maze[r][0] = EMPTY;        // Bordo sinistro
    if (random(100) < chanceToOpenBorder) maze[r][CHANNEL-1] = EMPTY;   // Bordo destro
  }
  
  for (int8_t c = 0; c < CHANNEL; c++) {
    if (random(100) < chanceToOpenBorder) maze[0][c] = EMPTY;        // Bordo superiore
    if (random(100) < chanceToOpenBorder) maze[ROWS-1][c] = EMPTY;   // Bordo inferiore
  }
}

void connectPlayerToMaze(int8_t posX, int8_t posY/) {
  maze[posX][posY] = EMPTY;

  static const int8_t deltaX[] = {-1, 1, 0, 0};
  static const int8_t deltaY[] = {0, 0, -1, 1};
  bool connected = false;

  //Controllo se è connesso
  for(int8_t i=0; i<4; i++) 
  {
    int8_t neighborX = posX + deltaX[i];
    int8_t neighborY = posY + deltaY[i];
    if (neighborX >= 0 && neighborX < ROWS && neighborY >= 0 && neighborY < CHANNEL) 
    {
      if (maze[neighborX][neighborY] != WALL) connected = true;
    }
  }

  //Se non è connesso:
  if (!connected) 
  {
    int8_t centerX = ROWS / 2;
    int8_t centerY = CHANNEL / 2;
    int8_t stepX = (centerX > posX) ? 1 : ((centerX < posX) ? -1 : 0);
    int8_t stepY = (centerY > posY) ? 1 : ((centerY < posY) ? -1 : 0);

    if (posX + stepX >= 0 && posX + stepX < ROWS) 
      maze[posX + stepX][posY] = EMPTY;
    else if (posY + stepY >= 0 && posY + stepY < CHANNEL)
      maze[posX][posY + stepY] = EMPTY;
 }
}

//genera i punti casuali nell'intorno del giocatore
Point generateUnprPoint(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  Point p = {-1, -1};
  int8_t x, y;
  //controllo a vuoto per evitsre un loop infinito
  if (pl1.x == -1 && pl2.x == -1 && pl3.x == -1 && pl4.x == -1) {
    return p; 
  }
  uint8_t genTry=0;
  while (genTry < 50){
    x = random(-UNPR_OFFSET, UNPR_OFFSET+ 1);
    y = random(-UNPR_OFFSET, UNPR_OFFSET+ 1);
    switch (random(4)){
      case 0:
        if (pl1.x != -1 && pl1.y != -1) {
          p.x = pl1.x + x;
          p.y = pl1.y + y;
          if (isValidinUnpr(p.x, p.y)){
            return p;
          }
        }
        break;
      case 1:
        if (pl2.x != -1 && pl2.y != -1) {
          p.x = pl2.x + x;
          p.y = pl2.y + y;
          if (isValidinUnpr(p.x, p.y)){
            return p;
          }
        }
        break;
      case 2:
        if (pl3.x != -1 && pl3.y != -1) {
          p.x = pl3.x + x;
          p.y = pl3.y + y;
          if (isValidinUnpr(p.x, p.y)){
            return p;
          }
        }
        break;
      case 3:
        if (pl4.x != -1 && pl4.y != -1) {
          p.x = pl4.x + x;
          p.y = pl4.y + y;
          if (isValidinUnpr(p.x, p.y)){
            return p;
          }
        }
        break;
    }
    genTry++;
  }
  return {-1, -1};
}