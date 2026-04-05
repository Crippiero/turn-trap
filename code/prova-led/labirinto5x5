#include <Adafruit_NeoPixel.h>

#define NUM_STRIPS 5
#define NUMPIXELS 5
#define BRIGHTNESS 100

const int PINS[NUM_STRIPS] = {3, 4, 5, 6, 7};

Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUMPIXELS, PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUMPIXELS, PINS[4], NEO_GRB + NEO_KHZ800)
};

// ==========================================
// 🎨 IL TUO DISEGNO (MODIFICALO DA QUI!)
// ==========================================
// Legenda dei colori:
// 'R' = Rosso
// 'W' = Bianco
// 'B' = Blu scuro
// 'O' = Arancione
// 'N' = Nero (Spento)

char disegno[NUM_STRIPS][NUMPIXELS] = {
  {'R', 'R', 'W', 'W', 'R'}, // Striscia 1 (Pin 3)
  {'W', 'R', 'R', 'W', 'O'}, // Striscia 2 (Pin 4)
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
  // Il programma legge la matrice riga per riga e accende i LED
  for(int riga = 0; riga < NUM_STRIPS; riga++) {
    for(int colonna = 0; colonna < NUMPIXELS; colonna++) {
      
      // Legge la lettera dalla matrice
      char letteraColore = disegno[riga][colonna]; 
      
      // Traduce la lettera nel colore effettivo
      uint32_t coloreReale = decodificaColore(letteraColore, riga); 
      
      // Assegna il colore al pixel
      strips[riga].setPixelColor(colonna, coloreReale);
    }
    strips[riga].show(); // Mostra i cambiamenti per questa striscia
  }
  
  delay(100); // Un piccolo ritardo per stabilità (opzionale)
}

// --------------------------------------------------------
// Funzione che "traduce" le lettere nei colori di NeoPixel
// --------------------------------------------------------
uint32_t decodificaColore(char lettera, int indiceStriscia) {
  // Usiamo strips[indiceStriscia].Color per generare il codice colore corretto
  switch(lettera) {
    case 'R': 
      return strips[indiceStriscia].Color(255, 0, 0);      // Rosso pieno
    case 'W': 
      return strips[indiceStriscia].Color(255, 255, 255);  // Bianco
    case 'B': 
      return strips[indiceStriscia].Color(0, 0, 50);       // Blu (valore basso = luminosità 50 come nel tuo codice)
    case 'O': 
      return strips[indiceStriscia].Color(255, 180, 0);    // Arancione
    case 'N': 
      return strips[indiceStriscia].Color(0, 0, 0);        // Nero / Spento
    default:  
      return strips[indiceStriscia].Color(0, 0, 0);        // Sicurezza: se scrivi una lettera sbagliata, lo spegne
  }
}
