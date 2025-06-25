# ESP32 MQTT 音訊傳遞與梅爾頻譜特徵提取系統

## 功能概述

本系統將原本的 WebSocket 音訊流功能升級為 MQTT 協議，提供更穩定、更標準化的音訊數據傳輸和梅爾頻譜特徵提取功能。

### 核心功能

- ✅ **MQTT 音訊傳遞**：透過 MQTT 協議實時傳送 16kHz 16位音訊數據
- ✅ **MFCC 特徵發布**：實時計算並發布 13 個 MFCC 係數
- ✅ **梅爾頻譜發布**：發布 26 個梅爾濾波器組能量
- ✅ **音訊特徵發布**：頻譜重心、帶寬、過零率、RMS 能量等
- ✅ **遠程控制**：透過 MQTT 控制音訊發布和特徵提取
- ✅ **狀態監控**：即時狀態發布和統計信息

### MQTT 主題結構

```
esp32/audio/raw        - 原始音訊數據 (JSON格式)
esp32/audio/mfcc       - MFCC 係數 (JSON格式)
esp32/audio/mel        - 梅爾能量 (JSON格式)
esp32/audio/features   - 其他音訊特徵 (JSON格式)
esp32/audio/status     - 系統狀態 (JSON格式)
esp32/audio/control    - 控制命令 (訂閱)
```

## 硬體配置

### I2S 麥克風接線

| 功能 | ESP32-S3 引腳 | 說明 |
|------|---------------|------|
| SCK (時鐘) | GPIO20 | I2S 時鐘信號 |
| WS (字選擇) | GPIO18 | 左右聲道選擇 |
| SD (數據) | GPIO21 | 音訊數據輸入 |
| VDD | 3.3V | 電源 |
| GND | GND | 接地 |

### 網路要求

- WiFi 連接 (2.4GHz)
- MQTT 服務器 (推薦使用 Mosquitto)
- 網路延遲 < 100ms (區域網絡最佳)

## 軟體架構

### 主要類別

#### AudioMqttManager

- **功能**：MQTT 音訊數據管理和發布
- **特色**：
  - 多任務並行處理
  - 自動重連機制
  - JSON 格式數據傳輸
  - 隊列緩衝管理
  - 錯誤統計和監控

#### AudioFeatureExtractor (保持不變)

- **功能**：實時音訊特徵提取
- **演算法**：FFT → 功率譜 → 梅爾濾波器 → MFCC
- **參數**：
  - FFT 大小：512
  - 梅爾濾波器數：26
  - MFCC 係數數：13
  - 視窗大小：400 樣本
  - 跳躍大小：160 樣本

### 任務架構

```
Core 0 (PRO_CPU):
├── MQTT Task (優先級 3) - MQTT 連接管理
├── Audio Control Task (優先級 3) - 音樂播放控制
└── LED Control Task (優先級 2) - LED 控制

Core 1 (APP_CPU):
├── Audio Publish Task (優先級 2) - 音訊數據發布
├── Feature Publish Task (優先級 2) - 特徵數據發布
├── Microphone Task (優先級 2) - 麥克風數據採集
├── Button Control Task (優先級 2) - 按鈕控制
└── OLED Update Task (優先級 1) - 顯示更新
```

## 配置設置

### MQTT 服務器設置 (main.cpp)

```cpp
// MQTT 設置
const char* mqttServer = "192.168.137.1";  // 您的 MQTT 服務器 IP
const int mqttPort = 1883;                 // MQTT 端口
const char* mqttUser = nullptr;            // 認證用戶名 (可選)
const char* mqttPassword = nullptr;        // 認證密碼 (可選)
```

### WiFi 設置 (main.cpp)

```cpp
// WiFi 設置
const char *ssid = "您的WiFi名稱";
const char *password = "您的WiFi密碼";
```

## 數據格式

### 1. 原始音訊數據 (`esp32/audio/raw`)

```json
{
    "timestamp": 1234567890,
    "sequence": 1,
    "length": 256,
    "audio": [1234, -567, 890, ...]
}
```

### 2. MFCC 係數 (`esp32/audio/mfcc`)

```json
{
    "timestamp": 1234567890,
    "mfcc": [12.34, -5.67, 8.90, ...]
}
```

### 3. 梅爾能量 (`esp32/audio/mel`)

```json
{
    "timestamp": 1234567890,
    "mel": [0.123, 0.456, 0.789, ...]
}
```

### 4. 音訊特徵 (`esp32/audio/features`)

```json
{
    "timestamp": 1234567890,
    "spectralCentroid": 1234.5,
    "spectralBandwidth": 567.8,
    "zeroCrossingRate": 0.123,
    "rmsEnergy": 45.6
}
```

### 5. 狀態信息 (`esp32/audio/status`)

```json
{
    "device": "ESP32_ProjectG",
    "status": "online",
    "publishing": true,
    "featureExtraction": true,
    "timestamp": 1234567890,
    "stats": {
        "audioPackets": 1000,
        "mfccPackets": 200,
        "melPackets": 200,
        "featurePackets": 200,
        "reconnects": 1,
        "errors": 0
    }
}
```

## 控制命令

### 發送到 `esp32/audio/control` 主題的 JSON 命令

#### 1. 開始音訊發布

```json
{
    "command": "startPublishing",
    "timestamp": 1234567890
}
```

