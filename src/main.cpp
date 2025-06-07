#include <Arduino.h>
#include "LedController.h"

// 使用定義的內置LED
const int LED_PIN = LED_BUILTIN;
LedController ledController;
bool ledState = false;

void setup()
{
  Serial.begin(115200);
  delay(1000); // 等待串口初始化
  Serial.println("\n\n==============================");
  Serial.println("LED開關測試程序 - 專注於開關功能");
  Serial.println("==============================");

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
  if (ledState)
  {
    ledController.setColor(255, 0, 0);
    ledController.on();
  }
  else
  {
    ledController.off();
  }

  ledState = !ledState;
  delay(3000);
}