#include <MuxControl.h>
#include <Adafruit_NeoPixel.h>

#define PIN 8
#define NUMPIXELS 16
#define SIZE 16
#define DELAY 1000

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
Mux myMux; 

int myReadings[SIZE]; 

void setup() {
  Serial.begin(9600);

  myMux.setupMux(2, 3, 4, 5, A0);

  strip.begin();
  strip.clear();
}

void loop() {
  myMux.fullMuxAnalogRead(SIZE, myReadings);
  int maxIndex = 0, maxValue = 0;
  
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
  if(maxValue < 400 || maxValue > 600){
    strip.setPixelColor(maxIndex, strip.Color(255, 0, 0));
    strip.show();
  }

  Serial.println("---");
  serial.print("maxValue:");
  Serial.println(maxValue);
  serial.print("maxIndex:");
  Serial.println(maxIndex);
  Serial.println("---");

  delay(DELAY);
}
