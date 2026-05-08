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

// x = colonna logica (0 fino a CHANNEL-1)
// y = riga logica (0 fino a ROWS-1)
// k = offset interno se RESOLUTION > 1 (da 0 a RESOLUTION-1)

uint16_t getPixelIndex(int8_t x, int8_t y, uint8_t k = 0) {
  uint16_t index;

  if ((y & 1) == 0) {
    // RIGA PARI: Direzione normale (->)
    index = (y * PHYS_COLS) + (x * RESOLUTION) + k;
  } else {
    // RIGA DISPARI: Direzione invertita (<-)
    index = (y * PHYS_COLS) + (PHYS_COLS - 1) - ((x * RESOLUTION) + k);
  }
  return index;
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

void getUnpr(Point unpr, Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  switch (random(4)){
    case 0: //doppio turno
      printUnpr(unpr, colorBonus);
      delay(1000);
      printUnpr(unpr, colorEmpty);
      break;
    case 1: //spostamento casuale
      Point p;
      bool check = false;
      do{
        p.x = random(CHANNEL);
        p.y = random(ROWS);
        if(isValidinUnpr(p.x, p.y)){
          if(!(p.x == (ROWS - 1) / 2 && p.y == (CHANNEL - 1) / 2)) {
            if (!samePoint(pl1, p) && !samePoint(pl2, p) && !samePoint(pl3, p) && !samePoint(pl4, p)) {
              check = true;
            }
          }
        }
      } while (!check);
      printUnpr(p, colorMalus);
      delay(1000);
      printUnpr(p, colorEmpty);
      break;
    case 2: //blocca il turno a un giocatore casuale
      uint8_t numPlayer = 0;
      if (pl1.x != -1 && pl1.y != -1) numPlayer++;
      if (pl2.x != -1 && pl2.y != -1) numPlayer++;
      if (pl3.x != -1 && pl3.y != -1) numPlayer++;
      if (pl4.x != -1 && pl4.y != -1) numPlayer++;
      switch(random(numPlayer)){
        case 0:
          printUnpr(pl1, colorWall);
          delay(1000);
          printUnpr(p, colorEmpty);
          break;
        case 1:
          printUnpr(pl2, colorWall);
          delay(1000);
          printUnpr(p, colorEmpty);
          break;
        case 2:
          printUnpr(pl3, colorWall);
          delay(1000);
          printUnpr(p, colorEmpty);
          break;
        case 3:
          printUnpr(pl4, colorWall);
          delay(1000);
          printUnpr(p, colorEmpty);
          break;
      }
      break;
    case 3: //scambiarsi di posto con qualcuno
      printUnpr(unpr, colorMalus);
      delay(1000);
      printUnpr(unpr, colorEmpty);
      break;
  }
}

void checkUnpr(Point unpr, Point pl1 = {-1, -1}, Point pl2 = {-1, -1}, Point pl3 = {-1, -1}, Point pl4 = {-1, -1}){
  if(pl1.x == unpr.x && pl1.y == unpr.y){
    getUnpr(unpr);
  }
  else if(pl2.x == unpr.x && pl2.y == unpr.y){
    getUnpr(unpr);
  }
  else if(pl3.x == unpr.x && pl3.y == unpr.y){
    getUnpr(unpr);
  }
  else if(pl4.x == unpr.x && pl4.y == unpr.y){
    getUnpr(unpr);
  }
}

void printUnpr(Point unpr, uint32_t color){
  for (uint8_t k = 0; k < RESOLUTION; k++) {
    strip.setPixelColor(getPixelIndex(unpr.y, unpr.x, k), color);
  }
}

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
  //stampa sulla seriale per debug e per l'ESP
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

void resetCalibration (uint8_t _delay = 1000){
  Serial.println("Inizio Procedura di Setup/Reset...");
  Serial.println("Assicurati che TUTTE le pedine siano rimosse!");
  
  // Un piccolo delay per dare il tempo di togliere le mani/pedine se necessario
  Serial.print(".");
  delay(_delay /6);
  Serial.print(".");
  delay(_delay /6);
  Serial.println(".");
  delay(_delay /6);

  Serial.print(".");
  delay(_delay /6);
  Serial.print(".");
  delay(_delay /6);
  Serial.println(".");
  delay(_delay /6);

  // Eseguiamo direttamente la calibrazione senza aspettare pulsanti fisici, 
  // dato che il comando è arrivato via seriale.
  Serial.println("Calibrazione in corso... NON avvicinare magneti!");

  randomSeed(analogRead(A2));

  strip.clear();
  strip.show();

  calibrateSensors();

  Serial.println("Calibrazione completata!");
  Serial.println("Posizionare le pedine agli angoli della partita e inviare comando START");
}

void gamePlay(){
  delay(200);
  magnetDetection(); // Cerco i magneti e stampo su seriale
  
  player1 = {-1, -1};
  player2 = {-1, -1};
  player3 = {-1, -1};
  player4 = {-1, -1};

  // Controllo se il punto esiste (intensità > 0) prima di aggiornare i player
  if (podiumValue[0].intensity > 0) player1 = {podiumValue[0].x, podiumValue[0].y};
  if (podiumValue[1].intensity > 0) player2 = {podiumValue[1].x, podiumValue[1].y};
  if (podiumValue[2].intensity > 0) player3 = {podiumValue[2].x, podiumValue[2].y};
  if (podiumValue[3].intensity > 0) player4 = {podiumValue[3].x, podiumValue[3].y};
}

void processSerialCommands() {
  // 1. Clausola di guardia: se non ci sono abbastanza byte, usciamo subito.
  if (Serial.available() < 4) {
    return;
  }

  // 2. Sincronizzazione: se il primo byte non è START, lo scartiamo.
  if (Serial.peek() != SERIAL_START_BYTE) {
    Serial.read(); // Rimuove la spazzatura dal buffer
    return;        // Usciamo per far ripartire il controllo al prossimo giro
  }

  // 3. Lettura dei dati (sappiamo per certo che il primo byte è START)
  Serial.read(); // Consumiamo il byte di start
  uint8_t cmd = Serial.read();
  uint8_t len = Serial.read();
  uint8_t chk = Serial.read();

  // 4. Verifica Checksum
  if (chk != (cmd ^ len)) {
    Serial.println("Errore: Checksum Seriale non valido!");
    return; // Interrompiamo l'esecuzione qui se il pacchetto è corrotto
  }

  // 5. Esecuzione dei comandi tramite switch (più pulito degli if/else)
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

    default:
      // Opzionale: gestire comandi non riconosciuti
      Serial.println("Avviso: Comando sconosciuto ignorato.");
      break;
  }
}

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

    
    // Pre-calcoliamo i colori una volta sola all'avvio!
    colorWall   = strip.Color(255, 0, 0);       // Rosso
    colorEmpty  = strip.Color(180, 160, 180);    // Bianco debole
    colorGoal   = strip.Color(0, 0, 50);     // Blu
    colorUnpr  = strip.Color(255, 255, 0);  //Giallo
    colorBonus = strip.Color(0, 255, 0);    //Verde
    colorMalus  = strip.Color(255, 0, 220); //Viola

    //riempio sensorBaseline con il valore di base per quello specifico sensore
    calibrateSensors();
    Serial.println("Calibrazione completata!");
}

void loop() {
  processSerialCommands();
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousGameMillis >= gameInterval) {
    previousGameMillis = currentMillis; 

    gamePlay();
    
    printUnpr(unpr, colorUnpr);
    checkUnpr(unpr, player1, player2, player3, player4);

    strip.show();
  }
}