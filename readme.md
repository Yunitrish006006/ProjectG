# ESP32-S3 ProjectG - MQTT 音訊傳遞與特徵提取系統

## 📖 專案概述

本系統是基於 ESP32-S3 N16R8 開發板的音訊處理專案，主要功能包括 MQTT 音訊傳遞、梅爾頻譜特徵提取、LED 控制和 OLED 顯示。系統採用穩定的 MQTT 協議傳遞音訊數據和特徵，支援實時音訊分析。

### 🎯 核心功能

- ✅ **MQTT 音訊傳遞**：透過 MQTT 協議實時傳送 16kHz 16位音訊數據
- ✅ **特徵提取**：實時計算 MFCC、梅爾能量、頻譜重心等音訊特徵
- ✅ **LED 控制系統**：RGB LED 和 NeoPixel 控制，包含各種燈效
- ✅ **OLED 顯示**：實時顯示系統狀態、WiFi 連接和統計信息
- ✅ **按鈕控制**：音量控制和麥克風開關
- ✅ **多任務處理**：基於 FreeRTOS 的並行處理架構

## 🔌 硬體配置與接線

### ESP32-S3 N16R8 開發板

推薦購買：[ESP32-S3 N16R8 開發板](https://www.taiwansensor.com.tw/product/esp32-s3-devkitc-1-%E9%96%8B%E7%99%BC%E6%9D%BF-%E6%A8%82%E9%91%AB-wroom-1-n16r8-%E5%B7%B2%E7%84%8A%E6%8E%A5%E9%87%9D%E8%85%B3/?srsltid=AfmBOopRlAkA0Pqg56TTQj8SVzKgSCUIz-w4B6i-VNpxVR8ESW0LrRBx)

### I2S 麥克風接線 (INMP441 推薦)

| 功能 | ESP32-S3 引腳 | I2S 麥克風 | 說明 |
|------|---------------|------------|------|
| 時鐘信號 | GPIO20 | SCK (BCLK) | I2S 位時鐘 |
| 字選擇 | GPIO18 | WS (LR) | 左右聲道選擇 |
| 數據輸入 | GPIO21 | SD (DATA) | 音訊數據 |
| 電源 | 3.3V | VDD (VCC) | 電源正極 |
| 接地 | GND | GND | 電源負極 |
| 聲道選擇 | GND | L/R | 左聲道選擇 |

```
     INMP441                    ESP32-S3
   ┌─────────────┐            ┌─────────────┐
   │ VDD     ────┼────────────┼──── 3.3V    │
   │ GND     ────┼────────────┼──── GND     │  
   │ L/R     ────┼────────────┼──── GND     │
   │ WS      ────┼────────────┼──── GPIO18  │
   │ SCK     ────┼────────────┼──── GPIO20  │
   │ SD      ────┼────────────┼──── GPIO21  │
   └─────────────┘            └─────────────┘
```

### OLED 顯示模組 (SH1106/SSD1306)

| 功能 | ESP32-S3 引腳 | OLED 模組 |
|------|---------------|-----------|
| 電源 | 3.3V | VCC |
| 接地 | GND | GND |
| I2C 時鐘 | GPIO9 | SCL |
| I2C 數據 | GPIO8 | SDA |

### LED 控制系統

#### RGB LED (共陰極)

| 顏色 | ESP32-S3 引腳 |
|------|---------------|
| 紅色 | GPIO35 |
| 綠色 | GPIO36 |
| 藍色 | GPIO37 |

#### NeoPixel/WS2812B LED

| 功能 | ESP32-S3 引腳 |
|------|---------------|
| 數據輸入 | GPIO48 |
| 電源 | 5V |
| 接地 | GND |

### 按鈕控制

| 功能 | ESP32-S3 引腳 | 接線方式 |
|------|---------------|----------|
| 音量增加 | GPIO1 | 按鈕 -> GND |
| 音量減少 | GPIO2 | 按鈕 -> GND |
| 麥克風控制 | GPIO42 | 按鈕 -> GND |

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

---

**專案版本**: 2.0 MQTT  
**最後更新**: 2024年12月  
**相容性**: ESP32-S3, PlatformIO, Arduino Framework  
**授權**: MIT License
