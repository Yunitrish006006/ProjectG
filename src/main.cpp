#include <Arduino.h>
#include "LedController.h"

const int LED_PIN = LED_BUILTIN;
LedController ledController;
uint8_t r, g, b;

void getVividColor(uint8_t &r, uint8_t &g, uint8_t &b, int colorMode = -1)
{
  switch (colorMode)
  {
  case 0:
    r = random(180, 256);
    g = random(50);
    b = random(50);
    break;
  case 1:
    r = random(50);
    g = random(180, 256);
    b = random(50);
    break;
  case 2:
    r = random(50);
    g = random(50);
    b = random(180, 256);
    break;
  case 3:
    r = random(180, 256);
    g = random(100, 200);
    b = random(30);
    break;
  case 4:
    r = random(150, 256);
    g = random(20, 80);
    b = random(150, 256);
    break;
  case 5:
    r = random(20, 80);
    g = random(150, 256);
    b = random(150, 256);
    break;
  default:
    r = uint8_t(255);
    g = uint8_t(255);
    b = uint8_t(255);
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  randomSeed(analogRead(A0));
  pinMode(LED_PIN, OUTPUT);
  ledController.setPinConfig(33, 12, 13, 14);
  ledController.begin();
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  delay(1000);
  getVividColor(r, g, b, random(0, 6));
}

void loop()
{
  uint8_t brightness = 10;
  Serial.printf("設定鮮豔顏色 RGB: (%d, %d, %d)\n", r, g, b);

  ledController.on();
  ledController.setColor(r, g, b);
  for (; brightness <= 128; brightness += 3)
  {
    Serial.printf("設定亮度: %d\n", brightness);
    ledController.setBrightness(brightness);
    r = (r + random(-1, 1)) % 128;
    g = (g + random(-1, 1)) % 128;
    b = (b + random(-1, 1)) % 128;
    ledController.setColor(r, g, b);
    delay(30);
  }
  delay(500);
  for (; brightness > 4; brightness -= 3)
  {
    Serial.printf("設定亮度: %d\n", brightness);
    ledController.setBrightness(brightness);
    r = (r + random(-1, 1)) % 128;
    g = (g + random(-1, 1)) % 128;
    b = (b + random(-1, 1)) % 128;
    ledController.setColor(r, g, b);
    delay(30);
  }
  delay(500);
}