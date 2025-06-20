#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class LedController
{
public:
  LedController(int neopixelPin = 33, int numPixels = 1,
                int redPin = 12, int greenPin = 13, int bluePin = 14,
                bool isCommonAnode = false);
  void begin();
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void off();
  void on(); // 亮度控制方法
  void setBrightness(uint8_t brightness);
  uint8_t getBrightness() const;
  // 呼吸燈效果方法
  void breathe(uint8_t r, uint8_t g, uint8_t b,
               int steps = 100, int duration = 4000, bool colorShifting = true);
  void stopBreathe(); // 停止呼吸燈效果

  // 顏色生成方法
  void getVividColor(uint8_t &r, uint8_t &g, uint8_t &b, int colorMode = -1);

  // 診斷方法
  void testPins();
  void setPinConfig(int neopixelPin, int redPin, int greenPin, int bluePin);
  void setCommonAnode(bool isCommonAnode);

private:
  Adafruit_NeoPixel _pixels;
  int _neopixelPin;
  int _numPixels;

  int _redPin;
  int _greenPin;
  int _bluePin;
  // 保存當前狀態的變數
  bool _isOn = true;
  uint8_t _savedBrightness = 50;
  uint8_t _lastR = 0;
  uint8_t _lastG = 0;
  uint8_t _lastB = 0;

  // ESP32-S3 PWM設置
  static const int PWM_FREQ = 5000;
  static const int PWM_RESOLUTION = 8;
  // 使用較高的通道號，避免與系統其他功能衝突
  static const int RED_CHANNEL = 4;
  static const int GREEN_CHANNEL = 5;
  static const int BLUE_CHANNEL = 6;

  void setRGBColor(int r, int g, int b);
};

#endif // LED_CONTROLLER_H