#### 2. 停止音訊發布

```json
{
    "command": "stopPublishing",
    "timestamp": 1234567890
}
```

#### 3. 啟用特徵提取

```json
{
    "command": "enableFeatures",
    "timestamp": 1234567890
}
```

#### 4. 停用特徵提取

```json
{
    "command": "disableFeatures",
    "timestamp": 1234567890
}
```

#### 5. 獲取狀態

```json
{
    "command": "getStatus",
    "timestamp": 1234567890
}
```

## 使用方法

### 1. 設置 MQTT 服務器

**使用 Mosquitto (推薦)：**

```bash
# Windows
# 下載並安裝 Mosquitto from https://mosquitto.org/download/
mosquitto -v

# Ubuntu/Debian
sudo apt-get install mosquitto mosquitto-clients
sudo systemctl start mosquitto

# 測試
mosquitto_sub -h localhost -t "esp32/audio/#" -v
```

### 2. 配置 ESP32

1. 修改 `main.cpp` 中的 WiFi 和 MQTT 設置
2. 編譯並上傳程式到 ESP32
3. 監控串口輸出確認連接狀態

### 3. 使用 Python 客戶端

```bash
# 安裝依賴
pip install paho-mqtt numpy matplotlib

# 運行測試客戶端
python test_mqtt_audio_client.py
```

### 4. 手動測試 MQTT

```bash
# 訂閱所有音訊主題
mosquitto_sub -h YOUR_MQTT_SERVER -t "esp32/audio/#" -v

# 發送控制命令
mosquitto_pub -h YOUR_MQTT_SERVER -t "esp32/audio/control" \
  -m '{"command":"startPublishing","timestamp":1234567890}'

# 獲取狀態
mosquitto_pub -h YOUR_MQTT_SERVER -t "esp32/audio/control" \
  -m '{"command":"getStatus","timestamp":1234567890}'
```

## 性能調優

### 1. 發布頻率調整

```cpp
// 在 AudioMqttManager.h 中調整
#define AUDIO_PUBLISH_INTERVAL 100   // ms (10 fps)
#define FEATURE_PUBLISH_INTERVAL 200 // ms (5 fps)
```

### 2. 緩衝區大小調整

```cpp
// 在 AudioMqttManager.h 中調整
#define MQTT_BUFFER_SIZE 2048        // MQTT 緩衝區
#define AUDIO_BUFFER_SIZE 256        // 音訊包大小
```

### 3. 網路優化

- 使用有線乙太網絡 (如果可能)
- 設置 MQTT QoS = 0 (最快傳輸)
- 減少其他網路流量

## 監控和調試

### 1. ESP32 串口監控

```bash
pio device monitor --baud 115200
```

### 2. MQTT 流量監控

```bash
# 監控所有 ESP32 主題
mosquitto_sub -h YOUR_MQTT_SERVER -t "esp32/#" -v

# 統計消息頻率
mosquitto_sub -h YOUR_MQTT_SERVER -t "esp32/audio/raw" | wc -l
```

### 3. 性能指標

- **音訊包頻率**：約 10 包/秒 (每包 256 樣本)
- **MFCC 包頻率**：約 5 包/秒
- **網路帶寬**：約 50-100 KB/s (包含所有數據)
- **記憶體使用**：約 200-300 KB (動態分配)

## 故障排除

### 1. MQTT 連接問題

- 檢查網路連接
- 確認 MQTT 服務器 IP 和端口
- 檢查防火牆設置
- 驗證認證信息 (如果啟用)

### 2. 音訊數據問題

- 檢查麥克風接線
- 確認 I2S 配置正確
- 監控任務堆棧使用情況
- 檢查 FreeRTOS 任務優先級

### 3. 特徵提取問題

- 確認 FFT 庫正確安裝
- 檢查 PSRAM 使用情況
- 監控 CPU 使用率
- 驗證音訊數據質量

### 4. 常見錯誤代碼

- **MQTT 錯誤 -1**：網路連接失敗
- **MQTT 錯誤 -2**：連接被拒絕
- **MQTT 錯誤 -3**：服務器不可用
- **I2S 錯誤**：硬體配置或時序問題

## 擴展功能

### 1. 數據儲存

- 添加 InfluxDB 或 MongoDB 支援
- 實現音訊文件錄制功能
- 長期數據分析和統計

### 2. 進階處理

- 語音識別集成
- 噪聲抑制算法
- 音訊壓縮 (如 MP3/AAC)

### 3. 多設備支援

- 設備自動發現
- 多通道音訊混合
- 分散式音訊處理

## 相關文件

- `platformio.ini` - 項目配置和依賴
- `include/AudioMqttManager.h` - MQTT 音訊管理器頭文件
- `src/AudioMqttManager.cpp` - MQTT 音訊管理器實現
- `include/AudioFeatureExtractor.h` - 特徵提取器頭文件
- `src/AudioFeatureExtractor.cpp` - 特徵提取器實現
- `src/main.cpp` - 主程式
- `test_mqtt_audio_client.py` - Python 測試客戶端

## 版本歷史

- **v2.0** - MQTT 音訊傳遞功能
- **v1.0** - WebSocket 音訊流功能 (已廢棄)

---

**注意**：本系統已從 WebSocket 完全遷移到 MQTT 協議，提供更好的穩定性和標準化支援。
