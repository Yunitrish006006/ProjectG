#include "LedController.h"

LedController::LedController(int neopixelPin, int numPixels, int redPin, int greenPin, int bluePin, bool isCommonAnode)
  : _neopixelPin(neopixelPin), _numPixels(numPixels),
    _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin),
    _isCommonAnode(isCommonAnode),
    _pixels(numPixels, neopixelPin, NEO_GRB + NEO_KHZ800) {
}

void LedController::begin() {
  Serial.println("初始化 LED 控制器");
  Serial.printf("引腳設置: NeoPixel=%d, R=%d, G=%d, B=%d\n", 
                _neopixelPin, _redPin, _greenPin, _bluePin);
  
  // 初始化 NeoPixel
  _pixels.begin();
  _pixels.setBrightness(_savedBrightness);
  _pixels.clear();  // 先清除所有像素
  _pixels.show();
  
  // 配置 PWM 通道
  Serial.printf("設置PWM通道: 頻率=%d, 解析度=%d位, 通道號=%d,%d,%d\n", 
                PWM_FREQ, PWM_RESOLUTION, RED_CHANNEL, GREEN_CHANNEL, BLUE_CHANNEL);
  
  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BLUE_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  
  // 將通道附加到 GPIO 引腳
  ledcAttachPin(_redPin, RED_CHANNEL);
  ledcAttachPin(_greenPin, GREEN_CHANNEL);
  ledcAttachPin(_bluePin, BLUE_CHANNEL);
  
  // 初始化狀態變量
  _isOn = true;  // 默認為開啟狀態
  _lastR = _lastG = _lastB = 255;  // 默認顏色為白色（最容易看見）
  
  // 將所有LED先設為白色，確保可以工作
  Serial.println("初始化測試：設置為白色");
  setRGBColor(255, 255, 255);
  delay(500);  // 短暫顯示白色
  
  // 然後關閉
  Serial.println("初始化測試完成，關閉LED");
  setRGBColor(0, 0, 0);
  
  Serial.println("LED 控制器初始化完成");
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b) {
  // 儲存當前設定的顏色
  _lastR = r;
  _lastG = g;
  _lastB = b;
  
  Serial.printf("設定顏色: R=%d, G=%d, B=%d, LED狀態: %s\n", 
                r, g, b, _isOn ? "開啟" : "關閉");
  
  // 如果目前是關閉狀態，只更新儲存的顏色，不實際顯示
  if (!_isOn) {
    Serial.println("LED處於關閉狀態，只保存顏色設定但不顯示");
    return;
  }
  
  // 設定並顯示顏色
  uint32_t neoPixelColor = _pixels.Color(r, g, b);
  _pixels.setPixelColor(0, neoPixelColor);
  _pixels.show();
  setRGBColor(r, g, b);
  
  Serial.println("顏色已更新並顯示");
}

void LedController::setRGBColor(int r, int g, int b) {
  // 如果是共陽極LED，需要反轉數值（255-值）
  if (_isCommonAnode) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  
  Serial.printf("寫入PWM通道: R(通道%d, 引腳%d)=%d, G(通道%d, 引腳%d)=%d, B(通道%d, 引腳%d)=%d\n", 
                RED_CHANNEL, _redPin, r,
                GREEN_CHANNEL, _greenPin, g,
                BLUE_CHANNEL, _bluePin, b);
  
  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
}

void LedController::off() {
  // 如果已經關閉，不需要再執行
  if (!_isOn) {
    Serial.println("LED已經是關閉狀態");
    return;
  }
  
  // 儲存當前亮度供之後恢復使用
  _savedBrightness = _pixels.getBrightness();
  Serial.println("保存當前亮度: " + String(_savedBrightness));
  
  // 先設置狀態，防止在方法執行過程中被重復調用
  _isOn = false;
  
  // 關閉 NeoPixel LED
  _pixels.setBrightness(0);
  _pixels.clear(); // 清除所有像素
  _pixels.show();
  
  // 同時關閉外部 RGB LED - 直接寫入通道，不經過setRGBColor
  Serial.println("直接寫入PWM通道值0以關閉LED");
  ledcWrite(RED_CHANNEL, 0);
  ledcWrite(GREEN_CHANNEL, 0);
  ledcWrite(BLUE_CHANNEL, 0);
  
  Serial.println("LED已關閉");
}

// 實現 on() 方法來恢復 LED 的顯示
void LedController::on() {
  // 先設置狀態，防止在方法執行過程中被重復調用
  _isOn = true;
  
  Serial.println("開啟LED，設置亮度: " + String(_savedBrightness));
  
  // 恢復NeoPixel亮度和顏色
  _pixels.setBrightness(_savedBrightness);
  uint32_t neoPixelColor = _pixels.Color(_lastR, _lastG, _lastB);
  _pixels.setPixelColor(0, neoPixelColor);
  _pixels.show();
  
  // 恢復外部 RGB LED 顏色 - 確保有顏色可以顯示
  if (_lastR == 0 && _lastG == 0 && _lastB == 0) {
    // 如果沒有保存的顏色，預設為白色
    _lastR = _lastG = _lastB = 255;
    Serial.println("無保存顏色，設置為白色");
  }
  
  // 直接寫入PWM通道，不經過setRGBColor
  int r = _lastR;
  int g = _lastG;
  int b = _lastB;
  
  // 如果是共陽極LED，需要反轉數值
  if (_isCommonAnode) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  
  Serial.printf("直接寫入PWM通道: R=%d, G=%d, B=%d\n", r, g, b);
  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
  
  Serial.println("LED已開啟，亮度: " + String(_savedBrightness) + 
                ", 顏色: R=" + String(_lastR) + 
                ", G=" + String(_lastG) + 
                ", B=" + String(_lastB));
}

// 設置LED類型（共陽極或共陰極）
void LedController::setCommonAnode(bool isCommonAnode) {
  _isCommonAnode = isCommonAnode;
  Serial.printf("LED類型設置為: %s\n", isCommonAnode ? "共陽極" : "共陰極");
}

// 設置引腳配置
void LedController::setPinConfig(int neopixelPin, int redPin, int greenPin, int bluePin) {
  // 先分離現有的通道
  ledcDetachPin(_redPin);
  ledcDetachPin(_greenPin);
  ledcDetachPin(_bluePin);
  
  // 更新引腳配置
  _neopixelPin = neopixelPin;
  _redPin = redPin;
  _greenPin = greenPin;
  _bluePin = bluePin;
  
  // 重新初始化NeoPixel
  _pixels = Adafruit_NeoPixel(_numPixels, neopixelPin, NEO_GRB + NEO_KHZ800);
  _pixels.begin();
  _pixels.setBrightness(_savedBrightness);
  _pixels.show();
  
  // 重新附加通道到新引腳
  ledcAttachPin(_redPin, RED_CHANNEL);
  ledcAttachPin(_greenPin, GREEN_CHANNEL);
  ledcAttachPin(_bluePin, BLUE_CHANNEL);
  
  Serial.printf("引腳配置已更新: NeoPixel=%d, R=%d, G=%d, B=%d\n", 
                neopixelPin, redPin, greenPin, bluePin);
}

// 測試LED引腳
void LedController::testPins() {
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
