# ESP32-S3 ProjectG - 按鈕控制 MQTT 音訊系統

## 📖 專案概述

ESP32-S3 ProjectG 是一個基於 ESP32-S3 N16R8 開發板的智能音訊處理系統，支援按鈕控制錄音、MQTT 音訊傳輸、即時特徵提取和多媒體播放功能。本專案從原本的「開機自動錄音」模式改進為「按鈕控制錄音」模式，提供更精確的音訊錄製控制。

### 🎯 核心功能

- **🎯 按鈕控制錄音**：按下按鈕開始錄音，再按一次停止並處理音訊
- **📡 MQTT 音訊傳輸**：即時音訊數據透過 MQTT 協議傳送到遠端服務器
- **🎵 音訊特徵提取**：支援 MFCC、梅爾頻譜、頻譜重心等特徵計算
- **💡 智能狀態指示**：LED 和 OLED 顯示器反映系統當前狀態
- **🔧 硬體自適應**：自動檢測硬體可用性並降級運行
- **⚡ 安全資源管理**：I2S 資源安全清理，避免系統崩潰

### 🎮 操作方式

1. **開始錄音**：按下麥克風控制按鈕
   - LED 變為紅色呼吸燈
   - OLED 顯示 "Btn: REC" + 錄音時間

2. **停止錄音**：再次按下麥克風控制按鈕
   - LED 變為藍色處理狀態
   - 自動處理音訊數據並輸出

3. **系統就緒**：處理完成後
   - LED 變為綠色待機
   - OLED 顯示 "Btn: READY"

### 專案歷程

本專案經歷了從自動錄音到按鈕控制的重大重構：

1. **初始版本**：開機自動開始 MQTT 音訊傳輸
2. **按鈕控制版本**：改為用戶可控的錄音模式
3. **穩定版本**：解決 I2S 資源管理問題，實現穩定運行

## 🔌 硬體配置

### 開發板規格

- **主控芯片**：ESP32-S3 WROOM-1 N16R8
- **Flash 記憶體**：16 MB
- **PSRAM**：8 MB
- **WiFi**：2.4 GHz 802.11 b/g/n
- **藍牙**：Bluetooth 5.0 LE

