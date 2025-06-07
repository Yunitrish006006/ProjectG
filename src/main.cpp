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
    digitalWrite(LED_PIN, LOW);
    Serial.println("內建LED關閉");
  } else {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("內建LED開啟");
    uint8_t r = random(256);
    uint8_t g = random(256);
    uint8_t b = random(256);
    ledController.setColor(r, g, b);
  }
  ledState = !ledState;
  delay(1000);  
}