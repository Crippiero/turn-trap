#include <SoftwareSerial.h>

// Imposta i pin per la ricezione:
// RX = Pin 10 (Da collegare al TX dell'altro Arduino, il pin 7)
// TX = Pin 11 (Da collegare all'RX dell'altro Arduino, il pin 6)
SoftwareSerial swSerial(10, 11);

void setup() {
  // Inizializza la seriale hardware (quella collegata al PC)
  Serial.begin(57600);
  
  // Inizializza la seriale software (quella collegata al primo Arduino)
  swSerial.begin(57600);
  
  Serial.println("--- Ricevitore Neopixel Avviato ---");
  Serial.println("In attesa di comandi...");
}

void loop() {
  // Controlla se ci sono dati in arrivo dal primo Arduino
  if (swSerial.available()) {
    
    // Legge l'intera riga fino al carattere di "a capo" (\n)
    String comando = swSerial.readStringUntil('\n');
    
    // Pulisce la stringa da eventuali spazi vuoti o ritorni a capo extra (\r)
    comando.trim();
    
    // Se la stringa non è vuota, la stampa sul Monitor Seriale
    if (comando.length() > 0) {
      Serial.print("Ricevuto: ");
      Serial.println(comando);
    }
  }
}