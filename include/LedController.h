#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>



class LedController {
public:
  LedController(int neopixelPin = 33, int numPixels = 1, 
                int redPin = 12, int greenPin = 13, int bluePin = 14);
  void begin();
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void off();
private:
  Adafruit_NeoPixel _pixels;
  int _neopixelPin;
  int _numPixels;
  
  int _redPin;
  int _greenPin;
  int _bluePin;

  static const int PWM_FREQ = 5000;
  static const int PWM_RESOLUTION = 8;
  static const int RED_CHANNEL = 0;
  static const int GREEN_CHANNEL = 1;
  static const int BLUE_CHANNEL = 2;

  void setRGBColor(int r, int g, int b);
};

#endif // LED_CONTROLLER_H
