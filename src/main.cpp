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
#include <driver/i2s.h>

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
// 音量控制按鈕
const int VOLUME_UP_PIN = 1;   // GPIO1 音量增加按鈕
const int VOLUME_DOWN_PIN = 2; // GPIO2 音量減少按鈕
// 麥克風控制按鈕
const int MIC_CONTROL_PIN = 42; // GPIO3 麥克風開關按鈕
// ESP32-S3 N16R8 常用 I2C 腳位
const int I2C_SDA = 8; // GPIO8 是 ESP32-S3 N16R8 的默認 SDA
const int I2C_SCL = 9; // GPIO9 是 ESP32-S3 N16R8 的默認 SCL

// I2S 音頻引腳定義 (用於音頻播放)
#define I2S_DOUT 15
#define I2S_BCLK 16
#define I2S_LRC 17

// I2S 麥克風引腳定義 (使用 I2S_NUM_1)
#define MIC_I2S_WS 18  // 字選擇信號
#define MIC_I2S_SD 21  // 音頻數據輸入 (改用 GPIO21)
#define MIC_I2S_SCK 20 // 時鐘信號
#define MIC_I2S_PORT I2S_NUM_1
#define MIC_SAMPLE_BITS 16
#define MIC_SAMPLE_RATE 16000

// 任務處理柄
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t oledTaskHandle = NULL;
TaskHandle_t breathingTaskHandle = NULL;
TaskHandle_t audioTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t micTaskHandle = NULL;

// 隊列與同步對象
QueueHandle_t ledCommandQueue = NULL;
QueueHandle_t volumeCommandQueue = NULL;
SemaphoreHandle_t ledMutex = NULL;

// 全局對象
LedController ledController;
OledDisplay oled(I2C_SDA, I2C_SCL);                   // 使用 ESP32-S3 N16R8 默認 I2C 引腳
AudioPlayer audioPlayer(I2S_BCLK, I2S_LRC, I2S_DOUT); // 音頻播放器
uint8_t r = 255, g = 0, b = 0;                        // 初始顏色為紅色

// 音量控制變數
int currentVolume = 12; // 初始音量 (0-21)

// 麥克風相關變數
const int micBufLen = 256;
const int micBufLenBytes = micBufLen * (MIC_SAMPLE_BITS / 8);
int16_t micAudioBuf[256];
bool micEnabled = false;

// 命令定義
enum LedCommand
{
    CMD_SET_COLOR,
    CMD_BREATHE,
    CMD_OFF,
    CMD_ON
};

// 音量控制命令
enum VolumeCommand
{
    VOL_UP,
    VOL_DOWN,
    VOL_SET
};

// 麥克風控制命令
enum MicCommand
{
    MIC_START,
    MIC_STOP,
    MIC_TOGGLE
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

// 音量命令結構
struct VolumeCommandData
{
    VolumeCommand cmd;
    int volume;
};

// 麥克風命令結構
struct MicCommandData
{
    MicCommand cmd;
};

// 函數聲明
void oledUpdateTask(void *pvParameters);
void ledControlTask(void *pvParameters);
void breathingEffectTask(void *pvParameters);
void audioControlTask(void *pvParameters);
void buttonControlTask(void *pvParameters);
void microphoneTask(void *pvParameters);
void runLedTestSequence();
void connectToWiFi();
void configureNTP();
String getCurrentDateTime();
void syncTimeWithNTP();
void wifiMonitorTask(void *pvParameters);
esp_err_t setupMicrophone();
void startMicrophone();
void stopMicrophone();

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

            // 顯示音頻和音量狀態
            String audioStatus = "Audio: Playing Vol:" + String(currentVolume);
            oled.showText(audioStatus, 0, 54);

