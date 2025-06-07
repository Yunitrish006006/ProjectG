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
  delay(1500);

  Serial.println("測試LED引腳 - 綠色");
  setColor(0, 255, 0);
  delay(1500);

  Serial.println("測試LED引腳 - 藍色");
  setColor(0, 0, 255);
  delay(1500);

  Serial.println("測試LED引腳 - 白色");
  setColor(255, 255, 255);
  delay(1500);

  Serial.println("測試LED引腳 - 漸變");
  // 快速展示漸變效果
  for (int i = 0; i < 256; i += 10)
  {
    setColor(255 - i, i, 128);
    delay(50);
  }

  Serial.println("測試LED引腳 - 熄滅");
  off();
  delay(500);

  Serial.println("LED測試完成");
}

void LedController::setBrightness(uint8_t brightness)
{
  // 確保亮度值在有效範圍內（1-255）
  // NeoPixel庫實際上在setBrightness的內部實現中會把0當作最大亮度
  // 因此我們這裡確保最小值為1，避免意外的行為
  _savedBrightness = constrain(brightness, 1, 255);

  if (_isOn)
  {
    // 使用NeoPixel庫的亮度控制
    _pixels.setBrightness(_savedBrightness);
    _pixels.show(); // 顯示更新後的亮度

    // 計算亮度比例
    float brightnessRatio = _savedBrightness / 255.0;

    // 根據亮度比例縮放RGB值，保持顏色相同
    uint8_t scaledR = round(_lastR * brightnessRatio);
    uint8_t scaledG = round(_lastG * brightnessRatio);
    uint8_t scaledB = round(_lastB * brightnessRatio);

    // 更新PWM通道的值，實現外部RGB LED的亮度控制
    setRGBColor(scaledR, scaledG, scaledB);
  }
}

uint8_t LedController::getBrightness() const
{
  return _savedBrightness;
}

// 呼吸燈效果實現
void LedController::breathe(uint8_t r, uint8_t g, uint8_t b,
                            int steps, int duration, bool colorShifting)
{
  uint8_t brightness;
  int delayTime = duration / (steps * 2); // 計算每步的延遲時間

  // 確保LED是開啟狀態
  on();
  setColor(r, g, b);

  // 漸亮過程 - 使用sin函數使變化更自然
  for (int i = 0; i < steps; i++)
  {
    // 使用正弦函數產生更自然的亮度變化曲線 (5-255)
    brightness = 5 + (sin(i * 3.14159 / steps) * 250);

    setBrightness(brightness);

    // 顏色微調 - 使LED有輕微的顏色變化，更加生動
    if (colorShifting && (i % 5 == 0))
    {
      r = constrain(r + random(-3, 4), 30, 255);
      g = constrain(g + random(-3, 4), 30, 255);
      b = constrain(b + random(-3, 4), 30, 255);
      setColor(r, g, b);
    }

    delay(delayTime);
  }

  delay(delayTime * 5); // 在最高亮度停留短暫時間

  // 漸暗過程
  for (int i = steps; i >= 0; i--)
  {
    // 使用正弦函數產生更自然的亮度變化曲線
    brightness = 5 + (sin(i * 3.14159 / steps) * 250);

    setBrightness(brightness);

    // 顏色微調
    if (colorShifting && (i % 5 == 0))
    {
      r = constrain(r + random(-3, 4), 30, 255);
      g = constrain(g + random(-3, 4), 30, 255);
      b = constrain(b + random(-3, 4), 30, 255);
      setColor(r, g, b);
    }

    delay(delayTime);
  }
}

void LedController::stopBreathe()
{
  // 可以用來中止呼吸燈效果的方法
  // 目前簡單實現為恢復到一個固定亮度
  setBrightness(128);
}

/**
 * 產生鮮豔的顏色組合
 *
 * @param r 紅色值的參考，將被修改
 * @param g 綠色值的參考，將被修改
 * @param b 藍色值的參考，將被修改
 * @param colorMode 顏色模式 (0-5)，用於選擇不同的顏色主題
 */
void LedController::getVividColor(uint8_t &r, uint8_t &g, uint8_t &b, int colorMode)
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
