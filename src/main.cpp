#include <Arduino.h>
#include "LedController.h"
#include "OledDisplay.h"
// #include "AudioPlayer.h"  // 電台播放功能已停用
#include "AudioFeatureExtractor.h"
#include "AudioMqttManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <time.h>
#include <driver/i2s.h>
#include "esp_task_wdt.h"

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
const int MIC_CONTROL_PIN = 21; // GPIO21 麥克風開關按鈕
// ESP32-S3 N16R8 常用 I2C 腳位
const int I2C_SDA = 8; // GPIO8 是 ESP32-S3 N16R8 的默認 SDA
const int I2C_SCL = 9; // GPIO9 是 ESP32-S3 N16R8 的默認 SCL

// OLED 顯示器設定
const int OLED_SDA = I2C_SDA;  // OLED SDA 引腳
const int OLED_SCL = I2C_SCL;  // OLED SCL 引腳
const int OLED_ADDRESS = 0x3C; // OLED I2C 地址

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
SemaphoreHandle_t i2sMutex = NULL;

// 全局對象
LedController ledController;
OledDisplay oled(I2C_SDA, I2C_SCL); // 使用 ESP32-S3 N16R8 默認 I2C 引腳
// AudioPlayer audioPlayer(I2S_BCLK, I2S_LRC, I2S_DOUT); // 音頻播放器 - 電台播放功能已停用
AudioMqttManager audioMqttManager; // MQTT 音訊管理器
uint8_t r = 255, g = 0, b = 0;     // 初始顏色為紅色

const char *mqttServer = "broker.emqx.io";

const int mqttPort = 1883;
const char *mqttUser = nullptr;     // 公共服務器通常不需要認證
const char *mqttPassword = nullptr; // 公共服務器通常不需要認證

// 音量控制變數
int currentVolume = 12; // 初始音量 (0-21)

// 麥克風相關變數
const int micBufLen = 256;
const int micBufLenBytes = micBufLen * (MIC_SAMPLE_BITS / 8);
int16_t micAudioBuf[256];
bool micEnabled = false;
bool musicPaused = false;      // 音樂是否被暫停
bool micToSpeakerMode = false; // 麥克風輸出到喇叭模式

// MQTT 音訊流和特徵提取控制
bool mqttAudioEnabled = false;
bool mqttFeatureExtractionEnabled = false;

// 按鈕控制錄音模式
bool buttonRecordingMode = true;      // 啟用按鈕錄音模式
bool diagnosticMode = false;          // 診斷模式：簡化按鈕處理避免重啟
bool testMutexOnly = false;           // 測試階段1：只測試互斥鎖獲取
bool testCleanupOnly = false;         // 測試階段2：測試 I2S 清理操作
bool testSetupOnly = false;           // 測試階段3：測試麥克風設置操作
bool testDirectSetup = false;         // 測試階段4：直接設置，完全跳過清理
bool isRecording = false;             // 當前是否正在錄音
bool pendingAudioProcessing = false;  // 是否有待處理的音訊
unsigned long recordingStartTime = 0; // 錄音開始時間
unsigned long recordingDuration = 0;  // 錄音持續時間

// OLED 硬件狀態
bool oledHardwareAvailable = false; // OLED 硬件是否可用

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
esp_err_t safeCleanupI2SResources(); // 安全的 I2S 資源清理
bool initializeOLED();               // OLED 硬件檢測與初始化
void updateOLEDDisplay();            // 安全的 OLED 更新函數
void startButtonRecording();
void stopButtonRecordingAndProcess();
void processRecordedAudio();
esp_err_t setupI2SOutput();
void pauseMusicAndStartMic();
void stopMicAndResumeMusic();

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

// OLED 顯示任務 - 安全版本
void oledUpdateTask(void *pvParameters)
{
    const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 每2秒更新一次，降低頻率

    while (true)
    {
        // 只有在 OLED 硬件可用時才嘗試更新
        if (oledHardwareAvailable)
        {
            updateOLEDDisplay();
        }
        else
        {
            // 如果 OLED 不可用，通過串口輸出狀態
            static uint32_t statusCounter = 0;
            statusCounter++;
            if (statusCounter % 15 == 0) // 每30秒輸出一次狀態（15次循環 * 2秒）
            {
                Serial.println("=== 系統狀態 (無OLED顯示) ===");
                Serial.printf("運行時間: %lu秒\n", millis() / 1000);
                Serial.printf("可用記憶體: %d kB\n", ESP.getFreeHeap() / 1024);
                Serial.printf("WiFi: %s\n", wifiConnected ? "已連接" : "未連接");

                if (buttonRecordingMode)
                {
                    if (isRecording)
                    {
                        Serial.println("按鈕錄音: 錄音中...");
                    }
                    else if (pendingAudioProcessing)
                    {
                        Serial.println("按鈕錄音: 處理中...");
                    }
                    else
                    {
                        Serial.println("按鈕錄音: 就緒");
                    }
                }
                Serial.println("==============================");
            }
        }

        vTaskDelay(xDelay);
    }
}