            // 顯示麥克風狀態
            String micStatus = "Mic: " + String(micEnabled ? "ON" : "OFF");
            oled.showText(micStatus, 90, 54);
        }
        else
        {
            // 繪製無連接WiFi圖標
            oled.drawIcon(0, 42, ICON_WIFI_OFF);
            oled.showText("WiFi: Disconnected", 18, 42);
            String audioStatus = "Audio: Stopped Vol:" + String(currentVolume);
            oled.showText(audioStatus, 0, 54);

            // 顯示麥克風狀態
            String micStatus = "Mic: " + String(micEnabled ? "ON" : "OFF");
            oled.showText(micStatus, 90, 54);
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

// 按鈕控制任務
void buttonControlTask(void *pvParameters)
{
    // 按鈕狀態變數
    bool volumeUpLastState = false;
    bool volumeDownLastState = false;
    bool micControlLastState = false;

    // 防抖動計時器
    unsigned long lastVolumeUpTime = 0;
    unsigned long lastVolumeDownTime = 0;
    unsigned long lastMicControlTime = 0;
    const unsigned long debounceDelay = 200; // 200ms防抖動延遲

    VolumeCommandData volumeCmd;
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS; // 50ms檢查間隔

    while (true)
    {
        // 讀取按鈕狀態 (按下為LOW，因為使用內部上拉電阻)
        bool volumeUpCurrentState = !digitalRead(VOLUME_UP_PIN);
        bool volumeDownCurrentState = !digitalRead(VOLUME_DOWN_PIN);
        bool micControlCurrentState = !digitalRead(MIC_CONTROL_PIN);

        unsigned long currentTime = millis();

        // 處理音量增加按鈕
        if (volumeUpCurrentState && !volumeUpLastState &&
            (currentTime - lastVolumeUpTime) > debounceDelay)
        {
            // 按鈕剛被按下
            volumeCmd.cmd = VOL_UP;
            xQueueSend(volumeCommandQueue, &volumeCmd, 0);
            lastVolumeUpTime = currentTime;
            Serial.println("Volume UP button pressed");
        } // 處理音量減少按鈕
        if (volumeDownCurrentState && !volumeDownLastState &&
            (currentTime - lastVolumeDownTime) > debounceDelay)
        {
            // 按鈕剛被按下
            volumeCmd.cmd = VOL_DOWN;
            xQueueSend(volumeCommandQueue, &volumeCmd, 0);
            lastVolumeDownTime = currentTime;
            Serial.println("Volume DOWN button pressed");
        }

        // 處理麥克風控制按鈕
        if (micControlCurrentState && !micControlLastState &&
            (currentTime - lastMicControlTime) > debounceDelay)
        {
            // 切換麥克風狀態
            if (micEnabled)
            {
                stopMicrophone();
            }
            else
            {
                startMicrophone();
            }
            lastMicControlTime = currentTime;
            Serial.println("Microphone control button pressed");
        }

        // 更新按鈕狀態
        volumeUpLastState = volumeUpCurrentState;
        volumeDownLastState = volumeDownCurrentState;
        micControlLastState = micControlCurrentState;

        vTaskDelay(xDelay);
    }
}

// 麥克風設置函數
esp_err_t setupMicrophone()
{
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = MIC_SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(MIC_SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false};

    const i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_I2S_SCK,
        .ws_io_num = MIC_I2S_WS,
        .data_out_num = -1,
        .data_in_num = MIC_I2S_SD};

    esp_err_t result = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (result != ESP_OK)
    {
        Serial.printf("麥克風 I2S 驅動安裝失敗: %d\n", result);
        return result;
    }

    result = i2s_set_pin(MIC_I2S_PORT, &pin_config);
    if (result != ESP_OK)
    {
        Serial.printf("麥克風 I2S 引腳設定失敗: %d\n", result);
        return result;
    }

    Serial.println("✓ 麥克風 I2S 設定成功");
    return ESP_OK;
}

void startMicrophone()
{
    if (!micEnabled)
    {
        esp_err_t result = i2s_start(MIC_I2S_PORT);
        if (result == ESP_OK)
        {
            micEnabled = true;
            Serial.println("✓ 麥克風已啟動");
        }
        else
        {
            Serial.printf("麥克風啟動失敗: %d\n", result);
        }
    }
}

void stopMicrophone()
{
    if (micEnabled)
    {
        i2s_stop(MIC_I2S_PORT);
        micEnabled = false;
        Serial.println("✓ 麥克風已停止");
    }
}