推薦購買：[ESP32-S3 N16R8 開發板](https://www.taiwansensor.com.tw/product/esp32-s3-devkitc-1-%E9%96%8B%E7%99%BC%E6%9D%BF-%E6%A8%82%E9%91%AB-wroom-1-n16r8-%E5%B7%B2%E7%84%8A%E6%8E%A5%E9%87%9D%E8%85%B3/?srsltid=AfmBOopRlAkA0Pqg56TTQj8SVzKgSCUIz-w4B6i-VNpxVR8ESW0LrRBx)

### 完整接線圖

```text
ESP32-S3 開發板接線圖
├── I2S 麥克風 (INMP441)
│   ├── SCK  → GPIO20 (I2S 時鐘)
│   ├── WS   → GPIO18 (字選擇)
│   ├── SD   → GPIO21 (數據輸入)
│   ├── VDD  → 3.3V
│   ├── GND  → GND
│   └── L/R  → GND (左聲道)
│
├── OLED 顯示器 (SH1106/SSD1306)
│   ├── SDA  → GPIO8  (I2C 數據)
│   ├── SCL  → GPIO9  (I2C 時鐘)
│   ├── VCC  → 3.3V
│   └── GND  → GND
│
├── 控制按鈕
│   ├── 音量增加    → GPIO1  → GND
│   ├── 音量減少    → GPIO2  → GND
│   └── 麥克風控制  → GPIO42 → GND
│
└── LED 指示燈
    ├── RGB LED (共陰極)
    │   ├── 紅色 → GPIO35
    │   ├── 綠色 → GPIO36
    │   └── 藍色 → GPIO37
    │
    └── NeoPixel/WS2812B
        ├── 數據 → GPIO48
        ├── VCC  → 5V
        └── GND  → GND
```

### 引腳配置表

| 功能 | GPIO | 類型 | 說明 |
|------|------|------|------|
| OLED SDA | GPIO8 | I2C | I2C 數據線 |
| OLED SCL | GPIO9 | I2C | I2C 時鐘線 |
| 音量增加 | GPIO1 | 按鈕 | 內部上拉，按下接地 |
| 音量減少 | GPIO2 | 按鈕 | 內部上拉，按下接地 |
| 麥克風控制 | GPIO42 | 按鈕 | 內部上拉，錄音控制 |
| 麥克風 WS | GPIO18 | I2S | I2S 字選擇信號 |
| 麥克風 SD | GPIO21 | I2S | I2S 數據輸入 |
| 麥克風 SCK | GPIO20 | I2S | I2S 時鐘信號 |
| LED 控制 | GPIO48 | NeoPixel | 狀態指示燈 |
| 紅色 LED | GPIO35 | 輸出 | RGB LED 紅色分量 |
| 綠色 LED | GPIO36 | 輸出 | RGB LED 綠色分量 |
| 藍色 LED | GPIO37 | 輸出 | RGB LED 藍色分量 |

### 按鈕控制

| 功能 | ESP32-S3 引腳 | 接線方式 |
|------|---------------|----------|
| 音量增加 | GPIO1 | 按鈕 -> GND |
| 音量減少 | GPIO2 | 按鈕 -> GND |
| 麥克風控制 | GPIO42 | 按鈕 -> GND |

## 🏗️ 軟體架構

### 系統架構圖

```text
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 ProjectG                       │
├─────────────────────────────────────────────────────────────┤
│ 任務管理 (FreeRTOS)                                        │
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│ │按鈕控制任務  │ │麥克風任務    │ │OLED顯示任務 │           │
│ │buttonControl│ │microphone   │ │oledUpdate   │           │
│ │Task         │ │Task         │ │Task         │           │
│ └─────────────┘ └─────────────┘ └─────────────┘           │
│ ┌─────────────┐ ┌─────────────┐                           │
│ │LED控制任務   │ │音訊控制任務  │                           │
│ │ledControl   │ │audioControl │                           │
│ │Task         │ │Task         │                           │
│ └─────────────┘ └─────────────┘                           │
├─────────────────────────────────────────────────────────────┤
│ 硬體抽象層                                                  │
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│ │I2S 驅動     │ │I2C 驅動     │ │GPIO 驅動    │           │
│ │(麥克風)     │ │(OLED)       │ │(按鈕/LED)   │           │
│ └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│ 通訊協議                                                    │
│ ┌─────────────┐ ┌─────────────┐                           │
│ │WiFi 管理    │ │MQTT 客戶端  │                           │
│ │(自動重連)   │ │(音訊傳輸)   │                           │
│ └─────────────┘ └─────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

### 核心功能模組

#### 1. 按鈕控制系統

- **檔案**：`src/main.cpp` (buttonControlTask 函數)
- **功能**：監控按鈕輸入，處理防抖動，管理錄音狀態
- **狀態機**：待機 → 錄音中 → 處理中 → 待機

#### 2. I2S 音訊處理

- **檔案**：`src/main.cpp` (microphoneTask 函數)
- **功能**：16kHz 16-bit 音訊採集，電平監控，資料輸出
- **關鍵配置**：
  - 採樣率：16kHz
  - 位深度：16-bit
  - 聲道：單聲道 (左聲道)
  - DMA 緩衝區：4 × 512 樣本

#### 3. MQTT 音訊傳輸

- **檔案**：`src/AudioMqttManager.cpp` (目前停用)
- **功能**：音訊數據封包化，MQTT 發布，特徵提取
- **主題結構**：
  - `esp32/audio/raw` - 原始音訊數據
  - `esp32/audio/mfcc` - MFCC 特徵係數
  - `esp32/audio/features` - 統計特徵

#### 4. 狀態顯示系統

- **OLED 顯示**：`src/OledDisplay.cpp`
- **LED 控制**：`src/LedController.cpp`
- **狀態映射**：
  - 待機：綠色常亮，OLED 顯示 "Btn: READY"
  - 錄音：紅色呼吸燈，OLED 顯示 "Btn: REC" + 時間
  - 處理：藍色常亮，OLED 顯示 "Btn: PROC"

### 工作流程

#### 按鈕錄音完整流程

```text
用戶操作                系統狀態               硬體指示
   │                      │                     │
 按下按鈕  ────────────▶  開始錄音              LED 紅色呼吸燈
   │                      │                   OLED "Btn: REC"
   │                   I2S 初始化              
   │                   音訊數據收集             串口輸出音訊數據
   │                      │                     │
 再按按鈕  ────────────▶  停止錄音              LED 藍色常亮
   │                      │                   OLED "Btn: PROC"
   │                   I2S 資源清理            
   │                   音訊特徵提取             
   │                   MQTT 數據傳送           
   │                      │                     │
   │      ────────────▶  回到待機              LED 綠色常亮
   │                      │                   OLED "Btn: READY"
```

### I2S 配置

```cpp
// 麥克風 I2S 配置 (I2S_NUM_1)
const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
};
```

## ⚙️ 軟體安裝與配置

### 1. 開發環境設置

```bash
# 安裝 PlatformIO
pip install platformio