// LED控制任務
void ledControlTask(void *pvParameters)
{
    LedCommandData cmd;

    // 等待隊列初始化
    while (ledCommandQueue == NULL || ledMutex == NULL)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    Serial.println("LED 控制任務已啟動");

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
    if (ledCommandQueue == NULL)
    {
        Serial.println("ERROR: ledCommandQueue 未初始化，跳過 LED 測試");
        return;
    }

    Serial.println("開始 LED 測試序列...");
    LedCommandData cmd;

    // RED
    cmd.cmd = CMD_SET_COLOR;
    cmd.r = 255;
    cmd.g = 0;
    cmd.b = 0;
    if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Serial.println("LED 隊列發送失敗 (RED)");
        return;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // GREEN
    cmd.cmd = CMD_SET_COLOR;
    cmd.r = 0;
    cmd.g = 255;
    cmd.b = 0;
    if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Serial.println("LED 隊列發送失敗 (GREEN)");
        return;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); // BLUE
    cmd.cmd = CMD_SET_COLOR;
    cmd.r = 0;
    cmd.g = 0;
    cmd.b = 255;
    if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Serial.println("LED 隊列發送失敗 (BLUE)");
        return;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // WHITE
    cmd.cmd = CMD_SET_COLOR;
    cmd.r = 255;
    cmd.g = 255;
    cmd.b = 255;
    if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Serial.println("LED 隊列發送失敗 (WHITE)");
        return;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); // 漸變效果
    for (int i = 0; i < 256; i += 16)
    {
        cmd.cmd = CMD_SET_COLOR;
        cmd.r = 255 - i;
        cmd.g = i;
        cmd.b = 128;
        if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            Serial.println("LED 隊列發送失敗 (漸變效果)");
            break;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // 啟動呼吸效果
    cmd.cmd = CMD_BREATHE;
    cmd.r = 255;
    cmd.g = 100;
    cmd.b = 0;
    if (xQueueSend(ledCommandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        Serial.println("LED 隊列發送失敗 (BREATHE)");
        return;
    }

    Serial.println("LED 測試序列完成");
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

        unsigned long currentTime = millis(); // 處理音量增加按鈕
        if (volumeUpCurrentState && !volumeUpLastState &&
            (currentTime - lastVolumeUpTime) > debounceDelay)
        {
            // 按鈕剛被按下
            if (volumeCommandQueue != NULL)
            {
                volumeCmd.cmd = VOL_UP;
                xQueueSend(volumeCommandQueue, &volumeCmd, 0);
                Serial.println("Volume UP button pressed");
            }
            else
            {
                Serial.println("ERROR: volumeCommandQueue 未初始化");
            }
            lastVolumeUpTime = currentTime;
        } // 處理音量減少按鈕
        if (volumeDownCurrentState && !volumeDownLastState &&
            (currentTime - lastVolumeDownTime) > debounceDelay)
        {
            // 按鈕剛被按下
            if (volumeCommandQueue != NULL)
            {
                volumeCmd.cmd = VOL_DOWN;
                xQueueSend(volumeCommandQueue, &volumeCmd, 0);
                Serial.println("Volume DOWN button pressed");
            }
            else
            {
                Serial.println("ERROR: volumeCommandQueue 未初始化");
            }
            lastVolumeDownTime = currentTime;
        } // 處理麥克風控制按鈕
        if (micControlCurrentState && !micControlLastState &&
            (currentTime - lastMicControlTime) > debounceDelay)
        {
            Serial.println("🔘 麥克風控制按鈕被按下");

            // 重置看門狗，防止系統重啟
            esp_task_wdt_reset();
            if (buttonRecordingMode)
            {
                Serial.printf("📍 按鈕錄音模式 - 當前狀態: isRecording=%d\n", isRecording);

                if (diagnosticMode)
                {
                    // 診斷模式：只打印訊息，不執行實際操作
                    Serial.println("🔍 診斷模式：按鈕動作檢測成功");
                    Serial.printf("   - 按鈕狀態: 已按下\n");
                    Serial.printf("   - 時間戳: %lu ms\n", currentTime);
                    Serial.printf("   - 防抖動間隔: %lu ms\n", currentTime - lastMicControlTime);
                    Serial.printf("   - 當前錄音狀態: %s\n", isRecording ? "錄音中" : "待機");
                    Serial.println("🔍 診斷模式：跳過實際錄音操作");
                }
                else
                {
                    // 正常模式：執行錄音操作
                    if (!isRecording)
                    {
                        // 開始錄音
                        Serial.println("🎙️ 準備開始錄音...");
                        startButtonRecording();
                    }
                    else
                    {
                        // 停止錄音並處理
                        Serial.println("🛑 準備停止錄音...");
                        stopButtonRecordingAndProcess();
                    }
                }
            }
            else
            {
                // 原始持續錄音模式
                if (micEnabled)
                {
                    stopMicrophone();
                }
                else
                {
                    startMicrophone();
                }
            }
            lastMicControlTime = currentTime;
            Serial.println("✓ 麥克風控制按鈕處理完成");
        } // 更新按鈕狀態
        volumeUpLastState = volumeUpCurrentState;
        volumeDownLastState = volumeDownCurrentState;
        micControlLastState = micControlCurrentState;

        // 檢查狀態一致性，處理異常情況
        static int stateCheckCounter = 0;
        stateCheckCounter++;
        if (stateCheckCounter >= 1000)
        { // 每1000次循環檢查一次
            stateCheckCounter = 0;
            if (!micEnabled && micToSpeakerMode)
            {
                // 狀態不一致：麥克風未啟用但對講模式開啟
                Serial.println("偵測到狀態不一致：強制關閉對講模式");
                micToSpeakerMode = false;
                musicPaused = false;
            }
        }

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

// 設置 I2S 輸出（用於麥克風聲音輸出到喇叭）
esp_err_t setupI2SOutput()
{
    Serial.println("開始設置 I2S 輸出...");

    // 安全地停止並卸載現有的 I2S_NUM_0
    Serial.println("嘗試清理現有 I2S_NUM_0 資源...");

    // 使用 try-catch 風格的錯誤處理，忽略無效狀態錯誤
    esp_err_t stopResult = i2s_stop(I2S_NUM_0);
    if (stopResult == ESP_OK)
    {
        Serial.println("成功停止 I2S_NUM_0");
    }
    else if (stopResult == ESP_ERR_INVALID_STATE)
    {
        Serial.println("I2S_NUM_0 未運行，跳過停止步驟");
    }
    else
    {
        Serial.printf("警告：I2S 停止返回錯誤: %d\n", stopResult);
    }

    // 短暫延遲
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_err_t uninstallResult = i2s_driver_uninstall(I2S_NUM_0);
    if (uninstallResult == ESP_OK)
    {
        Serial.println("成功卸載 I2S_NUM_0 驅動");
    }
    else if (uninstallResult == ESP_ERR_INVALID_STATE)
    {
        Serial.println("I2S_NUM_0 驅動未安裝，跳過卸載步驟");
    }
    else
    {
        Serial.printf("警告：I2S 卸載返回錯誤: %d\n", uninstallResult);
    }

    // 較長延遲確保完全卸載
    vTaskDelay(pdMS_TO_TICKS(200));

    Serial.println("開始安裝新的 I2S_NUM_0 驅動...");

    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = MIC_SAMPLE_RATE, // 使用與麥克風相同的採樣率
        .bits_per_sample = i2s_bits_per_sample_t(MIC_SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256, // 匹配麥克風緩衝區大小
        .use_apll = false};

    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = -1};

    esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (result != ESP_OK)
    {
        Serial.printf("✗ I2S 輸出驅動安裝失敗: %d\n", result);
        return result;
    }
    Serial.println("✓ I2S 驅動安裝成功");

    result = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (result != ESP_OK)
    {
        Serial.printf("✗ I2S 輸出引腳設定失敗: %d\n", result);
        i2s_driver_uninstall(I2S_NUM_0);
        return result;
    }
    Serial.println("✓ I2S 引腳設定成功");

    result = i2s_start(I2S_NUM_0);
    if (result != ESP_OK)
    {
        Serial.printf("✗ I2S 輸出啟動失敗: %d\n", result);
        i2s_driver_uninstall(I2S_NUM_0);
        return result;
    }
    Serial.println("✓ I2S 輸出啟動成功");

    // 清空 I2S FIFO 緩衝區
    i2s_zero_dma_buffer(I2S_NUM_0);

    Serial.println("✓ I2S 輸出設定完成（對講模式）");
    return ESP_OK;
}

// 強制清理所有 I2S 資源
esp_err_t forceResetI2S()
{
    Serial.println("🔄 強制重置所有 I2S 資源...");

    // 嘗試清理所有可能的 I2S 端口
    for (int port = 0; port < I2S_NUM_MAX; port++)
    {
        Serial.printf("檢查 I2S 端口 %d...\n", port);

        // 嘗試停止
        esp_err_t stopResult = i2s_stop((i2s_port_t)port);
        if (stopResult == ESP_OK)
        {
            Serial.printf("成功停止 I2S 端口 %d\n", port);
        }

        // 嘗試卸載
        esp_err_t uninstallResult = i2s_driver_uninstall((i2s_port_t)port);
        if (uninstallResult == ESP_OK)
        {
            Serial.printf("成功卸載 I2S 端口 %d\n", port);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    Serial.println("✅ 強制 I2S 重置完成");
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待硬體重置
    return ESP_OK;
}

// 安全清理 I2S 資源
esp_err_t safeCleanupI2SResources()
{
    Serial.println("開始安全清理 I2S 資源...");

    // 只有在麥克風已啟用的情況下才進行清理
    if (!micEnabled)
    {
        Serial.println("麥克風未啟用，跳過 I2S 清理");
        return ESP_OK;
    }

    // 只清理麥克風使用的 I2S_NUM_1，避免觸碰未初始化的 I2S_NUM_0
    esp_err_t result2 = ESP_OK;

    Serial.println("檢查 I2S 端口狀態...");

    // 嘗試檢查 I2S 是否已安裝，使用更安全的方法
    bool i2sInstalled = false;

    // 嘗試停止 I2S，如果失敗說明可能未安裝
    Serial.println("嘗試停止麥克風 I2S...");
    result2 = i2s_stop(MIC_I2S_PORT);
    if (result2 == ESP_OK)
    {
        Serial.println("成功停止麥克風 I2S");
        i2sInstalled = true;
    }
    else if (result2 == ESP_ERR_INVALID_STATE)
    {
        Serial.println("麥克風 I2S 未運行，但可能已安裝");
        i2sInstalled = true; // 可能已安裝但未運行
    }
    else
    {
        Serial.printf("麥克風 I2S 停止錯誤: %d - 可能未安裝\n", result2);
        i2sInstalled = false;
    }

    // 短暫延遲
    vTaskDelay(pdMS_TO_TICKS(100));

    // 只有在確認 I2S 已安裝的情況下才嘗試卸載
    if (i2sInstalled)
    {
        Serial.println("嘗試卸載麥克風 I2S 驅動...");
        esp_err_t result4 = i2s_driver_uninstall(MIC_I2S_PORT);
        if (result4 == ESP_OK)
        {
            Serial.println("成功卸載麥克風 I2S 驅動");
        }
        else if (result4 == ESP_ERR_INVALID_STATE)
        {
            Serial.println("麥克風 I2S 驅動未安裝，跳過卸載步驟");
        }
        else
        {
            Serial.printf("麥克風 I2S 卸載警告: %d\n", result4);
        }
    }
    else
    {
        Serial.println("跳過 I2S 驅動卸載（未安裝）");
    }

    Serial.printf("I2S 清理結果 - 停止: %d\n", result2);

    // 較長延遲確保硬體完全重置
    vTaskDelay(pdMS_TO_TICKS(200));
    Serial.println("✓ I2S 資源清理完成");
    return ESP_OK;
}

// OLED 硬件檢測與初始化
bool initializeOLED()
{
    Serial.println("正在檢測 OLED 硬件...");

    // 先嘗試初始化 I2C
    Wire.begin(OLED_SDA, OLED_SCL);

    // 檢測 OLED 是否存在
    Wire.beginTransmission(OLED_ADDRESS);
    byte error = Wire.endTransmission();

    if (error == 0)
    {
        Serial.println("✓ OLED 硬件檢測成功");

        // 嘗試初始化 OLED
        try
        {
            oled.begin();
            oledHardwareAvailable = true;
            Serial.println("✓ OLED 初始化完成");
            return true;
        }
        catch (...)
        {
            Serial.println("✗ OLED 初始化失敗");
            oledHardwareAvailable = false;
            return false;
        }
    }
    else
    {
        Serial.printf("✗ OLED 硬件未檢測到 (錯誤碼: %d)\n", error);
        oledHardwareAvailable = false;
        return false;
    }
}

// 安全的 OLED 更新函數
void updateOLEDDisplay()
{
    if (!oledHardwareAvailable)
    {
        return; // 如果 OLED 硬件不可用，直接返回
    }

    try
    {
        oled.clear();

        if (buttonRecordingMode)
        {
            oled.showTitle("ProjectG - Btn Mode");

            if (isRecording)
            {
                oled.showText("Btn: REC", 0, 25);
                // 顯示錄音時間
                unsigned long elapsed = (millis() - recordingStartTime) / 1000;
                String timeStr = "Time: " + String(elapsed) + "s";
                oled.showText(timeStr.c_str(), 0, 40);
            }
            else if (pendingAudioProcessing)
            {
                oled.showText("Btn: PROC", 0, 25);
                oled.showText("Processing...", 0, 40);
            }
            else
            {
                oled.showText("Btn: READY", 0, 25);
                oled.showText("Press to start", 0, 40);
            }
        }
        else
        {
            oled.showTitle("ProjectG - Auto Mode");
            oled.showText("Auto recording", 0, 25);
        }

        // 顯示 WiFi 狀態
        if (wifiConnected)
        {
            oled.showText("WiFi: OK", 0, 55);
        }
        else
        {
            oled.showText("WiFi: --", 0, 55);
        }

        oled.display();
    }
    catch (...)
    {
        Serial.println("OLED 更新時發生錯誤");
        oledHardwareAvailable = false; // 標記為不可用
    }
}

void startMicrophone()
{
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        if (!micEnabled)
        {
            Serial.println("正在啟動麥克風錄音模式...");

            // ===== 電台播放功能已停用 =====
            Serial.println("準備啟動麥克風 (電台播放功能已停用)");
            vTaskDelay(pdMS_TO_TICKS(200));

            // 啟動麥克風 I2S
            esp_err_t micStartResult = i2s_start(MIC_I2S_PORT);
            if (micStartResult != ESP_OK && micStartResult != ESP_ERR_INVALID_STATE)
            {
                Serial.printf("✗ 麥克風啟動失敗: %d\n", micStartResult);
                xSemaphoreGive(i2sMutex);
                return;
            }

            Serial.println("✓ 麥克風 I2S 啟動成功");

            // 暫時不啟用對講模式，避免 I2S 衝突
            // 僅啟用 MQTT 錄音模式
            micEnabled = true;
            micToSpeakerMode = false; // 關閉對講模式

            Serial.println("✓ 麥克風已啟動 - MQTT 錄音模式");
            Serial.println("ℹ️  對講模式已暫時停用以避免系統衝突");

            /* 對講模式暫時停用
            // 重新配置 I2S_NUM_0 為輸出模式
            esp_err_t outputSetupResult = setupI2SOutput();
            if (outputSetupResult == ESP_OK)
            {
                micEnabled = true;
                micToSpeakerMode = true;
                Serial.println("✓ 麥克風已啟動，切換到對講模式");
            }
            else
            {
                Serial.println("✗ I2S 輸出設定失敗，使用錄音模式");
                micEnabled = true;
                micToSpeakerMode = false;
            }
            */
        }
        xSemaphoreGive(i2sMutex);
    }
    else
    {
        Serial.println("✗ 無法獲取 I2S 互斥鎖，跳過麥克風啟動");
    }
}

void stopMicrophone()
{
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        if (micEnabled)
        {
            Serial.println("正在停止麥克風..."); // 先設置狀態變量，防止 microphoneTask 繼續寫入            micToSpeakerMode = false;
            micEnabled = false;                  // 短暂延遲，让其他任务有时间检查状态变化
            vTaskDelay(pdMS_TO_TICKS(100));

            // 使用完全清理函数
            safeCleanupI2SResources();

            // ===== 電台播放功能已停用 =====
            // 重新初始化 AudioPlayer 並恢復音樂
            /*
            Serial.println("正在重新初始化音樂播放器...");

            // 使用安全初始化方法
            bool audioInitSuccess = audioPlayer.beginSafe(3);

            if (audioInitSuccess)
            {
                if (musicPaused)
                {
                    // 延遲後恢復播放
                    vTaskDelay(pdMS_TO_TICKS(500));
                    audioPlayer.playURL("http://tangosl.4hotel.tw:8005/play.mp3");
                    musicPaused = false;
                    Serial.println("✓ 音樂播放已恢復");
                }
                Serial.println("✓ 麥克風已停止，系統已恢復正常模式");
            }
            else
            {
                Serial.println("✗ AudioPlayer 初始化失敗，請檢查系統狀態");
                // 設置標誌，讓系統知道需要手動恢復
                musicPaused = true;
            }
            */
            Serial.println("✓ 麥克風已停止，電台播放功能已停用");
        }
        xSemaphoreGive(i2sMutex);
    }
    else
    {
        Serial.println("無法獲取 I2S 互斥鎖，跳過麥克風停止");
    }
}

// 麥克風任務
void microphoneTask(void *pvParameters)
{
    Serial.println("麥克風任務啟動");

    // 等待系統初始化完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 只在非按鈕錄音模式下設置麥克風
    if (!buttonRecordingMode)
    {
        if (setupMicrophone() != ESP_OK)
        {
            Serial.println("✗ 麥克風設置失敗，任務結束");
            vTaskDelete(NULL);
            return;
        }
    }
    else
    {
        Serial.println("🎙️ 按鈕錄音模式：麥克風將在按鈕按下時初始化");
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
                // 對講模式暫時停用，直接處理音訊輸出
                // 在按鈕錄音模式下，輸出音訊數據到串口供分析
                if (buttonRecordingMode)
                {
                    // 計算音頻電平用於顯示
                    long sum = 0;
                    int maxLevel = 0;
                    for (int i = 0; i < micBufLen; i++)
                    {
                        int sample = abs(micAudioBuf[i]);
                        sum += sample;
                        if (sample > maxLevel)
                            maxLevel = sample;
                    }
                    int avgLevel = sum / micBufLen;

                    // 輸出音訊數據和電平信息
                    Serial.printf("🎤 音訊數據: %d字節 | 平均電平: %d | 峰值: %d\n",
                                  bytesRead, avgLevel, maxLevel);

                    // 輸出原始音訊數據（16位有符號整數）
                    for (int i = 0; i < micBufLen; i += 8)
                    {
                        Serial.printf("樣本[%d-%d]: ", i, min(i + 7, micBufLen - 1));
                        for (int j = i; j < min(i + 8, micBufLen); j++)
                        {
                            Serial.printf("%6d ", micAudioBuf[j]);
                        }
                        Serial.println();
                    }
                }

                // MQTT 音訊傳遞：如果啟用了 MQTT 音訊，將數據發送到 MQTT 管理器
                if (mqttAudioEnabled && audioMqttManager.isPublishingActive())
                {
                    bool pushResult = audioMqttManager.pushAudioData(micAudioBuf, bytesRead / sizeof(int16_t));
                    if (!pushResult)
                    {
                        Serial.println("⚠️ MQTT 音訊數據推送失敗");
                    }
                }

                // 每500次循環顯示一次音頻電平（避免影響OLED顯示）
                audioLevelCounter++;
                if (audioLevelCounter >= 100) // 降低頻率以避免串口數據過多
                {
                    audioLevelCounter = 0;

                    // 計算音頻電平
                    long sum = 0;
                    for (int i = 0; i < micBufLen; i++)
                    {
                        sum += abs(micAudioBuf[i]);
                    }
                    int avgLevel = sum / micBufLen;

                    // 通過串口輸出電平信息
                    Serial.printf("📊 麥克風電平: %d, 字節讀取: %d\n", avgLevel, bytesRead);

                    // 顯示 MQTT 音訊狀態
                    Serial.printf("📡 MQTT 狀態 - 連接: %s, 發布: %s, 特徵提取: %s\n",
                                  audioMqttManager.isConnectedToMqtt() ? "是" : "否",
                                  audioMqttManager.isPublishingActive() ? "是" : "否",
                                  mqttFeatureExtractionEnabled ? "啟用" : "停用");
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
{ // 等待WiFi連接
    while (!wifiConnected)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // ===== 電台播放功能已停用 =====
    // 初始化音頻播放器（僅在非對講模式下）
    /*
    if (!micToSpeakerMode)
    {
        audioPlayer.begin();
        audioPlayer.setVolume(currentVolume); // 使用全局音量變數

        // 開始播放默認音頻流
        Serial.println("Starting audio playbook...");
        audioPlayer.playURL("http://tangosl.4hotel.tw:8005/play.mp3");
    }
    else
    {
        Serial.println("跳過音頻播放器初始化（對講模式）");
    }
    */
    Serial.println("電台播放功能已停用，專注於 MQTT 音訊傳遞");

    const TickType_t xDelay = 1 / portTICK_PERIOD_MS; // 1ms 延遲
    static uint32_t serialCheckCounter = 0;
    VolumeCommandData volumeCmd;
    while (true)
    {
        // ===== 電台播放功能已停用 =====
        // 在對講模式下，跳過 AudioPlayer 操作避免 I2S 衝突
        /*
        if (!micToSpeakerMode)
        {
            // 處理音頻循環 - 這必須經常被調用以避免音頻中斷
            audioPlayer.loop();
        }
        */
        // 檢查音量控制命令
        if (xQueueReceive(volumeCommandQueue, &volumeCmd, 0) == pdTRUE)
        {
            switch (volumeCmd.cmd)
            {
            case VOL_UP:
                if (currentVolume < 21)
                {
                    currentVolume++;
                    // ===== 電台音量控制已停用 =====
                    /*
                    if (!micToSpeakerMode)
                    {
                        audioPlayer.setVolume(currentVolume);
                    }
                    */
                    Serial.printf("Volume UP: %d (電台播放已停用)\n", currentVolume);
                }
                break;
            case VOL_DOWN:
                if (currentVolume > 0)
                {
                    currentVolume--;
                    // ===== 電台音量控制已停用 =====
                    /*
                    if (!micToSpeakerMode)
                    {
                        audioPlayer.setVolume(currentVolume);
                    }
                    */
                    Serial.printf("Volume DOWN: %d (電台播放已停用)\n", currentVolume);
                }
                break;
            case VOL_SET:
                if (volumeCmd.volume >= 0 && volumeCmd.volume <= 21)
                {
                    currentVolume = volumeCmd.volume;
                    // ===== 電台音量控制已停用 =====
                    /*
                    if (!micToSpeakerMode)
                    {
                        audioPlayer.setVolume(currentVolume);
                    }
                    */
                    Serial.printf("Volume SET: %d (電台播放已停用)\n", currentVolume);
                }
                break;
            }
        } // 每1000次循環才檢查一次串列埠輸入，避免影響音頻性能
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
                    // ===== 電台播放功能已停用 =====
                    /*
                    Serial.println("Changing audio source to: " + r);
                    audioPlayer.stopAndPlay(r.c_str());
                    */
                    Serial.println("電台播放功能已停用，忽略串列輸入: " + r);
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

    Serial.println("========================================");
    Serial.println("🎤 ESP32-S3 ProjectG MQTT 音訊系統");
    Serial.println("📡 自動錄音模式啟動");
    Serial.println("🔬 含特徵提取功能");
    Serial.println("========================================");

    randomSeed(analogRead(A0));
    pinMode(LED_PIN, OUTPUT); // 初始化音量控制按鈕
    pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
    pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
    pinMode(MIC_CONTROL_PIN, INPUT_PULLUP);
    Serial.println("✓ 音量控制和麥克風按鈕已初始化");

    // 初始化 OLED 顯示（帶硬件檢測）
    Serial.println("正在初始化 OLED...");
    bool oledAvailable = initializeOLED();
    if (oledAvailable)
    {
        Serial.println("✓ OLED 初始化成功");
        // 顯示啟動信息
        oled.clear();
        oled.showTitle("ProjectG");
        oled.showText("System starting...", 0, 30);
        oled.display();
    }
    else
    {
        Serial.println("⚠ OLED 硬件未檢測到，將以無顯示模式運行");
    }

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
    i2sMutex = xSemaphoreCreateMutex();

    // 檢查隊列和互斥鎖創建是否成功
    if (ledCommandQueue == NULL || volumeCommandQueue == NULL || ledMutex == NULL || i2sMutex == NULL)
    {
        Serial.println("ERROR: 隊列或互斥鎖創建失敗!");
        while (1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    // 安全顯示系統信息
    if (oledHardwareAvailable)
    {
        oled.showSimpleSystemInfo();
    }
    else
    {
        Serial.println("OLED 不可用，跳過系統信息顯示");
    }

    // 初始化 LED 控制器
    // 為 ESP32-S3 N16R8 設置適合的 LED 引腳
    // NeoPixel: GPIO48, RGB LED: GPIO35 (紅), GPIO36 (綠), GPIO37 (藍)
    Serial.println("正在初始化 LED 控制器...");
    ledController.setPinConfig(48, 35, 36, 37);

    // 嘗試初始化 LED 控制器
    try
    {
        ledController.begin();
        Serial.println("✓ LED 控制器初始化成功");
    }
    catch (...)
    {
        Serial.println("⚠️ LED 控制器初始化失敗，使用基本模式");
    }

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

    // 短暫延遲確保任務啟動
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // 運行LED測試序列（現在隊列已經創建並且任務已啟動）
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
    );                   // 創建按鈕控制任務
    xTaskCreatePinnedToCore(
        buttonControlTask, // 任務函數
        "Button Control",  // 任務名稱
        8192,              // 增加堆棧大小到8KB，避免堆疊溢出
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
    xTaskCreatePinnedToCore(audioControlTask, // 任務函數
                            "Audio Control",  // 任務名稱
                            8192,             // 增加堆棧大小以處理音頻緩衝
                            NULL,             // 參數
                            3,                // 高優先級 (3 是高優先級)
                            &audioTaskHandle, // 任務控制句柄
                            PRO_CPU_NUM       // 在主核心上運行以獲得更好的性能
    );                                        // 初始化 MQTT 音訊管理器（暫時停用以測試基本功能）
    /*
    if (wifiConnected)
    {
        Serial.println("正在初始化 MQTT 音訊管理器...");

        // 延遲確保 WiFi 穩定
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        if (audioMqttManager.begin(mqttServer, mqttPort, mqttUser, mqttPassword, "ESP32_ProjectG"))
        {
            Serial.println("✓ MQTT 音訊管理器初始化成功");

            // 延遲後再嘗試連接
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            // 連接到 MQTT 服務器
            Serial.println("正在嘗試連接到 MQTT 服務器...");
            if (audioMqttManager.connect())
            {
                mqttAudioEnabled = true;
                Serial.printf("✓ MQTT 服務器連接成功: %s:%d\n", mqttServer, mqttPort);

                // 預設啟用特徵提取
                if (audioMqttManager.enableFeatureExtraction())
                {
                    mqttFeatureExtractionEnabled = true;
                    Serial.println("✓ MQTT 特徵提取已啟用");
                }

                // 啟動音訊發布
                if (audioMqttManager.startPublishing())
                {
                    Serial.println("✓ MQTT 音訊發布已啟動");
                }
            }
            else
            {
                Serial.println("⚠️ MQTT 服務器連接失敗，將在後台重試");
                // 不阻塞，讓系統繼續運行
            }
        }
        else
        {
            Serial.println("✗ MQTT 音訊管理器初始化失敗");
        }
    }
    else
    {
        Serial.println("⚠️  WiFi 未連接，跳過 MQTT 音訊初始化");
    }
    */
    Serial.println("ℹ️ MQTT 功能暫時停用，專注測試按鈕錄音基本功能"); // 🎤 錄音模式初始化
    if (buttonRecordingMode)
    {
        Serial.println("🎙️ 按鈕錄音模式已啟用");
        Serial.println("============================================================");
        Serial.println("📋 使用說明:");
        Serial.println("   1. 按下麥克風按鈕開始錄音");
        Serial.println("   2. 再按一次停止錄音並處理音訊");
        Serial.println("   3. 音訊將透過串口輸出（MQTT暫時停用）");
        Serial.println("============================================================");
        Serial.println("🎙️ 按鈕錄音系統就緒 - 按下麥克風按鈕開始錄音");
        Serial.println("============================================================");

        // 設置 LED 為待機狀態（綠色）
        LedCommandData cmd;
        cmd.cmd = CMD_SET_COLOR;
        cmd.r = 0;
        cmd.g = 255;
        cmd.b = 0;
        if (ledCommandQueue != NULL)
        {
            xQueueSend(ledCommandQueue, &cmd, 0);
        }
    }
    else
    {
        Serial.println("🎤 正在自動啟動麥克風錄音模式...");
        vTaskDelay(500 / portTICK_PERIOD_MS); // 縮短等待時間

        // 在背景任務中啟動麥克風，避免阻塞主流程
        xTaskCreatePinnedToCore(
            [](void *parameter)
            {
                vTaskDelay(2000 / portTICK_PERIOD_MS); // 等待系統穩定
                Serial.println("背景啟動麥克風...");
                startMicrophone();

                if (micEnabled)
                {
                    Serial.println("✓ 麥克風錄音模式已自動啟動");
                    Serial.println("📡 音訊數據將透過 MQTT 傳遞");
                    Serial.println("🔬 特徵提取功能已啟用");
                }
                else
                {
                    Serial.println("✗ 麥克風自動啟動失敗");
                }

                vTaskDelete(NULL); // 刪除此一次性任務
            },
            "Mic Startup",
            4096,
            NULL,
            1,
            NULL,
            APP_CPU_NUM);
    }

    Serial.println("📟 系統初始化完成，麥克風將在背景啟動");
}

void loop()
{
    // 在使用 FreeRTOS 時，主循環可以保持空白或執行低優先級任務
    // 所有主要功能都已在任務中完成

    // 非阻塞式處理 MQTT 循環
    if (mqttAudioEnabled)
    {
        audioMqttManager.loop();
    }

    // 減少檢查頻率，避免看門狗問題
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms 延遲

    // 系統監控（降低頻率）
    static uint32_t watchdogCounter = 0;
    watchdogCounter++;

    if (watchdogCounter % 50 == 0) // 每5秒檢查一次
    {
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
    }

    // 定期輸出系統狀態（每30秒）
    static uint32_t lastStatusTime = 0;
    if (millis() - lastStatusTime > 30000)
    {
        lastStatusTime = millis();
        Serial.printf("Free heap: %d bytes, Audio task running: %s\n",
                      ESP.getFreeHeap(),
                      (audioTaskHandle != NULL) ? "Yes" : "No");

        // 輸出 MQTT 狀態（非阻塞）
        if (mqttAudioEnabled)
        {
            Serial.println("MQTT 音訊系統運行中...");
        }
    }

    // 重置看門狗
    esp_task_wdt_reset();
}

// 按鈕控制錄音模式函數
void startButtonRecording()
{
    if (isRecording)
    {
        Serial.println("⚠️ 已在錄音中，無法重複開始");
        return;
    }

    Serial.println("============================================================");
    Serial.println("🎙️ 開始按鈕錄音模式測試");
    Serial.println("============================================================");

    // 清除待處理標誌
    pendingAudioProcessing = false;

    // 記錄開始時間
    recordingStartTime = millis();

    // 添加看門狗重置，防止系統重啟
    esp_task_wdt_reset();
    if (testMutexOnly)
    {
        Serial.println("🔧 測試階段1：只測試互斥鎖獲取");

        // 測試獲取互斥鎖
        if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            Serial.println("✅ 成功獲取 I2S 互斥鎖");
            vTaskDelay(pdMS_TO_TICKS(100)); // 模擬一些處理時間
            Serial.println("✅ 模擬處理完成，準備釋放鎖");
            xSemaphoreGive(i2sMutex);
            Serial.println("✅ 互斥鎖測試完成，無問題");
        }
        else
        {
            Serial.println("❌ 無法獲取 I2S 互斥鎖 - 這可能是問題所在");
        }
        return;
    }
    if (testDirectSetup)
    {
        Serial.println("🔧 測試階段4：直接麥克風設置（無任何清理）");

        // 測試獲取互斥鎖和直接設置麥克風
        if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            Serial.println("✅ 成功獲取 I2S 互斥鎖");

            if (!micEnabled)
            {
                Serial.println("🎤 直接嘗試麥克風設置（無清理）...");

                // 完全跳過任何清理操作，直接設置
                esp_err_t setupResult = setupMicrophone();
                if (setupResult == ESP_OK)
                {
                    Serial.println("✅ 麥克風設置成功！");
                    micEnabled = true;
                    isRecording = true;

                    // 嘗試啟動 I2S
                    esp_err_t startResult = i2s_start(MIC_I2S_PORT);
                    if (startResult == ESP_OK)
                    {
                        Serial.println("✅ I2S 啟動成功！");
                    }
                    else
                    {
                        Serial.printf("⚠️ I2S 啟動警告: %d\n", startResult);
                    }

                    Serial.println("✅ 錄音狀態已設置");
                }
                else
                {
                    Serial.printf("❌ 麥克風設置失敗，錯誤代碼: %d\n", setupResult);

                    // 顯示錯誤代碼含義
                    if (setupResult == ESP_ERR_INVALID_STATE)
                    {
                        Serial.println("   → ESP_ERR_INVALID_STATE: I2S 端口可能已被使用");
                    }
                    else if (setupResult == ESP_ERR_NO_MEM)
                    {
                        Serial.println("   → ESP_ERR_NO_MEM: 記憶體不足");
                    }
                    else if (setupResult == ESP_ERR_INVALID_ARG)
                    {
                        Serial.println("   → ESP_ERR_INVALID_ARG: 參數無效");
                    }
                }
            }
            else
            {
                Serial.println("⚠️ 麥克風已啟用");
            }

            xSemaphoreGive(i2sMutex);
            Serial.println("✅ 直接設置測試完成");
        }
        else
        {
            Serial.println("❌ 無法獲取 I2S 互斥鎖");
        }
        return;
    }

    if (testSetupOnly)
    {
        Serial.println("🔧 測試階段3：測試麥克風設置操作（跳過清理）");

        // 測試獲取互斥鎖和麥克風設置
        if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            Serial.println("✅ 成功獲取 I2S 互斥鎖");
            if (!micEnabled)
            {
                Serial.println("🎤 開始測試麥克風設置（使用強制重置）...");

                // 先強制重置所有 I2S 資源
                forceResetI2S();

                // 然後設置麥克風
                esp_err_t setupResult = setupMicrophone();
                if (setupResult == ESP_OK)
                {
                    Serial.println("✅ 麥克風設置成功！");
                    micEnabled = true;
                    isRecording = true;

                    Serial.println("✅ 錄音狀態已設置");
                }
                else
                {
                    Serial.printf("❌ 麥克風設置失敗，錯誤代碼: %d\n", setupResult);
                }
            }
            else
            {
                Serial.println("⚠️ 麥克風已啟用");
            }

            xSemaphoreGive(i2sMutex);
            Serial.println("✅ 麥克風設置測試完成");
        }
        else
        {
            Serial.println("❌ 無法獲取 I2S 互斥鎖");
        }
        return;
    } // 啟動麥克風（正常模式）
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
    {
        if (!micEnabled)
        {
            Serial.println("正在啟動麥克風用於按鈕錄音...");

            // 在按鈕錄音模式下，第一次啟動不需要清理
            // 只有在重複啟動時才需要清理（但我們會在 stop 函數中處理）
            Serial.println("步驟1: 直接設置麥克風");
            esp_err_t setupResult = setupMicrophone();
            if (setupResult == ESP_OK)
            {
                Serial.println("步驟2: 啟動 I2S");
                esp_err_t startResult = i2s_start(MIC_I2S_PORT);
                if (startResult == ESP_OK)
                {
                    micEnabled = true;
                    isRecording = true;

                    // 啟動 MQTT 音訊發布
                    if (mqttAudioEnabled)
                    {
                        Serial.println("✓ MQTT 音訊錄製模式已啟動");
                    }

                    Serial.println("✓ 按鈕錄音已開始 - 再按一次停止並處理");
                    Serial.println("============================================================");

                    // 更新 LED 狀態為錄音中（紅色呼吸燈）
                    LedCommandData cmd;
                    cmd.cmd = CMD_BREATHE;
                    cmd.r = 255;
                    cmd.g = 0;
                    cmd.b = 0;
                    if (ledCommandQueue != NULL)
                    {
                        xQueueSend(ledCommandQueue, &cmd, 0);
                    }
                }
                else
                {
                    Serial.printf("✗ I2S 啟動失敗: %d\n", startResult);
                    setupResult = startResult; // 讓下面的錯誤處理統一處理
                }
            }

            if (setupResult != ESP_OK)
            {
                Serial.printf("✗ 麥克風啟動失敗，錯誤代碼: %d\n", setupResult);
                isRecording = false;
                micEnabled = false;

                // 顯示錯誤代碼含義
                if (setupResult == ESP_ERR_INVALID_STATE)
                {
                    Serial.println("   → ESP_ERR_INVALID_STATE: I2S 端口可能已被使用");
                    Serial.println("   → 建議重啟系統以清理 I2S 資源");
                }
            }
        }
        else
        {
            Serial.println("⚠️ 麥克風已啟用，無需重複啟動");
        }
        xSemaphoreGive(i2sMutex);
    }
    else
    {
        Serial.println("✗ 無法獲取 I2S 互斥鎖，錄音啟動失敗");
    }
}

void stopButtonRecordingAndProcess()
{
    if (!isRecording)
    {
        Serial.println("⚠️ 目前未在錄音，無法停止");
        return;
    }

    // 計算錄音時長
    recordingDuration = millis() - recordingStartTime;

    Serial.println("============================================================");
    Serial.printf("🛑 停止按鈕錄音 - 錄音時長: %.2f 秒\n", recordingDuration / 1000.0);
    Serial.println("============================================================");

    // 停止麥克風
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        if (micEnabled)
        {
            Serial.println("正在停止麥克風錄音...");

            // 先清理 I2S 資源（在設置 micEnabled = false 之前）
            Serial.println("步驟1: 清理 I2S 資源");
            esp_err_t stopResult = i2s_stop(MIC_I2S_PORT);
            if (stopResult == ESP_OK)
            {
                Serial.println("✅ 成功停止 I2S");
            }
            else
            {
                Serial.printf("⚠️ I2S 停止警告: %d\n", stopResult);
            }

            vTaskDelay(pdMS_TO_TICKS(100));

            esp_err_t uninstallResult = i2s_driver_uninstall(MIC_I2S_PORT);
            if (uninstallResult == ESP_OK)
            {
                Serial.println("✅ 成功卸載 I2S 驅動");
            }
            else
            {
                Serial.printf("⚠️ I2S 卸載警告: %d\n", uninstallResult);
            }

            // 然後設置狀態變數
            micEnabled = false;
            isRecording = false;
            pendingAudioProcessing = true;

            vTaskDelay(pdMS_TO_TICKS(200)); // 等待硬體完全重置

            Serial.println("✓ 麥克風錄音已停止，I2S 資源已釋放");

            // 更新 LED 狀態為處理中（藍色常亮）
            LedCommand cmd = CMD_SET_COLOR;
            if (ledCommandQueue != NULL)
            {
                xQueueSend(ledCommandQueue, &cmd, 0);
            }
        }
        xSemaphoreGive(i2sMutex);
    }

    // 處理錄製的音訊
    processRecordedAudio();
}

void processRecordedAudio()
{
    if (!pendingAudioProcessing)
    {
        return;
    }

    Serial.println("============================================================");
    Serial.println("🔬 開始處理錄製的音訊數據...");
    Serial.println("============================================================");

    // 等待 MQTT 發布任務完成剩餘數據發送
    Serial.println("⏳ 等待音訊數據傳輸完成...");
    vTaskDelay(pdMS_TO_TICKS(2000)); // 獲取處理統計
    if (audioMqttManager.isConnectedToMqtt())
    {
        Serial.printf("📊 處理統計:\n");
        Serial.printf("   - 錄音時長: %.2f 秒\n", recordingDuration / 1000.0);
        Serial.printf("   - 預估音訊包數: %d\n", (int)(recordingDuration * 16000 / 256 / 1000));

        // 顯示音訊處理完成
        Serial.println("✅ 音訊處理完成！");
        Serial.println("📤 音訊數據已透過 MQTT 傳送");

        if (mqttFeatureExtractionEnabled)
        {
            Serial.println("🔬 特徵提取數據已同步傳送");
        }
    }
    else
    {
        Serial.println("⚠️ MQTT 未連接，音訊數據未能傳送");
    }

    // 清除處理標誌
    pendingAudioProcessing = false;

    // 更新 LED 狀態為待機（綠色常亮）
    LedCommand cmd = CMD_SET_COLOR;
    if (ledCommandQueue != NULL)
    {
        xQueueSend(ledCommandQueue, &cmd, 0);
    }

    Serial.println("============================================================");
    Serial.println("🎙️ 按鈕錄音系統就緒 - 按下麥克風按鈕開始新錄音");
    Serial.println("============================================================");
}
