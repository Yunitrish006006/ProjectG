# 免費 MQTT 服務器快速指南

## 🎯 **推薦的免費 MQTT 服務器**

### 1. **Eclipse Mosquitto (推薦)**

```cpp
const char *mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
```

- ✅ **最穩定**: Eclipse 基金會官方維護
- ✅ **無需註冊**: 直接使用
- ✅ **全球可用**: 24/7 可用性
- ⚠️ **公共服務器**: 數據不私密

### 2. **HiveMQ 公共服務器**

```cpp
const char *mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
```

- ✅ **高性能**: 工業級服務器
- ✅ **穩定可靠**: 商業級支持
- ⚠️ **連接時間限制**: 適合測試使用

### 3. **EMQX 公共服務器**

```cpp
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
```

- ✅ **多協議支持**: 支持 MQTT, WebSocket 等
- ✅ **良好性能**: 低延遲
- ⚠️ **使用限制**: 適合開發測試

## 🔧 **設置步驟**

### 1. 修改 ESP32 程式碼

在 `src/main.cpp` 中已經為您修改好了：

```cpp
// MQTT 設置 - 使用免費公共服務器
const char *mqttServer = "test.mosquitto.org";  // Eclipse Mosquitto
const int mqttPort = 1883;
const char *mqttUser = nullptr;     // 無需認證
const char *mqttPassword = nullptr; // 無需認證
```

### 2. 編譯並上傳

```bash
cd "d:\workspace\study\ProjectG"
pio run --target upload
pio device monitor
```

### 3. 測試 MQTT 服務器

運行服務器測試腳本：

```bash
python test_mqtt_servers.py
```

### 4. 測試音訊功能

運行音訊測試客戶端：

```bash
python simple_mqtt_test.py
```

## 📊 **性能比較**

| 服務器 | 穩定性 | 速度 | 功能 | 推薦度 |
|--------|--------|------|------|--------|
| Eclipse Mosquitto | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | 🏆 **最推薦** |
| HiveMQ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| EMQX | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

## 🚀 **快速驗證**

### 使用 MQTT 客戶端工具測試

```bash
# 安裝 mosquitto 客戶端 (Windows)
# 下載: https://mosquitto.org/download/

# 訂閱測試
mosquitto_sub -h test.mosquitto.org -t "test/topic" -v

# 發布測試 (另一個終端)
mosquitto_pub -h test.mosquitto.org -t "test/topic" -m "Hello MQTT!"
```

### 使用網頁工具測試

- **HiveMQ WebSocket 客戶端**: <http://www.hivemq.com/demos/websocket-client/>
- **EMQX 線上客戶端**: <https://mqttx.app/web>

## ⚠️ **注意事項**

### 公共服務器限制

1. **數據不私密**: 任何人都能訂閱您的主題
2. **無服務保證**: 可能偶爾不可用
3. **連接限制**: 可能有同時連接數限制
4. **頻率限制**: 過於頻繁的發布可能被限制

### 主題命名建議

使用唯一前綴避免衝突：

```cpp
// 不推薦
esp32/audio/raw

// 推薦 (使用設備 ID 或隨機字符串)
esp32_projectg_abc123/audio/raw
your_unique_prefix/audio/raw
```

### 生產環境建議

對於正式項目，建議：

1. **自建 MQTT 服務器**: 使用 Mosquitto, EMQX 等
2. **雲端 MQTT 服務**: AWS IoT, Azure IoT Hub, Google Cloud IoT
3. **商業 MQTT 服務**: HiveMQ Cloud, CloudMQTT 等

## 🔧 **本地 MQTT 服務器安裝 (可選)**

### Windows

```bash
# 下載並安裝 Mosquitto
# https://mosquitto.org/download/

# 啟動服務 (命令提示符以管理員身份運行)
net start mosquitto

# 或手動啟動
mosquitto -v
```

### Linux/macOS

```bash
# Ubuntu/Debian
sudo apt-get install mosquitto mosquitto-clients

# macOS
brew install mosquitto

# 啟動服務
sudo systemctl start mosquitto
# 或
mosquitto -v
```

使用本地服務器時，將 IP 改為您的電腦 IP：

```cpp
const char *mqttServer = "192.168.1.100";  // 您的電腦 IP
```

## 📞 **技術支持**

如果遇到連接問題：

1. 檢查網絡連接
2. 確認防火牆設置
3. 嘗試不同的服務器
4. 檢查 ESP32 WiFi 連接
5. 查看串口監控輸出

Happy coding! 🎉
