#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// 顏色定義
enum LedColor {
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE,
  COLOR_YELLOW,
  COLOR_PURPLE,
  COLOR_CYAN,
  COLOR_WHITE,
  COLOR_COUNT
};

// RGB顏色結構
struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  const char* name;
};

class LedController {
public:
  // 構造函數
  LedController(int neopixelPin = 33, int numPixels = 1, 
                int redPin = 12, int greenPin = 13, int bluePin = 14);
  
  // 初始化
  void begin();
  
  // 使用預定義顏色枚舉設置顏色
  void setColor(LedColor color);
  
  // 使用RGB值設置顏色
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  
  // 循環到下一個顏色
  void nextColor();
  
  // 獲取當前顏色名稱
  const char* getCurrentColorName() const;
  
  // 取得預定義顏色的RGB值
  static RgbColor getColorRgb(LedColor color);

private:
  // NeoPixel相關
  Adafruit_NeoPixel _pixels;
  int _neopixelPin;
  int _numPixels;
  
  // 外部RGB LED相關
  int _redPin;
  int _greenPin;
  int _bluePin;
    // PWM通道
  static const int PWM_FREQ = 5000;
  static const int PWM_RESOLUTION = 8;
  static const int RED_CHANNEL = 0;
  static const int GREEN_CHANNEL = 1;
  static const int BLUE_CHANNEL = 2;
  
  // 顏色陣列
  static const RgbColor COLOR_MAP[COLOR_COUNT];
  
  // 顏色索引
  int _currentColorIndex;
  
  // 設置外部RGB LED顏色
  void setRGBColor(int r, int g, int b);
};

#endif // LED_CONTROLLER_H