# 進入專案目錄
cd "d:\workspace\study\ProjectG"

# 編譯專案
pio run

# 上傳程式
pio run --target upload

# 監控串列輸出
pio device monitor
```

### 2. WiFi 設置

在 `src/main.cpp` 中修改 WiFi 設定：

```cpp
// WiFi 設置
const char *ssid = "您的WiFi名稱";
const char *password = "您的WiFi密碼";
```

### 3. MQTT 服務器設置

系統已預設使用免費的 Eclipse Mosquitto 公共服務器：

```cpp
// MQTT 設置
const char *mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char *mqttUser = nullptr;     // 無需認證
const char *mqttPassword = nullptr; // 無需認證
```

## 📡 MQTT 協議架構

### MQTT 主題結構

| 主題 | 方向 | 內容 | 格式 |
|------|------|------|------|
| `esp32/audio/raw` | 發布 | 原始音訊數據 | JSON |
| `esp32/audio/mfcc` | 發布 | MFCC 係數 (13個) | JSON |
| `esp32/audio/mel` | 發布 | 梅爾能量 (26個) | JSON |
| `esp32/audio/features` | 發布 | 音訊特徵 | JSON |
| `esp32/audio/status` | 發布 | 系統狀態 | JSON |
| `esp32/audio/control` | 訂閱 | 控制命令 | JSON |

### 控制命令

發送到 `esp32/audio/control` 主題：

```json
// 開始音訊發布
{"command": "startPublishing", "timestamp": 1234567890}

// 停止音訊發布
{"command": "stopPublishing", "timestamp": 1234567890}

// 啟用特徵提取
{"command": "enableFeatures", "timestamp": 1234567890}

// 停用特徵提取
{"command": "disableFeatures", "timestamp": 1234567890}
```

## 🔬 音訊特徵提取

### 特徵類型

1. **MFCC 係數 (13個)**：語音識別常用特徵
2. **梅爾濾波器組能量 (26個)**：梅爾刻度的頻譜能量
3. **頻譜重心**：頻譜的"重心"位置
4. **頻譜帶寬**：頻譜能量的分散程度
5. **過零率**：音訊信號的過零頻率
6. **RMS 能量**：音訊信號的均方根能量

### 處理參數

- **採樣率**：16 kHz
- **位深度**：16 位
- **FFT 大小**：512
- **視窗大小**：400 樣本 (25ms)
- **跳躍大小**：160 樣本 (10ms)

## 🚀 免費 MQTT 服務器

### 推薦服務器

#### 1. Eclipse Mosquitto (預設，推薦)

```cpp
const char *mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
```

- ✅ 最穩定：Eclipse 基金會官方維護
- ✅ 無需註冊：直接使用
- ✅ 全球可用：24/7 可用性

#### 2. HiveMQ 公共服務器

```cpp
const char *mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
```

#### 3. EMQX 公共服務器

```cpp
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
```

## 🧪 測試與驗證

### Python 測試腳本

```bash
# 簡單 MQTT 測試
python simple_mqtt_test.py

