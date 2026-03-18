#include <MuxControl.h>
#include <Adafruit_NeoPixel.h>

#define PIN 8
#define NUMPIXELS 16

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
Mux myMux; 

int myReadings[16]; 

void setup() {
  Serial.begin(9600);
  myMux.setupMux(2, 3, 4, 5, A0);
  strip.begin();
  strip.clear();
}

void loop() {
  myMux.fullMuxAnalogRead(16, myReadings);
  int maxIndex = 0;
  int maxValue = myReadings[0];
  
  for(int i = 0; i < 16; i++) {
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
  strip.setPixelColor(maxIndex, strip.Color(255, 0, 0));
  strip.show();
  
  Serial.println("---");
  delay(100);
}
