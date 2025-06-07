#include "LedController.h"

LedController::LedController(int neopixelPin, int numPixels, int redPin, int greenPin, int bluePin)
  : _neopixelPin(neopixelPin), _numPixels(numPixels),
    _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin),
    _pixels(numPixels, neopixelPin, NEO_GRB + NEO_KHZ800) {
}

void LedController::begin() {
  _pixels.begin();
  _pixels.setBrightness(50);
  _pixels.show();
  
  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BLUE_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  
  ledcAttachPin(_redPin, RED_CHANNEL);
  ledcAttachPin(_greenPin, GREEN_CHANNEL);
  ledcAttachPin(_bluePin, BLUE_CHANNEL);
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t neoPixelColor = _pixels.Color(r, g, b);
  _pixels.setPixelColor(0, neoPixelColor);
  _pixels.show();
  setRGBColor(r, g, b);
}

void LedController::setRGBColor(int r, int g, int b) {
  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
}

void LedController::off() {
  _pixels.setBrightness(0);
  _pixels.clear();
  _pixels.show();
}