# 音訊客戶端測試
python test_mqtt_audio_client.py

# 服務器連接測試
python test_mqtt_servers.py
```

### MQTT 客戶端工具測試

```bash
# 訂閱所有音訊主題
mosquitto_sub -h test.mosquitto.org -t "esp32/audio/#" -v

# 發送控制命令
mosquitto_pub -h test.mosquitto.org -t "esp32/audio/control" -m '{"command":"startPublishing"}'
```

## 📁 專案檔案結構

```text
ProjectG/
├── src/
│   ├── main.cpp                 # 主程式
│   ├── AudioMqttManager.cpp     # MQTT 音訊管理
│   ├── AudioFeatureExtractor.cpp # 音訊特徵提取
│   ├── LedController.cpp        # LED 控制
│   ├── OledDisplay.cpp          # OLED 顯示
│   └── AudioPlayer.cpp          # 音訊播放
├── include/
│   ├── AudioMqttManager.h
│   ├── AudioFeatureExtractor.h
│   ├── LedController.h
│   ├── OledDisplay.h
│   └── AudioPlayer.h
├── test/
│   ├── simple_mqtt_test.py      # 簡單 MQTT 測試
│   ├── test_mqtt_audio_client.py # 音訊客戶端測試
│   └── test_mqtt_servers.py     # 服務器測試
├── platformio.ini               # PlatformIO 配置
└── README.md                    # 本說明文件
```

## 📝 依賴套件

已在 `platformio.ini` 中配置：

```ini
lib_deps = 
    adafruit/Adafruit NeoPixel@^1.11.0
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays@^4.4.0
    bblanchon/ArduinoJson@^7.0.0
    knolleary/PubSubClient@^2.8
```

## ❓ 常見問題

### Q1: 編譯錯誤 "No such file or directory"

**A**: 確保已安裝所有依賴套件：`pio lib install`

### Q2: WiFi 連接失敗

**A**: 檢查 WiFi 憑證和信號強度，確保使用 2.4GHz 網路。

### Q3: MQTT 連接失敗

**A**: 檢查網路連接，嘗試其他 MQTT 服務器，檢查防火牆設定。

### Q4: 麥克風無聲音

**A**: 檢查接線是否正確，確認麥克風電源供應，檢查 GPIO 引腳配置。

### Q5: OLED 不顯示

**A**: 檢查 I2C 接線，確認 OLED 模組類型，檢查電源供應。

## 🔧 詳細故障排除

### 編譯和上傳問題

#### 編譯錯誤

**問題**：Library not found 或依賴庫錯誤

**解決方案**：

```bash
# 安裝依賴庫
platformio lib install

# 清理並重新編譯
platformio run --target clean
platformio run
```

#### 上傳失敗

**問題**：無法找到 COM 端口或上傳超時

**解決方案**：

- 檢查 USB 線是否支援數據傳輸
- 確認開發板驅動程式已安裝
- 嘗試不同的 COM 端口
- 在上傳時按住 Boot 按鈕

### 硬體問題

#### 按鈕無響應

**解決方案**：

- 檢查按鈕接線是否正確
- 確認 GPIO 引腳配置
- 測試按鈕硬體是否正常
- 調整防抖動時間設定

#### 音訊問題

**問題**：麥克風無聲音或數據全零

**解決方案**：

- 確認麥克風接線正確
- 檢查 I2S 引腳配置
- 測試麥克風電源供應
- 查看串口輸出的音訊電平資訊

#### 系統重啟問題

**問題**：Guru Meditation Error 或頻繁重啟

**解決方案**：

- 檢查電源供應是否穩定
- 確認沒有引腳衝突
- 重新燒錄完整韌體
- 查看錯誤日誌定位問題原因

### 除錯工具

#### 串口除錯

```bash
# Windows
platformio device monitor --port COM7 --baud 115200

