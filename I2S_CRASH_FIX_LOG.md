# I2S 崩潰問題修復日誌

## 問題描述

系統在對講模式切換期間出現崩潰，主要錯誤：

1. `assert failed: spinlock_acquire` - I2S 資源衝突
2. `LoadProhibited` 錯誤 - 訪問已釋放的 I2S 資源
3. `xQueueReceive` 失敗 - 隊列未初始化就被使用

## 根本原因分析

1. **I2S 資源競爭**: `audioPlayer.loop()` 和 `microphoneTask` 同時嘗試使用 I2S_NUM_0
2. **狀態不同步**: `micToSpeakerMode` 狀態切換期間，任務間狀態不一致
3. **初始化順序問題**: 隊列在創建前就被調用
4. **資源釋放時機**: `stopMicrophone()` 釋放資源時，其他任務仍在使用

## 修復方案

### 1. 添加 I2S 互斥鎖保護

- 新增 `i2sMutex` 保護 I2S 資源切換
- `startMicrophone()` 和 `stopMicrophone()` 使用互斥鎖
- `microphoneTask` 中的 `i2s_write` 操作受互斥鎖保護

### 2. 改進狀態管理

- 在 `stopMicrophone()` 中優先設置狀態變量
- 添加狀態一致性檢查機制
- 雙重檢查避免競爭條件

### 3. 修復初始化順序

- 隊列創建後才調用 `runLedTestSequence()`
- 添加隊列檢查，避免空指針訪問
- LED 任務等待隊列初始化完成

### 4. 改進錯誤處理

- 減少錯誤報告頻率，避免串口溢出
- 添加超時機制，防止死鎖
- 異常狀態下自動恢復

## 最新修復 (LoadProhibited 錯誤)

### 問題描述

在 `stopMicrophone()` 函數中調用 `audioPlayer.begin()` 時出現 LoadProhibited 錯誤，具體在 `i2s_set_pin` 函數中崩潰。

### 根本原因

AudioPlayer 嘗試重新初始化 I2S_NUM_0 時，該資源還沒有完全從硬體層面釋放，導致訪問無效的記憶體地址。

### 修復方案

#### 1. 新增完全清理函數

```cpp
esp_err_t cleanupI2SResources() {
    // 停止所有 I2S 端口
    // 卸載所有 I2S 驅動
    // 確保硬體完全重置
}
```

#### 2. AudioPlayer 安全初始化

```cpp
bool AudioPlayer::beginSafe(int maxRetries) {
    // 多次嘗試初始化
    // 每次嘗試間增加延遲
    // 返回初始化結果
}
```

#### 3. 改進資源釋放時序

- 優先設置狀態變量
- 使用統一的清理函數
- 增加硬體重置等待時間
- 使用安全初始化方法

### 程式碼修改

- `cleanupI2SResources()`: 統一的 I2S 資源清理
- `AudioPlayer::beginSafe()`: 帶重試機制的安全初始化
- `stopMicrophone()`: 改進的資源管理流程

## 程式碼修改重點

### startMicrophone()

```cpp
if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // 安全的資源切換
}
```

### stopMicrophone()

```cpp
// 先設置狀態變量，防止其他任務繼續使用
micToSpeakerMode = false;
micEnabled = false;
vTaskDelay(pdMS_TO_TICKS(50)); // 讓其他任務檢查狀態變化
```

### microphoneTask()

```cpp
if (micToSpeakerMode && micEnabled) {
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        // 雙重檢查狀態
        if (micToSpeakerMode && micEnabled) {
            // 安全的 I2S 寫入
        }
        xSemaphoreGive(i2sMutex);
    }
}
```

### 隊列初始化

```cpp
// 檢查隊列創建是否成功
if (ledCommandQueue == NULL || volumeCommandQueue == NULL || ledMutex == NULL || i2sMutex == NULL) {
    Serial.println("ERROR: 隊列或互斥鎖創建失敗!");
    while(1) { vTaskDelay(1000 / portTICK_PERIOD_MS); }
}
```

## 測試建議

1. 反覆切換對講模式，確認不再崩潰
2. 檢查音樂播放和對講功能正常
3. 驗證 OLED 顯示狀態正確
4. 測試音量控制在各模式下正常工作

## 預期效果

- ✅ 消除 I2S 資源衝突導致的崩潰
- ✅ 狀態切換更穩定可靠
- ✅ 系統初始化更安全
- ✅ 錯誤處理更完善
