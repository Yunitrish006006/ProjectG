#include <Arduino.h>
#include "LedController.h"
#include "OledDisplay.h"
#include "AudioPlayer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <time.h>

// NTP 設置
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; // 東八區 (UTC+8)
const int daylightOffset_sec = 0; // 不使用夏令時

// WiFi 設置 - 固定使用以下SSID和密碼
// 要更改WiFi連接設置，請直接修改以下兩行
const char *ssid = "YunRog";         // 請修改為您的WiFi名稱
const char *password = "0937565253"; // 請修改為您的WiFi密碼
bool wifiConnected = false;

// 按鈕和LED引腳
const int CONFIG_BUTTON_PIN = 0; // Boot按鈕，通常用作系統重置
const int LED_PIN = LED_BUILTIN;
// ESP32-S3 N16R8 常用 I2C 腳位
const int I2C_SDA = 8; // GPIO8 是 ESP32-S3 N16R8 的默認 SDA
const int I2C_SCL = 9; // GPIO9 是 ESP32-S3 N16R8 的默認 SCL

// I2S 音頻引腳定義
#define I2S_DOUT 15
#define I2S_BCLK 16
#define I2S_LRC 17

// 任務處理柄
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t oledTaskHandle = NULL;
TaskHandle_t breathingTaskHandle = NULL;
TaskHandle_t audioTaskHandle = NULL;

// 隊列與同步對象
QueueHandle_t ledCommandQueue = NULL;
SemaphoreHandle_t ledMutex = NULL;

// 全局對象
LedController ledController;
OledDisplay oled(I2C_SDA, I2C_SCL);                   // 使用 ESP32-S3 N16R8 默認 I2C 引腳
AudioPlayer audioPlayer(I2S_BCLK, I2S_LRC, I2S_DOUT); // 音頻播放器
uint8_t r = 255, g = 0, b = 0;                        // 初始顏色為紅色

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

// 函數聲明
void oledUpdateTask(void *pvParameters);
void ledControlTask(void *pvParameters);
void breathingEffectTask(void *pvParameters);
void audioControlTask(void *pvParameters);
void runLedTestSequence();
void connectToWiFi();
void configureNTP();
String getCurrentDateTime();
void syncTimeWithNTP();
void wifiMonitorTask(void *pvParameters);

// 配置NTP服務
void configureNTP()
{
    if (wifiConnected)
    {
        // 配置NTP時間
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("無法獲取NTP時間");
            return;
        }

        Serial.println("NTP時間已同步");
        Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    }
}

// 獲取當前日期時間字串
String getCurrentDateTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return "Time not available";
    }

    char dateTimeStr[20];
    strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M", &timeinfo);
    return String(dateTimeStr);
}

