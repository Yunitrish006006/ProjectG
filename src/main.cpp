#include <Arduino.h>
#include "LedController.h"

const int LED_PIN = LED_BUILTIN;
LedController ledController;
uint8_t r, g, b;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  randomSeed(analogRead(A0));
  pinMode(LED_PIN, OUTPUT);
  ledController.setPinConfig(33, 12, 13, 14);
  ledController.begin();

  ledController.testPins();

  delay(1000);
}

void loop()
{
  ledController.getVividColor(r, g, b, random(0, 6));
  ledController.breathe(r, g, b, 100, 4000, true);
  ledController.setBrightness(20);
  delay(1000);
  if (random(10) > 7)
  {
    uint8_t primaryColor = random(0, 3);
    uint8_t fixedR = (primaryColor == 0) ? 255 : 30;
    uint8_t fixedG = (primaryColor == 1) ? 255 : 30;
    uint8_t fixedB = (primaryColor == 2) ? 255 : 30;
    ledController.breathe(fixedR, fixedG, fixedB, 80, 3000, false);
  }
}