# Linux/Mac
platformio device monitor --port /dev/ttyUSB0 --baud 115200
```

#### MQTT 除錯

```bash
# 訂閱所有主題
mosquitto_sub -h test.mosquitto.org -t "esp32/audio/#" -v

# 發送測試命令
mosquitto_pub -h test.mosquitto.org -t "esp32/audio/control" \
  -m '{"command":"startPublishing","timestamp":1234567890}'
```

## 📖 技術參考

### 核心 API

```cpp
// 錄音控制
void startButtonRecording();
void stopButtonRecordingAndProcess();
void processRecordedAudio();

// I2S 管理
bool setupMicrophone();
void safeCleanupI2SResources();

// 狀態管理
void updateSystemStatus();
void updateOLEDDisplay();
void updateLEDStatus();

// 網路通訊
bool connectToWiFi();
bool connectToMQTT();
void publishAudioData(const int16_t* data, size_t length);
```

### 效能指標

| 項目 | 數值 | 說明 |
|------|------|------|
| 音訊採樣率 | 16 kHz | 語音應用標準採樣率 |
| 音訊位深度 | 16-bit | CD 品質位深度 |
| 處理延遲 | < 100 ms | 從錄音到 MQTT 發布 |
| 記憶體使用 | ~46 KB RAM | 運行時記憶體佔用 |
| Flash 使用 | ~880 KB | 程式碼和庫檔案大小 |
| WiFi 連接時間 | < 10 s | 首次連接到可用網路 |
| I2S 啟動時間 | < 200 ms | I2S 初始化完成時間 |
| 功耗 | ~80 mA | 正常運行時平均功耗 |

### 系統限制

- **最大錄音時長**：受可用 RAM 限制，約 30-60 秒
- **同時 I2S 端口**：1 個 (I2S_NUM_1)
- **支援音訊格式**：PCM 16-bit 單聲道
- **網路協議**：WiFi 2.4GHz，不支援 5GHz
- **MQTT 封包大小**：最大 256 樣本/封包

## 📚 版本歷史

### v1.0.0 (當前版本) - 2025-07-02

**新功能：**

- ✅ 完整的按鈕控制錄音功能
- ✅ 安全的 I2S 資源管理
- ✅ OLED/LED 狀態指示系統
- ✅ WiFi 自動連接和重連
- ✅ 硬體自適應和降級機制

**修復問題：**

- 🐛 解決按鈕觸發系統重啟問題
- 🐛 修復 I2S 資源清理時的崩潰問題
- 🐛 解決 OLED 硬體缺失時的系統異常

**已知問題：**

- ⚠️ MQTT 功能暫時停用 (為專注基本功能穩定性)
- ⚠️ 音訊播放功能需要進一步測試

### v0.9.0 - 2024-12

**主要變更：**

- 從自動錄音模式改為按鈕控制模式
- 重構按鈕處理和狀態管理
- 新增 OLED 顯示支援

### v0.8.0 - 2024-11

**初始版本：**

- 基本 I2S 音訊錄製功能
- MQTT 音訊傳輸
- LED 狀態指示

## 🚀 未來規劃

### 計劃功能

- **🔄 MQTT 音訊傳輸**：重新啟用並優化 MQTT 功能
- **🌐 Web 管理介面**：透過瀏覽器管理系統設定
- **📱 行動應用**：Android/iOS 遠端控制應用
- **🔊 音訊播放**：支援多格式音訊檔案播放
- **☁️ 雲端整合**：支援 AWS IoT、Azure IoT 等雲端服務

### 技術優化

- **低功耗模式**：支援深度睡眠和喚醒機制
- **OTA 更新**：無線韌體更新功能
- **安全加密**：MQTT 和 WiFi 通訊加密
- **多語言支援**：國際化介面和語音提示

---

**專案版本**: v1.0.0  
**最後更新**: 2025-07-02  
**相容性**: ESP32-S3, PlatformIO, Arduino Framework  
**授權**: MIT License  
**維護者**: ProjectG 開發團隊
