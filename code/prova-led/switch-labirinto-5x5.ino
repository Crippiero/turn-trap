#include <Adafruit_NeoPixel.h>

#define NUM_STRIPS 5
#define NUMPIXELS 5
#define BRIGHTNESS 100

const int PINS[NUM_STRIPS] = {3, 4, 5, 6, 7};
const int PinButton = 2; // Pin del pulsante
bool on = false;

Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUMPIXELS, PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[4], NEO_GRB + NEO_KHZ800)
};

// %--------------------------------------------------------------%
// --% Funzione che "traduce" le lettere nei colori di NeoPixel %--
// %--------------------------------------------------------------%

uint32_t decodificaColore(char lettera, int indiceStriscia) {
  // Usiamo strips[indiceStriscia].Color per generare il codice colore corretto
  switch(lettera) {
    case 'R': 
      return strips[indiceStriscia].Color(255, 0, 0);      // Rosso pieno
    case 'W': 
      return strips[indiceStriscia].Color(255, 255, 255);  // Bianco
    case 'B': 
      return strips[indiceStriscia].Color(0, 0, 50);       // Blu (valore basso = luminosità 50 come nel tuo codice)
    case 'G': 
      return strips[indiceStriscia].Color(255, 180, 0);    // Giallo
    case 'N': 
      return strips[indiceStriscia].Color(0, 0, 0);        // Nero / Spento
    default:  
      return strips[indiceStriscia].Color(0, 0, 0);        // Sicurezza: se scrivi una lettera sbagliata, lo spegne
  }
}

// ==========================================
// 🎨 IL TUO DISEGNO (MODIFICALO DA QUI!)
// ==========================================
// Legenda dei colori:
// 'R' = Rosso
// 'W' = Bianco
// 'B' = Blu scuro
// 'G' = Giallo
// 'N' = Nero (Spento)

char disegno1[NUM_STRIPS][NUMPIXELS] = {
  {'R', 'R', 'W', 'W', 'R'}, // Striscia 1 (Pin 3)
  {'W', 'R', 'R', 'W', 'G'}, // Striscia 2 (Pin 4)
  {'W', 'W', 'B', 'R', 'W'}, // Striscia 3 (Pin 5)
  {'W', 'R', 'W', 'W', 'W'}, // Striscia 4 (Pin 6)
  {'W', 'W', 'R', 'R', 'W'}  // Striscia 5 (Pin 7)
};
// ==========================================

char disegno2[NUM_STRIPS][NUMPIXELS] = {
  {'R', 'R', 'W', 'W', 'R'}, // Striscia 1 (Pin 3)
  {'W', 'R', 'R', 'W', 'G'}, // Striscia 2 (Pin 4)
  {'W', 'W', 'B', 'R', 'W'}, // Striscia 3 (Pin 5)
  {'W', 'R', 'W', 'W', 'W'}, // Striscia 4 (Pin 6)
  {'W', 'W', 'R', 'R', 'W'}  // Striscia 5 (Pin 7)
};
// ==========================================

void setup() {
  for(int i = 0; i < NUM_STRIPS; i++) {
    strips[i].begin();
    strips[i].setBrightness(BRIGHTNESS);
    strips[i].clear();
    strips[i].show();
  }
}

void loop() {

  if (PinButton == LOW) { // Se il pulsante è premuto
    on = !on; // Cambia lo stato (acceso/spento)
    delay(200); // Debounce del pulsante
  }

  if (on) {
    for(int riga = 0; riga < NUM_STRIPS; riga++) 
    {
      for(int colonna = 0; colonna < NUMPIXELS; colonna++) 
      {
        char letteraColore = disegno1[riga][colonna]; 
        uint32_t coloreReale = decodificaColore(letteraColore, riga); 
        strips[riga].setPixelColor(colonna, coloreReale);
      }
      strips[riga].show();
    }
    delay(100);
  } 
  else {
    for(int riga = 0; riga < NUM_STRIPS; riga++) 
    {
      for(int colonna = 0; colonna < NUMPIXELS; colonna++) 
      {
        char letteraColore = disegno2[riga][colonna]; 
        uint32_t coloreReale = decodificaColore(letteraColore, riga); 
        strips[riga].setPixelColor(colonna, coloreReale);
      }
      strips[riga].show();
    }
  }
}
