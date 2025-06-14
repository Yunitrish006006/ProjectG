# 音量控制按鈕測試說明

## 🔧 硬體連接

```
ESP32-S3        按鈕        說明
GPIO1      ->   按鈕1   ->  GND    (音量增加)
GPIO2      ->   按鈕2   ->  GND    (音量減少)
GPIO42     ->   按鈕3   ->  GND    (麥克風開關)
```

## 🎮 功能測試

### 音量控制測試

1. **音量增加測試**
   - 按下連接到 GPIO1 的按鈕
   - 串口應顯示: `Volume UP button pressed`
   - 然後顯示: `Volume UP: [當前音量]`
   - OLED 顯示的音量數字應該增加

2. **音量減少測試**
   - 按下連接到 GPIO2 的按鈕
   - 串口應顯示: `Volume DOWN button pressed`
   - 然後顯示: `Volume DOWN: [當前音量]`
   - OLED 顯示的音量數字應該減少

### 麥克風控制測試

3. **麥克風開關測試**
   - 按下連接到 GPIO42 的按鈕
   - 串口應顯示: `Microphone control button pressed`
   - 然後顯示: `✓ 麥克風已啟動` 或 `✓ 麥克風已停止`
   - OLED 顯示應在 `Mic: ON` 和 `Mic: OFF` 間切換

## 🔍 調試步驟

如果音量控制不正常，請檢查：

1. **硬體連接**

   ```
   確認按鈕正確連接:
   - 按鈕一端連接到對應 GPIO
   - 按鈕另一端連接到 GND
   - 使用內部上拉電阻，按下時為 LOW 狀態
   ```

2. **串口監控**

   ```bash
   # 使用 PlatformIO 監控串口
   pio device monitor
   
   # 或使用其他串口工具
   # 波特率: 115200
   ```

3. **預期的串口輸出**

   ```
   Volume control and microphone buttons initialized
   ...
   Volume UP button pressed      # 按下 GPIO1
   Volume UP: 13                # 音量增加
   Volume DOWN button pressed    # 按下 GPIO2  
   Volume DOWN: 12              # 音量減少
   ```

## ⚡ 防抖動機制

- 按鈕防抖動延遲: 200ms
- 快速連按不會生效，需要間隔 200ms 以上

## 📱 OLED 顯示格式

```
System Status
Uptime: 123s
RAM: 234kB
WiFi: Connected      [WiFi圖標]
Audio: Playing Vol:12  Mic: OFF
```

音量範圍: 0-21 (0=靜音, 21=最大音量)

## 🐛 常見問題

1. **按鈕無響應**
   - 檢查 GPIO 連接
   - 確認按鈕接地正確
   - 檢查按鈕是否損壞

2. **音量不變**
   - 檢查串口是否有按鈕事件輸出
   - 確認 AudioPlayer 是否正常工作
   - 檢查音量是否已達到上下限 (0-21)

3. **OLED 顯示不更新**
   - OLED 更新間隔為 1 秒
   - 等待片刻觀察變化

請按照以上步驟測試，如果還有問題請告知具體的現象！