// OLED 顯示任務
void oledUpdateTask(void *pvParameters)
{
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS; // 每1秒更新一次

    while (true)
    {
        // 清除屏幕
        oled.clear();
        oled.showTitle("System Status");

        // 顯示上電時間
        String uptimeString = "Uptime: " + String(millis() / 1000) + "s";
        oled.showText(uptimeString, 0, 18);

        // 顯示可用內存
        String memString = "RAM: " + String(ESP.getFreeHeap() / 1024) + "kB";
        oled.showText(memString, 0, 30); // 顯示WiFi狀態與圖標
        if (wifiConnected)
        {
            // 繪製WiFi圖標
            oled.drawIcon(0, 42, ICON_WIFI);
            oled.showText("WiFi: Connected", 18, 42);

            // 顯示音頻狀態
            oled.showText("Audio: Playing", 0, 54);
        }
        else
        {
            // 繪製無連接WiFi圖標
            oled.drawIcon(0, 42, ICON_WIFI_OFF);
            oled.showText("WiFi: Disconnected", 18, 42);
            oled.showText("Audio: Stopped", 0, 54);
        }

        oled.display();
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

// WiFi 連接函數
void connectToWiFi()
{
    Serial.println();
    Serial.print("連接到WiFi網絡: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // 等待連接 (最多20秒)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS); // 改用 vTaskDelay 替代 delay
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");
        Serial.println("WiFi已連接");
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;

        // 配置NTP
        configureNTP();
    }
    else
    {
        Serial.println("");
        Serial.println("WiFi連接失敗");
        wifiConnected = false;
    }
}

// NTP時間同步
void syncTimeWithNTP()
{
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 1000))
    {
        Serial.println("獲取時間失敗");
        return;
    }
    Serial.print("當前時間: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
}

// WiFi監控任務
void wifiMonitorTask(void *pvParameters)
{
    const TickType_t xDelay = 30000 / portTICK_PERIOD_MS; // 每30秒檢查一次
    bool ntpConfigured = false;

    while (true)
    {
        // 檢查WiFi連接狀態
        if (WiFi.status() != WL_CONNECTED)
        {
            wifiConnected = false;
            ntpConfigured = false;
            Serial.println("WiFi連接中斷，嘗試重連...");
            connectToWiFi();
        }
        else if (!ntpConfigured && wifiConnected)
        {
            // 如果WiFi已連接但NTP尚未配置，則配置NTP
            configureNTP();
            ntpConfigured = true;
        }
        vTaskDelay(xDelay);
    }
}

// 音頻控制任務
void audioControlTask(void *pvParameters)
{
    // 等待WiFi連接
    while (!wifiConnected)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // 初始化音頻播放器
    audioPlayer.begin();
    audioPlayer.setVolume(21); // 0...21    // 開始播放默認音頻流
    Serial.println("Starting audio playback...");
    audioPlayer.playURL("http://tangosl.4hotel.tw:8005/play.mp3");
    const TickType_t xDelay = 1 / portTICK_PERIOD_MS; // 1ms 延遲
    static uint32_t serialCheckCounter = 0;

    while (true)
    {
        // 處理音頻循環 - 這必須經常被調用以避免音頻中斷
        audioPlayer.loop();

        // 每1000次循環才檢查一次串列埠輸入，避免影響音頻性能
        serialCheckCounter++;
        if (serialCheckCounter >= 1000)
        {
            serialCheckCounter = 0;
            if (Serial.available())
            {
                String r = Serial.readString();
                r.trim();
                if (r.length() > 5)
                {
                    Serial.println("Changing audio source to: " + r);
                    audioPlayer.stopAndPlay(r.c_str());
                }
            }
        }

        // 使用極短延遲或不延遲，讓音頻處理盡可能連續
        vTaskDelay(xDelay);
    }
}

void setup()
{
    Serial.begin(115200);
    // 使用 vTaskDelay 代替 delay
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    randomSeed(analogRead(A0));
    pinMode(LED_PIN, OUTPUT);

    // 初始化 OLED 顯示
    oled.begin();

    // 顯示啟動信息
    oled.clear();
    oled.showTitle("ProjectG");
    oled.showText("System starting...", 0, 30);
    oled.display();

    // 初始化WiFi
    WiFi.mode(WIFI_STA);
    connectToWiFi();

    // 同步時間
    if (wifiConnected)
    {
        syncTimeWithNTP();
    }

    // 創建隊列和互斥鎖
    ledCommandQueue = xQueueCreate(10, sizeof(LedCommandData));
    ledMutex = xSemaphoreCreateMutex();

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

    // 創建WiFi監控任務
    xTaskCreate(
        wifiMonitorTask, // 任務函數
        "WiFi Monitor",  // 任務名稱
        4096,            // 堆棧大小
        NULL,            // 參數
        1,               // 優先級 (1 是低優先級)
        NULL             // 不需要任務句柄
    );                   // 創建音頻控制任務 - 使用高優先級和專用核心
    xTaskCreatePinnedToCore(
        audioControlTask, // 任務函數
        "Audio Control",  // 任務名稱
        8192,             // 增加堆棧大小以處理音頻緩衝
        NULL,             // 參數
        3,                // 高優先級 (3 是高優先級)
        &audioTaskHandle, // 任務控制句柄
        PRO_CPU_NUM       // 在主核心上運行以獲得更好的性能
    );
}

void loop()
{
    // 在使用 FreeRTOS 時，主循環可以保持空白或執行低優先級任務
    // 所有主要功能都已在任務中完成

    // 每秒檢查一次系統健康狀況
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // 系統監控
    if (uxTaskGetStackHighWaterMark(NULL) < 128)
    {
        Serial.println("Warning: Main loop stack running low");
    }

    // 監控音頻任務狀態
    if (audioTaskHandle != NULL)
    {
        if (eTaskGetState(audioTaskHandle) == eDeleted)
        {
            Serial.println("Warning: Audio task has been deleted");
        }
    }

    // 定期輸出系統狀態（每30秒）
    static uint32_t lastStatusTime = 0;
    if (millis() - lastStatusTime > 30000)
    {
        lastStatusTime = millis();
        Serial.printf("Free heap: %d bytes, Audio task running: %s\n",
                      ESP.getFreeHeap(),
                      (audioTaskHandle != NULL) ? "Yes" : "No");
    }
}
