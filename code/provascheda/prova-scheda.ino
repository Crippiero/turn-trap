#include <Adafruit_NeoPixel.h>

#define NUMPIXELS 2
#define PIN 5

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);



void setup() {
  strip.begin();
  strip.clear(); 
  Serial.begin(9600);
}

void loop() {
  int valoreAnalogico = analogRead(A3);
  Serial.print("Valore letto su A3: ");
  Serial.println(valoreAnalogico);
  strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.show();
  strip.setPixelColor(1, strip.Color(0, 0, 255));
  strip.show();
  delay(1000);
  strip.setPixelColor(1, strip.Color(255, 0, 0));
  strip.show();
  strip.setPixelColor(0, strip.Color(0, 0, 255));
  strip.show();
  delay(1000);
}
