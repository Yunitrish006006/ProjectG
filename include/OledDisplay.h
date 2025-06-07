#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <SH1106Wire.h> // ThingPulse OLED SH1106 library

// Define modes for icon display
#define ICON_BREATHING 0
#define ICON_SOLID 1
#define ICON_GRADIENT 2
#define ICON_INFO 3

class OledDisplay
{
public:
    // 構造函數
    OledDisplay(uint8_t sda = 21, uint8_t scl = 22, uint8_t address = 0x3C);

    // 初始化顯示
    void begin();

    // 顯示信息
    void clear();
    void display();
    void showText(const String &text, int x = 0, int y = 0, int size = 1);
    void showTitle(const String &title);
    void showStatus(const String &status);

    // 顯示 LED 相關信息
    void showLedColor(uint8_t r, uint8_t g, uint8_t b);
    void showLedBrightness(uint8_t brightness);
    void showLedMode(const String &mode); // 顯示系統信息
    void showSystemInfo();

    // 顯示簡單的系統狀態信息 (只顯示系統相關信息)
    void showSimpleSystemInfo();

    // 顯示綜合信息頁面
    void showInfoPage(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, const String &mode);
    // 顯示圖形
    void drawProgressBar(int x, int y, int width, int height, int progress);
    void drawRgbPreview(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b);
    void drawIcon(int x, int y, int iconType);

private:
    SH1106Wire _display; // SH1106 OLED 顯示實例
    uint8_t _sda;
    uint8_t _scl;
    uint8_t _address;
};

#endif // OLED_DISPLAY_H
