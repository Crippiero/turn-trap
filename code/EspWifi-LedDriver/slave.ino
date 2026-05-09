#include <Adafruit_NeoPixel.h>

#define NUM_LEDS 11
#define NUM_STRIPS 11 // 12 Strisce in totale (dai Pin 2 al 12)

// Creiamo un array per contenere le striscie. 
Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUM_LEDS, 2, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 3, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 4, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 5, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 6, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 7, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 8, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 9, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 10, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 11, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, 12, NEO_GRB + NEO_KHZ800),
};

String packetBuffer = "";

void setup() {
  // Avvia la comunicazione con il Master
  // Assicurati che il TX del Master sia collegato all'RX (Pin 0) di questo Slave
  Serial.begin(9600); 
  
  // Riserva un po' di memoria per evitare frammentazione della RAM
  packetBuffer.reserve(50);

  // Inizializza tutte le strisce
  for(int i = 0; i < NUM_STRIPS; i++) {
    strips[i].begin();
    strips[i].show(); // Assicurati che partano spente
  }
}

void loop() {
  // Leggi i dati in arrivo dal Master
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '<') {
      packetBuffer = ""; // Inizio di un nuovo pacchetto, pulisci il buffer
    } 
    else if (c == '>') {
      processCommand(packetBuffer); // Fine del pacchetto, eseguilo!
    } 
    else {
      packetBuffer += c; // Accumula i caratteri
    }
  }
}

// --- LOGICA DI DECODIFICA E DISEGNO ---

void processCommand(String packet) {
  // Suddivide il pacchetto separato da virgole
  char cmdType = packet.charAt(0);

  if (cmdType == 'R') {
    // Ricezione di una riga: R,indiceRiga,caratteri
    int firstComma = packet.indexOf(',');
    int secondComma = packet.indexOf(',', firstComma + 1);
    
    if (firstComma != -1 && secondComma != -1) {
      int rIdx = packet.substring(firstComma + 1, secondComma).toInt();
      String rowData = packet.substring(secondComma + 1);
      
      if (rIdx >= 0 && rIdx < NUM_STRIPS) {
        for (int x = 0; x < NUM_LEDS && x < rowData.length(); x++) {
          char cell = rowData.charAt(x);
          uint32_t color = getColorFromChar(cell, rIdx);
          strips[rIdx].setPixelColor(x, color);
        }
      }
    }
  } 
  else if (cmdType == 'U') {
    // Imprevisto o Evento singolo: U,X,Y,Colore
    int c1 = packet.indexOf(',');
    int c2 = packet.indexOf(',', c1 + 1);
    int c3 = packet.indexOf(',', c2 + 1);
    
    if (c1 != -1 && c2 != -1 && c3 != -1) {
      int x = packet.substring(c1 + 1, c2).toInt();
      int y = packet.substring(c2 + 1, c3).toInt(); // Y corrisponde alla striscia
      char colorCode = packet.charAt(c3 + 1);
      
      if (y >= 0 && y < NUM_STRIPS && x >= 0 && x < NUM_LEDS) {
        uint32_t color = getColorFromChar(colorCode, y);
        strips[y].setPixelColor(x, color);
      }
    }
  } 
  else if (cmdType == 'C') {
    // Pulisci tutte le strisce
    for(int i = 0; i < NUM_STRIPS; i++) {
      strips[i].clear();
    }
  } 
  else if (cmdType == 'S') {
    // Mostra (Applica le modifiche fisicamente ai LED)
    for(int i = 0; i < NUM_STRIPS; i++) {
      strips[i].show();
    }
  }
}

// --- MAPPATURA COLORI ---

uint32_t getColorFromChar(char c, int stripIndex) {
  // Nota: usiamo strips[stripIndex].Color per generare il formato colore corretto.
  // Puoi modificare i valori (R, G, B) a tuo piacimento.
  
  switch (c) {
    case '#': return strips[stripIndex].Color(255, 0, 0);     // Muro = Rosso
    case 'x': return strips[stripIndex].Color(20, 20, 20);    // Vuoto = Bianco tenue
    case 'E': return strips[stripIndex].Color(20, 20, 20);    // Empty (pulizia imprevisto)
    case 'G': return strips[stripIndex].Color(150, 0, 200);   // Goal = Viola
    case 'U': return strips[stripIndex].Color(255, 255, 0);   // Bonus = Giallo
    case 'B': return strips[stripIndex].Color(255, 255, 0);   // Bonus = Giallo
    case 'M': return strips[stripIndex].Color(255, 128, 0);   // Malus = Arancione
    case 'W': return strips[stripIndex].Color(255, 128, 0);   // Malus = Arancione
    default:  return strips[stripIndex].Color(0, 0, 0);       // Sicurezza = Spento
  }
}