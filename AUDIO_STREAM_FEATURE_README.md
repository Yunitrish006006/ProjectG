# ESP32 音訊流和梅爾頻譜特徵提取系統

## 功能概述

本系統為 ESP32-S3 添加了完整的音訊流傳遞和實時梅爾頻譜特徵提取功能，主要包括：

### 核心功能

- ✅ **實時音訊捕獲**：從 I2S 麥克風捕獲 16kHz 16位音訊
- ✅ **音訊流傳遞**：透過 WebSocket 實時傳送音訊數據
- ✅ **梅爾頻譜特徵提取**：實時計算 MFCC、梅爾能量等特徵
- ✅ **Web 界面**：提供 HTML5 客戶端進行測試和監控
- ✅ **多客戶端支援**：最多支援 4 個同時連接的客戶端

### 特徵提取項目

- **MFCC 係數** (13個)：語音識別常用特徵
- **梅爾濾波器組能量** (26個)：梅爾刻度的頻譜能量
- **頻譜重心**：頻譜的"重心"位置
- **頻譜帶寬**：頻譜能量的分散程度
- **過零率**：音訊信號的過零頻率
- **RMS 能量**：音訊信號的均方根能量

## 硬體配置

### I2S 麥克風接線

| 功能 | ESP32-S3 引腳 | 說明 |
|------|---------------|------|
| SCK (時鐘) | GPIO20 | I2S 時鐘信號 |
| WS (字選擇) | GPIO18 | 左右聲道選擇 |
| SD (數據) | GPIO21 | 音訊數據輸入 |
| VDD | 3.3V | 電源 |
| GND | GND | 接地 |

### I2S 音訊輸出接線 (對講模式)

| 功能 | ESP32-S3 引腳 | 說明 |
|------|---------------|------|
| BCLK | GPIO16 | 位時鐘 |
| LRC | GPIO17 | 左右聲道時鐘 |
| DIN | GPIO15 | 音訊數據輸出 |

## 軟體架構

### 主要類別

#### AudioFeatureExtractor

- **功能**：實時音訊特徵提取
- **演算法**：FFT → 功率譜 → 梅爾濾波器 → MFCC
- **參數**：
  - FFT 大小：512
  - 梅爾濾波器數：26
  - MFCC 係數數：13
  - 視窗大小：400 樣本
  - 跳躍大小：160 樣本

#### AudioStreamManager

- **功能**：WebSocket 音訊流傳遞和客戶端管理
- **特色**：
  - 非阻塞音訊緩衝
  - 多客戶端管理
  - JSON 格式數據傳輸
  - 自動重連機制

### 任務架構

```
Core 0 (PRO_CPU):
├── Audio Control Task (優先級 3)
├── LED Control Task (優先級 2)
└── Stream Task (優先級 3)

Core 1 (APP_CPU):
├── Microphone Task (優先級 2)
├── Feature Task (優先級 2)
├── Button Control Task (優先級 2)
└── OLED Update Task (優先級 1)
```

## 使用方法

### 1. 基本設置

```cpp
// 在 main.cpp 中已自動初始化
AudioStreamManager audioStreamManager;
AudioFeatureExtractor featureExtractor;
```

### 2. Web 客戶端連接

- 開啟瀏覽器訪問：`http://[ESP32_IP]:82`
- WebSocket 端點：`ws://[ESP32_IP]:81`

### 3. WebSocket API

#### 訂閱音訊數據

```json
{
    "command": "subscribe",
    "dataType": "audio"
}
```

#### 訂閱特徵數據

```json
{
    "command": "subscribe",
    "dataType": "features"
}
```

#### 取消訂閱

```json
{
    "command": "unsubscribe",
    "dataType": "audio"
}
```

### 4. 數據格式

#### 音訊數據包

```json
{
    "type": "audio",
    "timestamp": 12345,
    "sequence": 100,
    "length": 256,
    "data": [sample1, sample2, ...]
}
```

#### 特徵數據包

```json
{
    "type": "features",
    "timestamp": 12345,
    "mfcc": [coeff1, coeff2, ...],
    "mel": [energy1, energy2, ...],
    "spectralCentroid": 1500.5,
    "spectralBandwidth": 800.2,
    "zeroCrossingRate": 0.123,
    "rmsEnergy": 0.456
}
```

## 控制命令

### 按鈕控制

- **GPIO1**: 音量增加
- **GPIO2**: 音量減少  
- **GPIO42**: 切換麥克風模式 (收音/對講)

### 串口命令

透過串口監視器輸入以下命令：

#### 音訊流控制

```
stream:start    # 啟動音訊流
stream:stop     # 停止音訊流
stream:status   # 查看狀態
```

#### 特徵提取控制

```
feature:enable  # 啟用特徵提取
feature:disable # 停用特徵提取
feature:print   # 列印當前特徵
```

## 性能參數

### 系統規格

- **採樣率**：16 kHz
- **位深度**：16 位
- **聲道數**：單聲道 (左聲道)
- **延遲**：< 100ms
- **記憶體使用**：約 150KB

### 處理能力

- **音訊緩衝**：1024 樣本
- **特徵更新頻率**：10 Hz
- **FFT 處理時間**：< 20ms
- **WebSocket 傳輸**：< 50ms

## 應用場景

### 1. 語音識別預處理

```javascript
// 客戶端 JavaScript 範例
ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    if (data.type === 'features') {
        // 使用 MFCC 係數進行語音識別
        processVoiceRecognition(data.mfcc);
    }
};
```

### 2. 音樂分析

```javascript
// 分析音樂特徵
if (data.spectralCentroid > 2000) {
    console.log("高頻音樂內容");
} else if (data.spectralCentroid < 500) {
    console.log("低頻音樂內容");
}
```

### 3. 環境音監測

```javascript
// 環境噪音檢測
if (data.rmsEnergy > 0.5) {
    console.log("高音量環境");
} else if (data.zeroCrossingRate > 0.1) {
    console.log("噪音環境");
}
```

## 故障排除

### 常見問題

#### 1. 音訊流無法啟動

- 檢查 WiFi 連接狀態
- 確認 I2S 麥克風接線正確
- 查看串口日誌錯誤信息

#### 2. 特徵提取異常

- 確認 FFT 記憶體分配成功
- 檢查音訊輸入是否正常
- 降低特徵更新頻率

#### 3. WebSocket 連接失敗

- 確認防火牆設置
- 檢查端口是否被占用
- 嘗試重啟 ESP32

### 調試命令

```cpp
// 在串口監視器中輸入
audioStreamManager.printStatus();  // 查看狀態
featureExtractor.printFeatures();  // 列印特徵
```

## 擴展功能

### 自定義特徵提取

```cpp
// 添加自定義特徵
double customFeature = computeCustomFeature(audioBuffer);
featurePacket.customValue = customFeature;
```

### 添加更多客戶端

```cpp
// 在 AudioStreamManager.h 中修改
#define MAX_CLIENTS 8  // 增加到 8 個客戶端
```

## 技術參考

### 相關論文

- Davis & Mermelstein (1980): "Comparison of parametric representations for monosyllabic word recognition"
- Rabiner & Juang (1993): "Fundamentals of speech recognition"

### 開源庫

- ArduinoFFT: 快速傅立葉變換
- WebSockets: WebSocket 通信
- ESPAsyncWebServer: 異步 Web 服務器

## 更新日誌

### v1.0.0 (2025-06-15)

- ✅ 初始版本發布
- ✅ 基本音訊流傳遞功能
- ✅ MFCC 和梅爾頻譜特徵提取
- ✅ WebSocket API
- ✅ 多客戶端支援
- ✅ Web 測試界面
