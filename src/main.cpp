#include <Arduino.h>
#include "LedController.h"
#include "OledDisplay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

const int LED_PIN = LED_BUILTIN;
// ESP32-S3 N16R8 常用 I2C 腳位
const int I2C_SDA = 8; // GPIO8 是 ESP32-S3 N16R8 的默認 SDA
const int I2C_SCL = 9; // GPIO9 是 ESP32-S3 N16R8 的默認 SCL

// 任務處理柄
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t oledTaskHandle = NULL;
TaskHandle_t breathingTaskHandle = NULL;

// 隊列與同步對象
QueueHandle_t ledCommandQueue = NULL;
SemaphoreHandle_t ledMutex = NULL;

// 全局對象
LedController ledController;
OledDisplay oled(I2C_SDA, I2C_SCL); // 使用 ESP32-S3 N16R8 默認 I2C 引腳
uint8_t r = 255, g = 0, b = 0;      // 初始顏色為紅色

// 命令定義
enum LedCommand
{
  CMD_SET_COLOR,
  CMD_BREATHE,
  CMD_OFF,
  CMD_ON
};

// 命令結構
struct LedCommandData
{
  LedCommand cmd;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  int duration;
  bool colorShifting;
};

// OLED 顯示任務
void oledUpdateTask(void *pvParameters)
{
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS; // 每1秒更新一次

  while (true)
  {
    oled.showSimpleSystemInfo();
    vTaskDelay(xDelay);
  }
}

// LED控制任務
void ledControlTask(void *pvParameters)
{
  LedCommandData cmd;

  while (true)
  {
    if (xQueueReceive(ledCommandQueue, &cmd, portMAX_DELAY) == pdTRUE)
    {
      xSemaphoreTake(ledMutex, portMAX_DELAY);

      switch (cmd.cmd)
      {
      case CMD_SET_COLOR:
        ledController.setColor(cmd.r, cmd.g, cmd.b);
        r = cmd.r;
        g = cmd.g;
        b = cmd.b;
        break;

      case CMD_BREATHE:
        ledController.breathe(cmd.r, cmd.g, cmd.b, 100, cmd.duration, cmd.colorShifting);
        break;

      case CMD_OFF:
        ledController.off();
        break;

      case CMD_ON:
        ledController.on();
        break;
      }

      xSemaphoreGive(ledMutex);
    }
  }
}

// 呼吸燈效果任務
void breathingEffectTask(void *pvParameters)
{
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  LedCommandData cmd;

  while (true)
  {
    // 每次循環生成新的顏色
    uint8_t newR, newG, newB;
    ledController.getVividColor(newR, newG, newB, random(0, 6));

    // 發送呼吸燈命令
    cmd.cmd = CMD_BREATHE;
    cmd.r = newR;
    cmd.g = newG;
    cmd.b = newB;
    cmd.duration = 4000;
    cmd.colorShifting = true;
    xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);

    // 等待一段時間
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    // 設置低亮度
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    ledController.setBrightness(20);
    xSemaphoreGive(ledMutex);

    // 等待一段時間
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 隨機決定是否使用固定顏色
    if (random(10) > 7)
    {
      uint8_t primaryColor = random(0, 3);
      uint8_t fixedR = (primaryColor == 0) ? 255 : 30;
      uint8_t fixedG = (primaryColor == 1) ? 255 : 30;
      uint8_t fixedB = (primaryColor == 2) ? 255 : 30;

      // 發送固定顏色呼吸燈命令
      cmd.cmd = CMD_BREATHE;
      cmd.r = fixedR;
      cmd.g = fixedG;
      cmd.b = fixedB;
      cmd.duration = 3000;
      cmd.colorShifting = false;
      xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);

      // 等待完成
      vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
  }
}

// 啟動測試序列
void runLedTestSequence()
{
  LedCommandData cmd;

  // RED
  cmd.cmd = CMD_SET_COLOR;
  cmd.r = 255;
  cmd.g = 0;
  cmd.b = 0;
  xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // GREEN
  cmd.cmd = CMD_SET_COLOR;
  cmd.r = 0;
  cmd.g = 255;
  cmd.b = 0;
  xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // BLUE
  cmd.cmd = CMD_SET_COLOR;
  cmd.r = 0;
  cmd.g = 0;
  cmd.b = 255;
  xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // WHITE
  cmd.cmd = CMD_SET_COLOR;
  cmd.r = 255;
  cmd.g = 255;
  cmd.b = 255;
  xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // 漸變效果
  for (int i = 0; i < 256; i += 16)
  {
    cmd.cmd = CMD_SET_COLOR;
    cmd.r = 255 - i;
    cmd.g = i;
    cmd.b = 128;
    xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }

  // 關閉LED
  cmd.cmd = CMD_OFF;
  xQueueSend(ledCommandQueue, &cmd, portMAX_DELAY);
}

void setup()
{
  Serial.begin(115200);
  // 使用 vTaskDelay 代替 delay
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  randomSeed(analogRead(A0));
  pinMode(LED_PIN, OUTPUT);

  // 創建隊列和互斥鎖
  ledCommandQueue = xQueueCreate(10, sizeof(LedCommandData));
  ledMutex = xSemaphoreCreateMutex();

  // 初始化 OLED 顯示
  oled.begin();

  // 僅顯示系統信息
  oled.showSimpleSystemInfo();

  // 初始化 LED 控制器
  // 為 ESP32-S3 N16R8 設置適合的 LED 引腳
  // NeoPixel: GPIO48, RGB LED: GPIO35 (紅), GPIO36 (綠), GPIO37 (藍)
  ledController.setPinConfig(48, 35, 36, 37);
  ledController.begin();

  // 創建任務
  xTaskCreatePinnedToCore(
      oledUpdateTask,  // 任務函數
      "OLED Update",   // 任務名稱
      4096,            // 堆棧大小
      NULL,            // 參數
      1,               // 優先級 (1 是低優先級)
      &oledTaskHandle, // 任務控制句柄
      APP_CPU_NUM      // 在第二個核心上運行
  );

  xTaskCreatePinnedToCore(
      ledControlTask, // 任務函數
      "LED Control",  // 任務名稱
      4096,           // 堆棧大小
      NULL,           // 參數
      2,              // 優先級 (2 是中優先級)
      &ledTaskHandle, // 任務控制句柄
      PRO_CPU_NUM     // 在主核心上運行
  );

  // 運行LED測試序列
  runLedTestSequence();

  // 創建呼吸燈任務 (在測試完成後啟動)
  xTaskCreatePinnedToCore(
      breathingEffectTask,  // 任務函數
      "Breathing Effect",   // 任務名稱
      4096,                 // 堆棧大小
      NULL,                 // 參數
      1,                    // 優先級 (1 是低優先級)
      &breathingTaskHandle, // 任務控制句柄
      PRO_CPU_NUM           // 在主核心上運行
  );
}

void loop()
{
  // 在使用 FreeRTOS 時，主循環可以保持空白或執行低優先級任務
  // 所有主要功能都已在任務中完成

  // 每秒檢查一次系統健康狀況
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  // 可以在這裡添加系統監控或低優先級任務
  // 例如，檢查記憶體用量
  if (uxTaskGetStackHighWaterMark(NULL) < 128)
  {
    // 如果堆棧水位標記過低，可能需要重置某些任務
    Serial.println("Warning: Main loop stack running low");
  }

  // 如果需要，可以在這裡添加看門狗喂狗代碼
}