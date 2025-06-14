# ESP32-S3 可用引腳與麥克風配置

## 🔌 ESP32-S3 N16R8 可用 GPIO 引腳

### 可安全使用的 GPIO 引腳

```
GPIO 0    - Boot 按鈕 (已被佔用)
GPIO 1-14 - 可用於一般 I/O
GPIO 15-16- 可用 (但 15,16,17 已用於音頻播放)
GPIO 17   - 已用於音頻播放
GPIO 18   - 可用 ✓ (麥克風 WS)
GPIO 19   - 不存在 ❌
GPIO 20   - 可用 ✓ (麥克風 SCK)
GPIO 21   - 可用 ✓ (麥克風 SD)
GPIO 35-45- 可用於一般 I/O
GPIO 46-48- 可用但需注意特殊功能
```

### 已被佔用的引腳

```
GPIO 0  - Boot 按鈕
GPIO 1  - 音量增加按鈕
GPIO 2  - 音量減少按鈕  
GPIO 8  - I2C SDA (OLED)
GPIO 9  - I2C SCL (OLED)
GPIO 15 - I2S 音頻播放 DOUT
GPIO 16 - I2S 音頻播放 BCLK
GPIO 17 - I2S 音頻播放 LRC
GPIO 42 - 麥克風控制按鈕
GPIO 48 - LED 控制 (NeoPixel)
```

## 🎙️ 修正後的麥克風接線配置

### 新的接線方案

```
ESP32-S3 引腳    I2S 麥克風模組    說明
─────────────────────────────────────────
GPIO18          WS (或 LR)      字選擇信號 (Word Select)
GPIO21          SD (或 DATA)    音頻數據輸入 (Serial Data)  
GPIO20          SCK (或 BCLK)   位時鐘信號 (Bit Clock)
3.3V            VDD (或 VCC)    電源正極
GND             GND             電源負極 & 接地
GND             L/R             左右聲道選擇 (接GND=左聲道)
```

### INMP441 接線圖 (修正版)

```
     INMP441                    ESP32-S3
   ┌─────────────┐            ┌─────────────┐
   │             │            │             │
   │ VDD     ────┼────────────┼──── 3.3V    │
   │ GND     ────┼────────────┼──── GND     │  
   │ L/R     ────┼────────────┼──── GND     │ (左聲道)
   │ WS      ────┼────────────┼──── GPIO18  │
   │ SCK     ────┼────────────┼──── GPIO20  │
   │ SD      ────┼────────────┼──── GPIO21  │ ← 修正
   │             │            │             │
   └─────────────┘            └─────────────┘
```

## 🔄 替代引腳方案

如果 GPIO18, 20, 21 有衝突，可以考慮以下替代方案：

### 方案 A (推薦)

```cpp
#define MIC_I2S_WS 18   // 字選擇信號
#define MIC_I2S_SD 21   // 音頻數據輸入
#define MIC_I2S_SCK 20  // 時鐘信號
```

### 方案 B (備用)

```cpp
#define MIC_I2S_WS 35   // 字選擇信號
#define MIC_I2S_SD 36   // 音頻數據輸入  
#define MIC_I2S_SCK 37  // 時鐘信號
```

### 方案 C (如果需要更多可用引腳)

```cpp
#define MIC_I2S_WS 10   // 字選擇信號
#define MIC_I2S_SD 11   // 音頻數據輸入
#define MIC_I2S_SCK 12  // 時鐘信號
```

## 📋 完整引腳分配表

| 功能 | ESP32-S3 引腳 | 連接對象 | 狀態 |
|------|---------------|----------|------|
| Boot按鈕 | GPIO0 | 系統 | 佔用 |
| 音量+ | GPIO1 | 按鈕 | 佔用 |
| 音量- | GPIO2 | 按鈕 | 佔用 |
| I2C SDA | GPIO8 | OLED | 佔用 |
| I2C SCL | GPIO9 | OLED | 佔用 |
| 音頻播放 | GPIO15-17 | I2S喇叭 | 佔用 |
| 麥克風WS | GPIO18 | 麥克風 | 新增 |
| 麥克風SCK | GPIO20 | 麥克風 | 新增 |
| 麥克風SD | GPIO21 | 麥克風 | 新增 |
| mic控制 | GPIO42 | 按鈕 | 新增 |
| LED | GPIO48 | NeoPixel | 佔用 |

## ⚠️ 注意事項

1. **GPIO19 不存在**: ESP32-S3 沒有 GPIO19，已改用 GPIO21
2. **引腳衝突檢查**: 確認新的引腳沒有與其他功能衝突
3. **電源供應**: 麥克風模組使用 3.3V 供電
4. **接地共用**: 所有 GND 必須連接在一起

## 🔧 如果需要修改引腳

如果您想使用不同的引腳，只需修改 `main.cpp` 中的定義：

```cpp
// 在 main.cpp 第 41-43 行修改
#define MIC_I2S_WS 18   // 改成您想要的引腳
#define MIC_I2S_SD 21   // 改成您想要的引腳  
#define MIC_I2S_SCK 20  // 改成您想要的引腳
```

修改後重新編譯上傳即可。有其他引腳需求嗎？