// 麥克風任務
void microphoneTask(void *pvParameters)
{
    Serial.println("麥克風任務啟動");

    // 等待系統初始化完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 設置麥克風
    if (setupMicrophone() != ESP_OK)
    {
        Serial.println("✗ 麥克風設置失敗，任務結束");
        vTaskDelete(NULL);
        return;
    }

    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    size_t bytesRead = 0;
    static int audioLevelCounter = 0;

    while (true)
    {
        if (micEnabled)
        {
            // 從麥克風讀取音頻數據
            esp_err_t result = i2s_read(MIC_I2S_PORT, micAudioBuf, micBufLenBytes, &bytesRead, 100);

            if (result == ESP_OK && bytesRead > 0)
            {
                // 將音頻數據寫入串口
                Serial.write((uint8_t *)micAudioBuf, bytesRead);

                // 每500次循環顯示一次音頻電平（避免影響OLED顯示）
                audioLevelCounter++;
                if (audioLevelCounter >= 500)
                {
                    audioLevelCounter = 0;

                    // 計算音頻電平
                    long sum = 0;
                    for (int i = 0; i < micBufLen; i++)
                    {
                        sum += abs(micAudioBuf[i]);
                    }
                    int avgLevel = sum / micBufLen;

                    // 通過串口輸出電平信息（可選）
                    // Serial.printf("Mic Level: %d\n", avgLevel);
                }
            }
        }
        else
        {
            // 麥克風未啟用時等待
            vTaskDelay(100 / portTICK_PERIOD_MS);
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
    audioPlayer.setVolume(currentVolume); // 使用全局音量變數

    const TickType_t xDelay = 1 / portTICK_PERIOD_MS; // 1ms 延遲
    static uint32_t serialCheckCounter = 0;
    VolumeCommandData volumeCmd;

    // 開始播放默認音頻流
    Serial.println("Starting audio playback...");
    audioPlayer.playURL("http://tangosl.4hotel.tw:8005/play.mp3");
    while (true)
    {
        // 處理音頻循環 - 這必須經常被調用以避免音頻中斷
        audioPlayer.loop();

        // 檢查音量控制命令
        if (xQueueReceive(volumeCommandQueue, &volumeCmd, 0) == pdTRUE)
        {
            switch (volumeCmd.cmd)
            {
            case VOL_UP:
                if (currentVolume < 21)
                {
                    currentVolume++;
                    audioPlayer.setVolume(currentVolume);
                    Serial.printf("Volume UP: %d\n", currentVolume);
                }
                break;
            case VOL_DOWN:
                if (currentVolume > 0)
                {
                    currentVolume--;
                    audioPlayer.setVolume(currentVolume);
                    Serial.printf("Volume DOWN: %d\n", currentVolume);
                }
                break;
            case VOL_SET:
                if (volumeCmd.volume >= 0 && volumeCmd.volume <= 21)
                {
                    currentVolume = volumeCmd.volume;
                    audioPlayer.setVolume(currentVolume);
                    Serial.printf("Volume SET: %d\n", currentVolume);
                }
                break;
            }
        }

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
    pinMode(LED_PIN, OUTPUT); // 初始化音量控制按鈕
    pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
    pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
    pinMode(MIC_CONTROL_PIN, INPUT_PULLUP);
    Serial.println("Volume control and microphone buttons initialized");

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
    } // 創建隊列和互斥鎖
    ledCommandQueue = xQueueCreate(10, sizeof(LedCommandData));
    volumeCommandQueue = xQueueCreate(10, sizeof(VolumeCommandData));
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
    );                        // 創建WiFi監控任務
    xTaskCreate(
        wifiMonitorTask, // 任務函數
        "WiFi Monitor",  // 任務名稱
        4096,            // 堆棧大小
        NULL,            // 參數
        1,               // 優先級 (1 是低優先級)
        NULL             // 不需要任務句柄
    );

    // 創建按鈕控制任務
    xTaskCreatePinnedToCore(
        buttonControlTask, // 任務函數
        "Button Control",  // 任務名稱
        4096,              // 堆棧大小
        NULL,              // 參數
        2,                 // 中等優先級 (2)
        &buttonTaskHandle, // 任務控制句柄
        APP_CPU_NUM        // 在第二個核心上運行
    );

    // 創建麥克風任務
    xTaskCreatePinnedToCore(
        microphoneTask, // 任務函數
        "Microphone",   // 任務名稱
        6144,           // 堆棧大小
        NULL,           // 參數
        2,              // 中等優先級 (2)
        &micTaskHandle, // 任務控制句柄
        APP_CPU_NUM     // 在第二個核心上運行
    );

    // 創建音頻控制任務 - 使用高優先級和專用核心
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
