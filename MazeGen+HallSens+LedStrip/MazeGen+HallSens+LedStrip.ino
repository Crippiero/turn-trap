#include <MuxControl.h>
#include <Adafruit_NeoPixel.h>

const int8_t ROWS = 11;
const int8_t COLS = 11;

// Usiamo singoli caratteri invece di oggetti String per risparmiare memoria
const char WALL = '#';
const char EMPTY = 'x';
const char GOAL = 'G';

const int8_t PIN = 8;
const uint8_t NUMPIXELS = ROWS*COLS; //NOTA: uint8_t = max 255 caselle!!!
const uint8_t SIZE = ROWS*COLS;
const uint16_t DELAY = 1000;

//PER POTER CHIAMARE UN PUNTO IN MODO PIU COMODO
struct Point {
    int8_t x, y;
};

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
Mux myMux(2000);

uint16_t myReadings[SIZE]; 
// Matrice statica globale (il labirinto)
char maze[ROWS][COLS];

// Variabili globali per i giocatori
Point player1 = {0, 0};
Point player2 = {0, COLS - 1};
Point player3 = {ROWS - 1, COLS - 1};
Point player4 = {ROWS - 1, 0};

// --- FUNZIONI ---

bool isValid(int8_t row, int8_t col) {
    return row > 0 && row < ROWS - 1 && col > 0 && col < COLS - 1;
}

void generateMazeDFS(int8_t row, int8_t col) {
    maze[row][col] = EMPTY; 

    int8_t directions[4][2] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}};
    int8_t directionIndices[4] = {0, 1, 2, 3};

    // Mescoliamo le direzioni usando la funzione random() di Arduino
    for (int8_t i = 0; i < 4; i++) {
        int8_t swapIdx = random(4);
        int8_t temp = directionIndices[i];
        directionIndices[i] = directionIndices[swapIdx];
        directionIndices[swapIdx] = temp;
    }

    // Proviamo tutte le direzioni nell'ordine nuovo
    for (int8_t i = 0; i < 4; i++) {
        int8_t deltaRow = directions[directionIndices[i]][0];
        int8_t deltaCol = directions[directionIndices[i]][1];
        
        int8_t nextRow = row + deltaRow;
        int8_t nextCol = col + deltaCol;
        
        if (isValid(nextRow, nextCol) && maze[nextRow][nextCol] == WALL) {
            // Abbattiamo il muro tra la cella corrente e la prossima
            maze[row + deltaRow/2][col + deltaCol/2] = EMPTY;
            generateMazeDFS(nextRow, nextCol);
        }
    }
}

void connectPlayerToMaze(int8_t posX, int8_t posY, char playerSymbol) {
    maze[posX][posY] = playerSymbol;

    int8_t deltaX[] = {-1, 1, 0, 0};
    int8_t deltaY[] = {0, 0, -1, 1};
    bool connected = false;

    // Controlla se il giocatore è già vicino a un sentiero vuoto
    for(int8_t i=0; i<4; i++) {
        int8_t neighborX = posX + deltaX[i];
        int8_t neighborY = posY + deltaY[i];
        if (neighborX >= 0 && neighborX < ROWS && neighborY >= 0 && neighborY < COLS) {
            if (maze[neighborX][neighborY] != WALL) connected = true;
        }
    }

    // Se isolato, crea un passaggio verso il centro
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

// Funzione per generare e visualizzare il labirinto
void renderNewMaze(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}) {
    // 1. Reset completo a MURI
    for (int8_t i = 0; i < ROWS; i++) {
        for (int8_t j = 0; j < COLS; j++) {
            maze[i][j] = WALL;
        }
    }

    // 2. Genera un NUOVO labirinto casuale partendo dal centro
    int8_t startX = (ROWS - 1) / 2;
    int8_t startY = (COLS - 1) / 2;
    generateMazeDFS(startX, startY);
    maze[startX][startY] = GOAL;

    // 3. Piazza i giocatori (SOLO se le coordinate sono valide)
    if (pl1.x != -1 && pl1.y != -1) connectPlayerToMaze(pl1.x, pl1.y, '1');
    if (pl2.x != -1 && pl2.y != -1) connectPlayerToMaze(pl2.x, pl2.y, '2');
    if (pl3.x != -1 && pl3.y != -1) connectPlayerToMaze(pl3.x, pl3.y, '3');
    if (pl4.x != -1 && pl4.y != -1) connectPlayerToMaze(pl4.x, pl4.y, '4');

    // 4. Stampa su Monitor Seriale
    Serial.println("\n\n--- NUOVO LABIRINTO ---");
    for (int8_t i = 0; i < ROWS; i++) {  
        for (int8_t j = 0; j < COLS; j++) {
            Serial.print(maze[i][j]);
            Serial.print(" ");
        }
        Serial.println();
    }
}

// --- SETUP E LOOP ARDUINO ---

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A2));

    myMux.setupMux(2, 3, 4, 5, A0);

    strip.begin();
    strip.clear();
}

void loop() {
    int maxIndex = 0, maxValue = 0;

    myMux.fullMuxAnalogRead(SIZE, myReadings);

    for(int i = 0; i < SIZE; i++) {
        Serial.print("Ch ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(myReadings[i]);

        if(myReadings[i] > maxValue) {
            maxValue = myReadings[i]; 
            maxIndex = i;
        }
    }

    strip.clear();  
    if(maxValue < 400 || maxValue > 500){
        strip.setPixelColor(maxIndex, strip.Color(255, 0, 0));
        renderNewMaze({0, maxIndex});
    }
    strip.show();

    Serial.println("---");
    Serial.print("maxValue:");
    Serial.println(maxValue);
    Serial.print("maxIndex:");
    Serial.println(maxIndex);
    Serial.println("---");

    delay(DELAY);
}

/*
for(int i=0; i<ROWS; i++){
    for(int j=0; j<COLS; j++){
        if(m[i][j] == '#'){
            strip.setPixelColor((i*ROWS)+j, strip.Color(255, 0, 0));
        }
        else if(m[i][j] == 'x'){
            strip.setPixelColor((i*ROWS)+j, strip.Color(255, 0, 0));
        }   
        else if(m[i][j] == 'G'){
            strip.setPixelColor((i*ROWS)+j, strip.Color(255, 0, 0));
        }       
    }
}
*/
