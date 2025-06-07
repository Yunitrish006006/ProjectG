#include "LedController.h"

LedController::LedController(int neopixelPin, int numPixels, int redPin, int greenPin, int bluePin, bool isCommonAnode)
    : _neopixelPin(neopixelPin), _numPixels(numPixels),
      _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin),
      _pixels(numPixels, neopixelPin, NEO_GRB + NEO_KHZ800)
{
}

void LedController::begin()
{
  _pixels.begin();
  _pixels.setBrightness(_savedBrightness);
  _pixels.clear();
  _pixels.show();

  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BLUE_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(_redPin, RED_CHANNEL);
  ledcAttachPin(_greenPin, GREEN_CHANNEL);
  ledcAttachPin(_bluePin, BLUE_CHANNEL);

  _isOn = true;
  _lastR = _lastG = _lastB = 255;
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b)
{
  _lastR = r;
  _lastG = g;
  _lastB = b;

  float brightnessRatio = _savedBrightness / 255.0;
  uint8_t adjustedR = round(r * brightnessRatio);
  uint8_t adjustedG = round(g * brightnessRatio);
  uint8_t adjustedB = round(b * brightnessRatio);

  uint32_t neoPixelColor = _pixels.Color(r, g, b);
  _pixels.setPixelColor(0, neoPixelColor);
  _pixels.show();

  setRGBColor(adjustedR, adjustedG, adjustedB);
}

void LedController::setRGBColor(int r, int g, int b)
{

  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
}

void LedController::off()
{

  if (_savedBrightness == 0)
  {
    _savedBrightness = 128;
  }

  _isOn = false;

  uint8_t tempBrightness = _pixels.getBrightness();
  _pixels.setBrightness(0);
  _pixels.clear();
  _pixels.show();
  _pixels.setBrightness(tempBrightness);

  ledcWrite(RED_CHANNEL, 0);
  ledcWrite(GREEN_CHANNEL, 0);
  ledcWrite(BLUE_CHANNEL, 0);
}

void LedController::on()
{
  _isOn = true;

  if (_lastR == 0 && _lastG == 0 && _lastB == 0)
  {
    _lastR = _lastG = _lastB = 255;
  }

  float brightnessRatio = _savedBrightness / 255.0;
  uint8_t adjustedR = round(_lastR * brightnessRatio);
  uint8_t adjustedG = round(_lastG * brightnessRatio);
  uint8_t adjustedB = round(_lastB * brightnessRatio);
  if (_savedBrightness == 0)
  {
    _savedBrightness = 128;
  }

  _pixels.setBrightness(_savedBrightness);
  uint32_t neoPixelColor = _pixels.Color(_lastR, _lastG, _lastB);
  _pixels.setPixelColor(0, neoPixelColor);
  _pixels.show();
  int r = adjustedR;
  int g = adjustedG;
  int b = adjustedB;

  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
}

// 設置引腳配置
void LedController::setPinConfig(int neopixelPin, int redPin, int greenPin, int bluePin)
{
  ledcDetachPin(_redPin);
  ledcDetachPin(_greenPin);
  ledcDetachPin(_bluePin);

  _neopixelPin = neopixelPin;
  _redPin = redPin;
  _greenPin = greenPin;
  _bluePin = bluePin;

  _pixels = Adafruit_NeoPixel(_numPixels, neopixelPin, NEO_GRB + NEO_KHZ800);
  _pixels.begin();
  _pixels.setBrightness(_savedBrightness);
  _pixels.show();

  ledcAttachPin(_redPin, RED_CHANNEL);
  ledcAttachPin(_greenPin, GREEN_CHANNEL);
  ledcAttachPin(_bluePin, BLUE_CHANNEL);
}

// 測試LED引腳
void LedController::testPins()
{
  Serial.println("開始測試LED引腳 - 紅色");
  setColor(255, 0, 0);
  delay(1000);

  Serial.println("測試LED引腳 - 綠色");
  setColor(0, 255, 0);
  delay(1000);

  Serial.println("測試LED引腳 - 藍色");
  setColor(0, 0, 255);
  delay(1000);

  Serial.println("測試LED引腳 - 白色");
  setColor(255, 255, 255);
  delay(1000);
  Serial.println("測試LED引腳 - 熄滅");
  off();
}

void LedController::setBrightness(uint8_t brightness)
{
  _savedBrightness = constrain(brightness, 0, 255);

  if (_isOn)
  {
    _pixels.setBrightness(_savedBrightness);
    uint32_t neoPixelColor = _pixels.Color(_lastR, _lastG, _lastB);
    _pixels.setPixelColor(0, neoPixelColor);
    _pixels.show();
    float brightnessRatio = _savedBrightness / 255.0;
    uint8_t scaledR = round(_lastR * brightnessRatio);
    uint8_t scaledG = round(_lastG * brightnessRatio);
    uint8_t scaledB = round(_lastB * brightnessRatio);
    setRGBColor(scaledR, scaledG, scaledB);
  }
}

uint8_t LedController::getBrightness() const
{
  return _savedBrightness;
}
