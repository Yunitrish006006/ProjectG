#include <Arduino.h>
#include "LedController.h"

const int LED_PIN = LED_BUILTIN;
LedController ledController;
bool ledState = false;

void setup() {
  Serial.begin(115200);
  Serial.println("彩色LED測試程序啟動中");
  pinMode(LED_PIN, OUTPUT);
  randomSeed(analogRead(A0));
  ledController.begin();
}

void loop() {
  if (ledState) {
    ledController.off();
    delay(1000);  
  } else {
    ledController.setColor(random(256), random(256), random(256));
  }
  ledState = !ledState;
  delay(1000);  
}