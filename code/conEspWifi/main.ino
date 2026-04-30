#include <Adafruit_NeoPixel.h>

constexpr uint8_t CHANNEL = 11;
constexpr uint8_t ROWS = 11; 

//quanti led di fila hanno lo stell colore, per test ora lasciamo a 1, nel progetto VA AUMENTATO!!
constexpr int8_t RESOLUTION = 1;
constexpr uint16_t PHYS_COLS = CHANNEL * RESOLUTION; // Le colonne fisiche sulla striscia LED diventano i canali moltiplicati per la risoluzione

constexpr uint8_t READINGS_DELAY = 1;
constexpr uint16_t RANGE = 50; //valore necessario per scartare il rumore

constexpr char WALL = '#';
constexpr char EMPTY = 'x';
constexpr char GOAL = 'G';

constexpr int8_t PIN = 12;
constexpr uint16_t NUMPIXELS = ROWS * PHYS_COLS;

constexpr uint16_t LOOP_DELAY = 10;
constexpr uint16_t PRESS_DELAY = 300;
constexpr uint16_t SETUP_DELAY = 30;

constexpr uint8_t SERIAL_START_BYTE = 0xAA;
constexpr uint8_t CMD_START = 0x01;
constexpr uint8_t CMD_RESET = 0x02;

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

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

//VA TOLTO???
Point player1 = {0, 0};
Point player2 = {0, CHANNEL - 1};
Point player3 = {ROWS - 1, CHANNEL - 1};
Point player4 = {ROWS - 1, 0};

// Variabili per pre-calcolare i colori (risparmia molta CPU nel loop)
uint32_t colorWall;
uint32_t colorEmpty;
uint32_t colorGoal;
uint32_t colorUnpr;
uint32_t colorMalus;
uint32_t colotBonus;

// --- FUNZIONI ---

