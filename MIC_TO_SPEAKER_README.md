# ESP32 麥克風串口音頻傳輸系統

## 🎯 功能說明

這個系統可以將 ESP32 連接的 I2S 麥克風收集的音頻數據，通過串口實時傳輸到電腦，並在電腦上播放到喇叭。

## 📁 文件說明

### ESP32 端程序

- **`simple_mic_serial.cpp`** - 簡潔版麥克風串口傳輸
- **`mic_to_serial.cpp`** - 完整版，包含音頻電平顯示

### 電腦端程序

- **`audio_receiver.py`** - Python 音頻接收和播放程序

## 🔌 硬體連接

### I2S 麥克風連接

```
ESP32        I2S 麥克風
GPIO15   ->  WS (字選擇)
GPIO13   ->  SD (數據)
GPIO2    ->  SCK (時鐘)
3.3V     ->  VCC
GND      ->  GND
```

### 推薦的 I2S 麥克風模組

- INMP441
- MAX4466 (需要轉換)
- SPH0645

## 🚀 使用步驟

### 1. ESP32 端設置

```cpp
// 在 platformio.ini 中使用其中一個程序
// 方法1: 使用簡潔版
src_filter = +<simple_mic_serial.cpp>

// 方法2: 使用完整版
src_filter = +<mic_to_serial.cpp>
```

### 2. 電腦端設置

#### 安裝 Python 依賴

```bash
pip install pyaudio pyserial numpy
```

#### 修改串口設置

在 `audio_receiver.py` 中修改串口號:

```python
SERIAL_PORT = 'COM3'  # Windows
# SERIAL_PORT = '/dev/ttyUSB0'  # Linux
# SERIAL_PORT = '/dev/cu.usbserial-*'  # macOS
```

### 3. 運行系統

1. **上傳 ESP32 程序**

   ```bash
   pio run -t upload
   ```

2. **運行電腦端接收程序**

   ```bash
   python audio_receiver.py
   ```

## ⚙️ 音頻參數

| 參數 | 數值 | 說明 |
|------|------|------|
| 取樣率 | 16kHz | 適合語音傳輸 |
| 位元深度 | 16位元 | 平衡品質與數據量 |
| 聲道 | 單聲道 | 減少數據傳輸量 |
| 緩衝區 | 128樣本 | 低延遲設計 |

## 🛠️ 故障排除

### 常見問題

1. **無聲音輸出**
   - 檢查 I2S 麥克風連接
   - 確認串口號正確
   - 檢查麥克風是否工作

2. **音頻斷續**
   - 降低取樣率 (如改為 8kHz)
   - 增加緩衝區大小
   - 檢查串口連接穩定性

3. **串口連接失敗**
   - 確認 ESP32 已連接
   - 檢查串口號和波特率
   - 關閉其他佔用串口的程序

### 調試技巧

1. **檢查麥克風數據**

   ```cpp
   // 在 ESP32 端添加調試輸出
   Serial.println("Audio level: " + String(avgLevel));
   ```

2. **監控串口數據**

   ```bash
   # 使用串口監視器查看原始數據
   pio device monitor
   ```

## 🎛️ 自定義配置

### 調整音頻品質

```cpp
// 提高品質（增加數據量）
#define SAMPLE_RATE 22050
#define SAMPLE_BITS 24

// 降低數據量（減少品質）
#define SAMPLE_RATE 8000
#define SAMPLE_BITS 8
```

### 調整緩衝區大小

```cpp
// 更大緩衝區 = 更穩定，但延遲更高
const int BUFFER_SIZE = 512;

// 更小緩衝區 = 低延遲，但可能不穩定
const int BUFFER_SIZE = 64;
```

## 📈 性能優化

1. **減少串口中斷**: 使用較大的緩衝區
2. **優化音頻處理**: 使用固定點運算代替浮點運算
3. **降低取樣率**: 根據應用需求調整
4. **使用硬體緩衝**: 增加 DMA 緩衝區數量

## 🔄 擴展功能

### 可能的改進方向

- 添加音頻壓縮 (如 ADPCM)
- 實現音頻錄製功能
- 添加音頻濾波器
- 支援雙向音頻通訊
- 整合到現有的 ProjectG 系統中

## 📝 技術說明

### 數據流程

```
I2S麥克風 -> ESP32 I2S -> 串口 -> 電腦 -> 音頻輸出設備
```

### 數據格式

- **ESP32輸出**: 16位元 PCM 數據，小端序
- **電腦接收**: 直接送入音頻API播放

這個系統提供了一個簡單而有效的方式，將 ESP32 變成一個無線麥克風，通過串口將音頻傳輸到電腦進行播放。
