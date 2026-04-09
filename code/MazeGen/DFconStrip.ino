#include <Adafruit_NeoPixel.h>

const int8_t ROWS = 11;
const int8_t COLS = 11;

const char WALL = '#';
const char EMPTY = 'x';
const char GOAL = 'G';

const int8_t PIN = 8;
const uint16_t NUMPIXELS = ROWS * COLS; 
const uint16_t DELAY = 5000;

struct Point {
    int8_t x, y;
};

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

char maze[ROWS][COLS];

Point player1 = {0, 0};
Point player2 = {0, COLS - 1};
Point player3 = {ROWS - 1, COLS - 1};
Point player4 = {ROWS - 1, 0};

// Variabili per pre-calcolare i colori (risparmia molta CPU nel loop)
uint32_t colorWall;
uint32_t colorEmpty;
uint32_t colorGoal;
uint32_t colorPlayer;

// --- FUNZIONI ---

bool isValid(int8_t row, int8_t col) {
    return row > 0 && row < ROWS - 1 && col > 0 && col < COLS - 1;
}

void generateMazeDFS(int8_t row, int8_t col) {
    maze[row][col] = EMPTY; 

    // OTTIMIZZAZIONE 1: Salviamo le direzioni come costanti statiche.
    // In questo modo Arduino non deve ricreare questi array nella RAM 
    // ad ogni chiamata ricorsiva, salvando un'enorme quantità di memoria Stack.
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

void connectPlayerToMaze(int8_t posX, int8_t posY, char playerSymbol) {
    maze[posX][posY] = playerSymbol;

    static const int8_t deltaX[] = {-1, 1, 0, 0};
    static const int8_t deltaY[] = {0, 0, -1, 1};
    bool connected = false;

    for(int8_t i=0; i<4; i++) {
        int8_t neighborX = posX + deltaX[i];
        int8_t neighborY = posY + deltaY[i];
        if (neighborX >= 0 && neighborX < ROWS && neighborY >= 0 && neighborY < COLS) {
            if (maze[neighborX][neighborY] != WALL) connected = true;
        }
    }

    if (!connected) {
        int8_t centerX = ROWS / 2;
        int8_t centerY = COLS / 2;
        int8_t stepX = (centerX > posX) ? 1 : ((centerX < posX) ? -1 : 0);
        int8_t stepY = (centerY > posY) ? 1 : ((centerY < posY) ? -1 : 0);

        if (posX + stepX >= 0 && posX + stepX < ROWS) 
            maze[posX + stepX][posY] = EMPTY;
        else if (posY + stepY >= 0 && posY + stepY < COLS)
            maze[posX][posY + stepY] = EMPTY;
    }
}

void renderNewMaze(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}) {
    // 1. Reset a MURI
    for (int8_t i = 0; i < ROWS; i++) {
        for (int8_t j = 0; j < COLS; j++) {
            maze[i][j] = WALL;
        }
    }

    // 2. Genera
    int8_t startX = (ROWS - 1) / 2;
    int8_t startY = (COLS - 1) / 2;
    generateMazeDFS(startX, startY);
    maze[startX][startY] = GOAL;

    // 3. Piazza i giocatori
    if (pl1.x != -1 && pl1.y != -1) connectPlayerToMaze(pl1.x, pl1.y, '1');
    if (pl2.x != -1 && pl2.y != -1) connectPlayerToMaze(pl2.x, pl2.y, '2');
    if (pl3.x != -1 && pl3.y != -1) connectPlayerToMaze(pl3.x, pl3.y, '3');
    if (pl4.x != -1 && pl4.y != -1) connectPlayerToMaze(pl4.x, pl4.y, '4');

    // 4. Stampa su led neopixel
    Serial.println("\n--- NUOVO LABIRINTO ---");
    
    // Non serve usare strip.clear(), sovrascriviamo direttamente tutti i LED
    for(uint8_t i = 0; i < ROWS; i++) {
        for(uint8_t j = 0; j < COLS; j++) {
            
            uint16_t pixelIndex;
            
            // OTTIMIZZAZIONE 2: Calcolo serpentina con bitwise (& 1) invece di modulo (% 2)
            if ((i & 1) == 0) {
                // Riga pari: sinistra -> destra
                pixelIndex = (i * COLS) + j;
            } else {
                // Riga dispari: destra -> sinistra
                pixelIndex = (i * COLS) + (COLS - 1 - j);
            }
            
            // OTTIMIZZAZIONE 3: Assegnazione diretta dei colori pre-calcolati
            char cell = maze[i][j];
            if(cell == WALL){
                strip.setPixelColor(pixelIndex, colorWall);
            }
            else if(cell == EMPTY){
                strip.setPixelColor(pixelIndex, colorEmpty);
            }   
            else if(cell == GOAL){
                strip.setPixelColor(pixelIndex, colorGoal);
            }
            else if(cell >= '1' && cell <= '4'){
                strip.setPixelColor(pixelIndex, colorPlayer);
            }
            
            Serial.print(cell);
            Serial.print(" ");
        }
        Serial.println();
    }
    strip.show();
}

// --- SETUP E LOOP ARDUINO ---

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A2));

    strip.begin();
    
    // Pre-calcoliamo i colori una volta sola all'avvio!
    colorWall   = strip.Color(0, 0, 0);       // Spento
    colorEmpty  = strip.Color(30, 30, 30);    // Bianco debole
    colorGoal   = strip.Color(0, 255, 0);     // Verde
    colorPlayer = strip.Color(0, 0, 255);     // Blu

    strip.clear();
    strip.show();
}

void loop() {
    renderNewMaze(player1, player2, player3, player4);
    delay(DELAY);
}
