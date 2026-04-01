#include <Adafruit_NeoPixel.h>

#define PIN1 3
#define PIN2 5
#define NUMPIXELS 5

Adafruit_NeoPixel strip1(NUMPIXELS, PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUMPIXELS, PIN2, NEO_GRB + NEO_KHZ800);
void setup() {
  strip1.begin();
  strip1.clear();
  strip2.begin();
  strip2.clear();

}

void loop() {
  for(int i=0; i<=NUMPIXELS; i++){
    strip1.setPixelColor(i, strip1.Color(255, 0, 255));
    strip1.show();
    strip2.setPixelColor(i, strip2.Color(255, 0, 255));
    strip2.show();

  }

}
