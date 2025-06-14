# Project G - LED 控制系統 with OLED 顯示

這個項目使用 ESP32-S3 N16R8 開發板實現 LED 控制系統，包括 RGB LED 控制和 OLED 顯示功能。

## 硬體需求

- ESP32-S3 N16R8 開發板 [開發版連結](https://www.taiwansensor.com.tw/product/esp32-s3-devkitc-1-%E9%96%8B%E7%99%BC%E6%9D%BF-%E6%A8%82%E9%91%AB-wroom-1-n16r8-%E5%B7%B2%E7%84%8A%E6%8E%A5%E9%87%9D%E8%85%B3/?srsltid=AfmBOopRlAkA0Pqg56TTQj8SVzKgSCUIz-w4B6i-VNpxVR8ESW0LrRBx)
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
   - 顯示系統資訊 (每秒更新)
   - 顯示WiFi連接狀態和圖標
   - 顯示可用記憶體和運行時間
   - 顯示當前時間 (透過NTP同步)

3. **WiFi 連接**
   - 使用硬編碼的WiFi憑證自動連接
   - 連接狀態顯示在OLED上
   - 斷線自動重連功能
   - NTP時間同步

## 使用說明

1. 啟動時，系統會自動測試 LED，顯示紅、綠、藍、白色和漸變效果。
2. 同時系統會自動連接到WiFi網絡（使用硬編碼的憑證）並與NTP服務器同步時間。
3. OLED顯示屏會每秒更新一次，顯示系統資訊、WiFi狀態和當前時間。
4. LED會自動在不同顏色間以呼吸效果變化，系統會隨機選擇不同的顏色和呼吸模式。
5. 如需更改WiFi設置，請直接修改代碼中的SSID和密碼常量。

## 開發說明

此專案使用 PlatformIO 開發，基於 Arduino 框架。主要文件結構：

- `main.cpp`: 主程序
- `LedController.h/cpp`: LED 控制類
- `OledDisplay.h/cpp`: OLED 顯示控制類

## 注意事項

1. 如果使用共陽極 RGB LED，請在 `main.cpp` 中將 `ledController.setCommonAnode(false)` 改為 `ledController.setCommonAnode(true)`。
2. USB CDC 已啟用，可使用 USB 直接連接進行序列監控和程式上傳。
3. 驗證 ESP32-S3 的 GPIO 引腳功能是否與 LED 和 OLED 有衝突。
4. WiFi憑證直接硬編碼在程式中，不使用EEPROM或配置門戶。若需更改，請直接修改程式碼。
5. 系統使用FreeRTOS進行任務管理，所有延遲都使用vTaskDelay而不是blocking delay。