bool isValid(int8_t row, int8_t col) {
    return row > 0 && row < ROWS - 1 && col > 0 && col < CHANNEL - 1;
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

void connectPlayerToMaze(int8_t posX, int8_t posY/*, char playerSymbol*/) {
    //maze[posX][posY] = playerSymbol; Commentato cosi non mette un'altro colore dov'è il giocatore
    maze[posX][posY] = EMPTY;

    static const int8_t deltaX[] = {-1, 1, 0, 0};
    static const int8_t deltaY[] = {0, 0, -1, 1};
    bool connected = false;

    //Controllo se è connesso
    for(int8_t i=0; i<4; i++) {
        int8_t neighborX = posX + deltaX[i];
        int8_t neighborY = posY + deltaY[i];
        if (neighborX >= 0 && neighborX < ROWS && neighborY >= 0 && neighborY < CHANNEL) {
            if (maze[neighborX][neighborY] != WALL) connected = true;
        }
    }

    //Se non è connesso:
    if (!connected) {
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

void renderNewMaze(Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}) {   
  /*-- 1. Reset della matrice a solo MURI --*/
  for (int8_t i = 0; i < ROWS; i++) {
      for (int8_t j = 0; j < CHANNEL; j++) {
          maze[i][j] = WALL;
      }
  }

  /*-- 2. Genera scavando nei muri --*/
  //Partenza dal centro
  int8_t startX = (ROWS - 1) / 2;
  int8_t startY = (CHANNEL - 1) / 2;
  generateMazeDFS(startX, startY);
  maze[startX][startY] = GOAL;

  // 3. Piazza i giocatori
  if (pl1.x != -1 && pl1.y != -1) connectPlayerToMaze(pl1.x, pl1.y);
  if (pl2.x != -1 && pl2.y != -1) connectPlayerToMaze(pl2.x, pl2.y);
  if (pl3.x != -1 && pl3.y != -1) connectPlayerToMaze(pl3.x, pl3.y);
  if (pl4.x != -1 && pl4.y != -1) connectPlayerToMaze(pl4.x, pl4.y);

  // 4. Stampa su led neopixel
  Serial.println("\n--- NUOVO LABIRINTO ---"); //debug
  
  // Non serve usare strip.clear(), sovrascriviamo direttamente tutti i LED
  for(uint8_t i = 0; i < ROWS; i++) {
    for(uint8_t j = 0; j < CHANNEL; j++) {
        
      // Decidiamo il colore una volta sola per questa cella logica
      char cell = maze[i][j];
      uint32_t targetColor = colorWall;
      
      if(cell == EMPTY){
          targetColor = colorEmpty;
      }   
      else if(cell == GOAL){
          targetColor = colorGoal;
      }

      // Moltiplichiamo il LED orizzontalmente in base a RESOLUTION
      for (uint8_t k = 0; k < RESOLUTION; k++) {
        uint16_t pixelIndex;
        
        // Calcolo serpentina basato sulle colonne FISICHE
        if ((i & 1) == 0) {
          // Riga pari: sinistra -> destra
          // Partiamo dall'inizio del blocco della cella (j * RESOLUTION) e sommiamo k
          pixelIndex = (i * PHYS_COLS) + (j * RESOLUTION) + k;
        } else {    
          // Riga dispari: destra -> sinistra
          // Partiamo dalla fine della riga e sottraiamo l'avanzamento
          pixelIndex = (i * PHYS_COLS) + (PHYS_COLS - 1) - ((j * RESOLUTION) + k);
        }
        strip.setPixelColor(pixelIndex, targetColor);
      }
      Serial.print(cell);
      Serial.print(" ");
    }
    Serial.println();
  }
  strip.show();
}

/*
  OUTPUT PINS TO SELECT THE MUX'S CHANNEL 
    S0 = 2   => PORTD (Bit 2)
    S1 = 3   => PORTD (Bit 3)
    S2 = 4   => PORTD (Bit 4)
    S3 = 5   => PORTD (Bit 5)
                               <- Seleziona la riga (Mux Master)
    S4 = 8   => PORTB (Bit 0)
    S5 = 9   => PORTB (Bit 1)
    S6 = 10  => PORTB (Bit 2)
    S7 = 11  => PORTB (Bit 3)
    SIG = A0 => PORTC
*/

void magnetDetection(){
  //Pulisco il podio per non avere fantasmi dalla vecchia misurazione
  for(uint8_t k = 0; k < 4; k++) {
    podiumValue[k].x = -1;
    podiumValue[k].y = -1;
    podiumValue[k].intensity = 0;
  }

  uint16_t reading;
  // Ciclo esterno: scorro le righe (MUX Master) tramite PORTB
  for(byte j = 0; j < ROWS; j++){
    // Azzero i primi 4 bit di PORTB (pin 8, 9, 10, 11)
    PORTB &= ~0b00001111;
    // Scrivo l'indice della riga sui pin 8-11
    PORTB |= j;

    // Ciclo interno: scorro le colonne (MUX Slave) tramite PORTD
    for(byte i = 0; i < CHANNEL; i++){
      //Azzero i pin 2, 3, 4 e 5 per evitare errori
      PORTD &= ~0b00111100;

      //Con il bitshift prendo l'indice del canale (i), inteso come numero binario, sposto i bit di 2 posizioni per coincidere con i canali 2, 3, 4 e 5
      PORTD |= (i << 2);

      //Delay per evitare letture di canali sovrapposti
      delayMicroseconds(READINGS_DELAY);

      //Dubby read => per scaricare il condensatore Sample & Hold del ADC (analog - digital converter)
      analogRead(A0);
      reading = analogRead(A0);

      // Prendo il valore a riposo specifico del sensore ATTUALE
      uint16_t baseline = sensorBaseline[j][i];

       /*
          Calcolo l'intensità assoluta (distanza dal valore di riposo baseline calcolato nel setup)
          Si usa l'operatore ternario, funziona come un if / elese, e significa, assumendo baseline=512:
          
          if (reading > 512)
            intensity = reading - 512
          else
            intensity = 512 - reading

          è come fare =  intensity=ABS(intensity) dopo la sottrazione
        */
        uint16_t intensity = (reading > baseline) ? (reading - baseline) : (baseline - reading);

      /*--% Per liberare la memoria mi salvo solo un podio dei 4 (possibili) valori maggiori %--*/

      //Se i valori rientrano in un range (quindi c'è il magnete) lo salvo nella struttua podiumValue
      if(intensity > RANGE) {
        /*
          Essendo podiumValue un podio, in posizione 3, ci sarà il valore minore.
          se un valore è minore di podiumValue[3] allora per forza è minore degli altri valori
        */
        if (intensity > podiumValue[3].intensity) {
          /*--% Metto il valore nella posizione corretta in ordine di dimensione dell'intensità (lontananza da baseline) %--*/
          // visto che è entrato nell'if intensity è maggiore di del valore in posizione 3, quindi controllo a partire da pos = 3
          int8_t pos = 3;
          while (pos > 0 && intensity > podiumValue[pos - 1].intensity) { 
            pos--; 
          }
          //Adesso pos indica la posizione corretta del valore che devo inserire
          // Faccio scorrere i valori più piccoli verso il basso, per fare spazio al mio valore nuovo
          for (int8_t k = 3; k > pos; k--) { 
            podiumValue[k] = podiumValue[k - 1]; 
          }
          
          // Inserisco il nuovo magnete al posto giusto, che ho appena liberato
          podiumValue[pos].x = j; //ROWS
          podiumValue[pos].y = i; //CHANNEL
          podiumValue[pos].intensity = intensity;
        }
      }
    }
  }
}

//calcolo una media dei valori per considerarlo come posizione 0 (ovvero misurazione senza magneti)
void calibrateSensors() {
  for(byte j = 0; j < ROWS; j++) {
    PORTB &= ~0b00001111;
    PORTB |= j;

    for(byte i = 0; i < CHANNEL; i++) {
      PORTD &= ~0b00111100;
      PORTD |= (i << 2);
      delayMicroseconds(READINGS_DELAY);
      
      // Faccio una media di 4 letture per avere una base super stabile
      uint32_t sum = 0;
      for(byte n = 0; n < 4; n++) {
        analogRead(A0); // Dummy
        sum += analogRead(A0);
      }
      sensorBaseline[j][i] = sum / 4; 
    }
  }
}

// --- SETUP E LOOP ARDUINO ---

void setup() {
    Serial.begin(9600);
    Serial.println("Calibrazione in corso... NON avvicinare magneti!");

    delay(SETUP_DELAY);

    randomSeed(analogRead(A2));

    //  |=    =>  è l'operatore OR, dove lascio zero, rimane come gia impostato, dove metto 1 lascia 1
    //  &= ~  =>  è l'opreatore AND NOT quindi dove metto uno a prescindere da cosa c'era mette 0
    //Imposto i pin 2, 3, 4 e 5 come OUTPUT
    DDRD |= 0b00111100;
    //Imposto i pin 8, 9, 10 e 11 come OUTPUT
    DDRB |= 0b00001111;
    //Imposto il pin A0 come INPUT
    DDRC &= ~0b00000001;

    strip.begin();
    
    // Pre-calcoliamo i colori una volta sola all'avvio!
    colorWall   = strip.Color(255, 0, 0);       // Rosso
    colorEmpty  = strip.Color(180, 160, 180);    // Bianco debole
    colorGoal   = strip.Color(0, 0, 50);     // Blu
    colorUnpr  = strip.Color(255, 255, 0);  //Giallo
    colorBonus = strip.Color(0, 255, 0);    //Verde
    colorMalus  = strip.Color(255, 0, 220); //Viola

    strip.clear();
    strip.show();

    //riempio sensorBaseline con il valore di base per quello specifico sensore
    calibrateSensors();
    Serial.println("Calibrazione completata!");
}

void loop() {
  // --- LETTURA E PARSING DELLA SERIALE ---
  // Aspettiamo che ci siano almeno 4 byte nel buffer (START, CMD, LEN, CHK)
  if (Serial.available() >= 4) {
    
    // Controlliamo se il primo byte è quello di START
    if (Serial.peek() == SERIAL_START_BYTE) {
      Serial.read(); // Consumiamo il byte di start (0xAA)
      
      uint8_t cmd = Serial.read();
      uint8_t len = Serial.read(); // Per ora è 0
      uint8_t chk = Serial.read();

      // Calcoliamo e verifichiamo il Checksum (XOR tra CMD e LEN)
      if (chk == (cmd ^ len)) {
        
        // ==========================================
        // COMANDO 1: START
        // ==========================================
        if (cmd == CMD_START) {
          delay(200);
          magnetDetection(); // Cerco i magneti

          Serial.println("%------------%");
          for (uint8_t k = 0; k < 4; k++) {
              if (podiumValue[k].intensity > 0) {
                  // Stampe di debug
                  Serial.print("Magnete "); Serial.print(k);
                  Serial.print(" in: X="); Serial.print(podiumValue[k].x);
                  Serial.print(" Y="); Serial.print(podiumValue[k].y);
                  Serial.print(" (Forza: "); Serial.print(podiumValue[k].intensity);
                  Serial.println(")");
              } 
              else { break; }
          }
          player1 = {-1, -1};
          player2 = {-1, -1};
          player3 = {-1, -1};
          player4 = {-1, -1};

          // Controllo se il punto esiste (intensità > 0) prima di aggiornare i player
          if (podiumValue[0].intensity > 0) player1 = {podiumValue[0].x, podiumValue[0].y};
          if (podiumValue[1].intensity > 0) player2 = {podiumValue[1].x, podiumValue[1].y};
          if (podiumValue[2].intensity > 0) player3 = {podiumValue[2].x, podiumValue[2].y};
          if (podiumValue[3].intensity > 0) player4 = {podiumValue[3].x, podiumValue[3].y};
                    
          renderNewMaze(player1, player2, player3, player4);
          
          renderNewMaze(player1, player2, player3, player4);
          delay(PRESS_DELAY);
        }
        
        // ==========================================
        // COMANDO 2: RESET
        // ==========================================
        else if (cmd == CMD_RESET) {
          Serial.println("Inizio Procedura di Setup/Reset...");
          Serial.println("Assicurati che TUTTE le pedine siano rimosse!");
          
          // Un piccolo delay per dare il tempo di togliere le mani/pedine se necessario
          delay(1000); 

          // Eseguiamo direttamente la calibrazione senza aspettare pulsanti fisici, 
          // dato che il comando è arrivato via seriale.
          Serial.println("Calibrazione in corso... NON avvicinare magneti!");

          randomSeed(analogRead(A2));

          strip.clear();
          strip.show();

          calibrateSensors();

          Serial.println("Calibrazione completata!");
          Serial.println("Posizionare le pedine agli angoli della partita e inviare comando START");
          delay(PRESS_DELAY);
        }
        
      } else {
        Serial.println("Errore: Checksum Seriale non valido!");
      }
    } else {
      // Se il primo byte non è 0xAA, c'è spazzatura sulla seriale.
      // Leggiamo un byte a vuoto per sbloccare il buffer finché non troviamo 0xAA.
      Serial.read(); 
    }
  }

  // Ritardo generale del loop
  delay(LOOP_DELAY);
}
