# Project G - LED 控制系統 with OLED 顯示

這個項目使用 ESP32-S3 N16R8 開發板實現 LED 控制系統，包括 RGB LED 控制和 OLED 顯示功能。

## 硬體需求

- ESP32-S3 N16R8 開發板
- SH1106 OLED 顯示模組 (I2C)
- RGB LED（共陰極或共陽極）
- WS2812B/NeoPixel LED（可選）
- 連接線和電阻

## 接線說明

### OLED 顯示模組 (SH1106)

- VCC -> 3.3V
- GND -> GND
- SCL -> GPIO9
- SDA -> GPIO8

### RGB LED（共陰極配置）

- R (紅色) -> GPIO35
- G (綠色) -> GPIO36
- B (藍色) -> GPIO37
- GND -> GND

### NeoPixel/WS2812B LED（可選）

- DIN -> GPIO48
- VCC -> 5V
- GND -> GND

## 功能說明

1. **LED 控制**
   - RGB 顏色控制
   - 亮度調節
   - 呼吸燈效果
   - 顏色漸變

2. **OLED 顯示**
   - 顯示當前 LED 狀態
   - 顯示 RGB 顏色值
   - 顯示亮度水平
   - 顯示系統資訊

## 使用說明

1. 啟動時，系統會自動測試 LED，顯示紅、綠、藍、白色和漸變效果，OLED 上會顯示當前測試狀態。
2. 主循環中，LED 會自動在不同顏色間呼吸效果變化，OLED 會即時顯示相關資訊。
3. 系統會隨機選擇不同的顏色和呼吸模式。

## 開發說明

此專案使用 PlatformIO 開發，基於 Arduino 框架。主要文件結構：

- `main.cpp`: 主程序
- `LedController.h/cpp`: LED 控制類
- `OledDisplay.h/cpp`: OLED 顯示控制類

## 注意事項

1. 如果使用共陽極 RGB LED，請在 `main.cpp` 中將 `ledController.setCommonAnode(false)` 改為 `ledController.setCommonAnode(true)`。
2. USB CDC 已啟用，可使用 USB 直接連接進行序列監控和程式上傳。
3. 驗證 ESP32-S3 的 GPIO 引腳功能是否與 LED 和 OLED 有衝突。
