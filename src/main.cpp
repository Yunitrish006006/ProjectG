#include <Arduino.h>
#include "LedController.h"
#include "OledDisplay.h"

const int LED_PIN = LED_BUILTIN;
// ESP32-S3 N16R8 常用 I2C 腳位
const int I2C_SDA = 8; // GPIO8 是 ESP32-S3 N16R8 的默認 SDA
const int I2C_SCL = 9; // GPIO9 是 ESP32-S3 N16R8 的默認 SCL

LedController ledController;
OledDisplay oled(I2C_SDA, I2C_SCL); // 使用 ESP32-S3 N16R8 默認 I2C 引腳
uint8_t r, g, b;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  randomSeed(analogRead(A0));
  pinMode(LED_PIN, OUTPUT);

  // 初始化 OLED 顯示
  oled.begin();

  // 僅顯示系統信息
  oled.showSimpleSystemInfo();

  // 初始化 LED 控制器
  // 為 ESP32-S3 N16R8 設置適合的 LED 引腳
  // NeoPixel: GPIO48, RGB LED: GPIO35 (紅), GPIO36 (綠), GPIO37 (藍)
  ledController.setPinConfig(48, 35, 36, 37);
  ledController.begin();

  // LED測試 - 但不顯示詳細信息在OLED上
  ledController.setColor(255, 0, 0); // RED
  delay(500);

  ledController.setColor(0, 255, 0); // GREEN
  delay(500);

  ledController.setColor(0, 0, 255); // BLUE
  delay(500);

  ledController.setColor(255, 255, 255); // WHITE
  delay(500);
  // 快速展示漸變效果
  for (int i = 0; i < 256; i += 16)
  {
    ledController.setColor(255 - i, i, 128);
    delay(50);
  }

  // 測試完成
  ledController.off();
  delay(500);

  // 更新系統信息顯示
  oled.showSimpleSystemInfo();
}

void loop()
{
  // 生成新的顏色
  ledController.getVividColor(r, g, b, random(0, 6));

  // 只在OLED上顯示系統信息
  oled.showSimpleSystemInfo();

  // 執行呼吸燈效果
  ledController.breathe(r, g, b, 100, 4000, true);

  // 設置低亮度狀態
  ledController.setBrightness(20);

  // 更新系統信息
  oled.showSimpleSystemInfo();

  delay(2000);

  // 有 30% 機率使用固定顏色
  if (random(10) > 7)
  {
    uint8_t primaryColor = random(0, 3);
    uint8_t fixedR = (primaryColor == 0) ? 255 : 30;
    uint8_t fixedG = (primaryColor == 1) ? 255 : 30;
    uint8_t fixedB = (primaryColor == 2) ? 255 : 30;

    // 繼續只顯示系統信息
    oled.showSimpleSystemInfo();

    // 執行固定顏色的呼吸燈效果
    ledController.breathe(fixedR, fixedG, fixedB, 80, 3000, false);
  }
}