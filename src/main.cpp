#include <Arduino.h>
#include "LedController.h"

// 使用定義的內置LED
const int LED_PIN = LED_BUILTIN;
LedController ledController;
bool ledState = false;

// 產生鮮豔隨機顏色的函數
void getVividColor(uint8_t &r, uint8_t &g, uint8_t &b) {
  // 顏色模式選擇
  int colorMode = random(6);
  
  switch (colorMode) {
    case 0: // 紅色主導
      r = random(180, 256);
      g = random(50);
      b = random(50);
      break;
    case 1: // 綠色主導
      r = random(50);
      g = random(180, 256);
      b = random(50);
      break;
    case 2: // 藍色主導
      r = random(50);
      g = random(50);
      b = random(180, 256);
      break;
    case 3: // 黃色/橙色
      r = random(180, 256);
      g = random(100, 200);
      b = random(30);
      break;
    case 4: // 紫色/粉紅色
      r = random(150, 256);
      g = random(20, 80);
      b = random(150, 256);
      break;
    case 5: // 青色/淺藍色
      r = random(20, 80);
      g = random(150, 256);
      b = random(150, 256);
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // 等待串口初始化
  Serial.println("\n\n==============================");
  Serial.println("LED隨機顏色測試程序");
  Serial.println("==============================");

  // 初始化隨機數生成器（使用未連接的模擬引腳讀取噪聲作為種子）
  randomSeed(analogRead(A0));
  
  // 設置內建LED為輸出
  pinMode(LED_PIN, OUTPUT);

  // 確認LED引腳設置
  Serial.println("使用引腳設置: NeoPixel=33, R=12, G=13, B=14");

  // 設置引腳配置 - 使用確認的值
  ledController.setPinConfig(33, 12, 13, 14);

  // 設置共陰極/共陽極模式
  // 嘗試註釋/取消註釋下面這行來切換模式，看看哪種能正常工作
  // ledController.setCommonAnode(true);

  Serial.println("初始化LED控制器...");
  ledController.begin();

  Serial.println("LED控制器已初始化");
  Serial.println("現在將交替測試LED的開啟和關閉");

  // 閃爍內建LED表示初始化完成
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  delay(1000);
}

void loop()
{
  static int effectMode = 0;
  
  // 每3次循環更換效果模式
  if (ledState) {
    effectMode = (effectMode + 1) % 3;
    Serial.printf("切換到效果模式 %d\n", effectMode);
  }
  
  if (ledState)
  {
    // 產生鮮豔的隨機顏色
    uint8_t r, g, b;
    
    // 隨機選擇是使用普通隨機顏色還是鮮豔顏色
    if (random(10) < 7) { // 70%的機率使用鮮豔顏色
      getVividColor(r, g, b);
      Serial.printf("設定鮮豔隨機顏色 RGB: (%d, %d, %d)\n", r, g, b);
    } else { // 30%的機率使用完全隨機顏色
      r = random(256);
      g = random(256);
      b = random(256);
      Serial.printf("設定完全隨機顏色 RGB: (%d, %d, %d)\n", r, g, b);
    }
    
    // 開啟LED
    ledController.on();
    
    switch (effectMode) {
      case 0: // 基本顯示模式
        ledController.setColor(r, g, b);
        delay(3000);
        break;
      
      case 1: // 閃爍模式
        for (int i = 0; i < 5; i++) {
          ledController.setColor(r, g, b);
          delay(300);
          ledController.setColor(0, 0, 0); // 短暫關閉
          delay(200);
        }
        break;
      
      case 2: // 呼吸模式
        for (int brightness = 0; brightness <= 100; brightness += 5) {
          // 逐漸增加亮度
          int adjR = r * brightness / 100;
          int adjG = g * brightness / 100;
          int adjB = b * brightness / 100;
          ledController.setColor(adjR, adjG, adjB);
          delay(50);
        }
        delay(500);
        for (int brightness = 100; brightness >= 0; brightness -= 5) {
          // 逐漸減少亮度
          int adjR = r * brightness / 100;
          int adjG = g * brightness / 100;
          int adjB = b * brightness / 100;
          ledController.setColor(adjR, adjG, adjB);
          delay(50);
        }
        break;
    }
  }
  else
  {
    ledController.off();
    Serial.println("LED已關閉");
    delay(2000); // 關閉狀態保持2秒
  }

  ledState = !ledState;
}