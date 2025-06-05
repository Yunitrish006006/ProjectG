#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Define the LED pin
// For most boards, you can use LED_BUILTIN which is the built-in LED
// For ESP32-S3, the built-in LED is usually on pin 13
const int LED_PIN = LED_BUILTIN;

// 定義NeoPixel引腳和配置
#define NEOPIXEL_PIN 33  // Adafruit Feather ESP32-S3上的NeoPixel引腳
#define NUM_PIXELS 1     // 只有一個RGB LED

// 如果您的板子上沒有NeoPixel，也可以使用PWM控制外接的RGB LED
#define RED_PIN 12   // 紅色LED引腳
#define GREEN_PIN 13 // 綠色LED引腳
#define BLUE_PIN 14  // 藍色LED引腳

// 創建NeoPixel對象
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// 定義一些基本顏色
uint32_t RED = pixels.Color(255, 0, 0);
uint32_t GREEN = pixels.Color(0, 255, 0);
uint32_t BLUE = pixels.Color(0, 0, 255);
uint32_t YELLOW = pixels.Color(255, 255, 0);
uint32_t PURPLE = pixels.Color(255, 0, 255);
uint32_t CYAN = pixels.Color(0, 255, 255);
uint32_t WHITE = pixels.Color(255, 255, 255);

// 定義顏色數組
uint32_t colors[] = {RED, GREEN, BLUE, YELLOW, PURPLE, CYAN, WHITE};
int colorIndex = 0;

// PWM配置
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8; // 8位解析度
const int RED_CHANNEL = 0;
const int GREEN_CHANNEL = 1;
const int BLUE_CHANNEL = 2;

// 函數聲明
int myFunction(int, int);
void setRGBColor(int r, int g, int b);

void setup() {
  // 初始化串口通信
  Serial.begin(115200);
  Serial.println("彩色LED測試程序啟動中");
  
  // Set the LED pin as output
  pinMode(LED_PIN, OUTPUT);

  // 初始化NeoPixel
  pixels.begin();
  pixels.setBrightness(50); // 設置亮度 (0-255)
  pixels.show(); // 初始化所有像素為'關閉'
  
  // 配置PWM通道（如果使用獨立RGB LED）
  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BLUE_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  
  // 將通道附加到GPIO引腳
  ledcAttachPin(RED_PIN, RED_CHANNEL);
  ledcAttachPin(GREEN_PIN, GREEN_CHANNEL);
  ledcAttachPin(BLUE_PIN, BLUE_CHANNEL);
  
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // Turn the LED on
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(1000);  // Wait for 1 second
  
  // Turn the LED off
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(1000);  // Wait for 1 second

  // 使用NeoPixel顯示不同顏色
  Serial.print("顯示顏色: ");
  
  // 顯示當前顏色
  pixels.setPixelColor(0, colors[colorIndex]);
  pixels.show();
  
  // 如果使用獨立RGB LED，也設置其顏色
  switch(colorIndex) {
    case 0: // 紅
      setRGBColor(255, 0, 0);
      Serial.println("紅色");
      break;
    case 1: // 綠
      setRGBColor(0, 255, 0);
      Serial.println("綠色");
      break;
    case 2: // 藍
      setRGBColor(0, 0, 255);
      Serial.println("藍色");
      break;
    case 3: // 黃
      setRGBColor(255, 255, 0);
      Serial.println("黃色");
      break;
    case 4: // 紫
      setRGBColor(255, 0, 255);
      Serial.println("紫色");
      break;
    case 5: // 青
      setRGBColor(0, 255, 255);
      Serial.println("青色");
      break;
    case 6: // 白
      setRGBColor(255, 255, 255);
      Serial.println("白色");
      break;
  }
  
  // 等待一秒
  delay(1000);
  
  // 更新顏色索引
  colorIndex = (colorIndex + 1) % 7;

  // put your main code here, to run repeatedly:
}

// 設置RGB LED顏色的函數（如果使用獨立RGB LED）
void setRGBColor(int r, int g, int b) {
  ledcWrite(RED_CHANNEL, r);
  ledcWrite(GREEN_CHANNEL, g);
  ledcWrite(BLUE_CHANNEL, b);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}