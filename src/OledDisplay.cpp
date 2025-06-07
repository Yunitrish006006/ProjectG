#include "OledDisplay.h"

OledDisplay::OledDisplay(uint8_t sda, uint8_t scl, uint8_t address)
    : _display(address, sda, scl), _sda(sda), _scl(scl), _address(address)
{
}

void OledDisplay::begin()
{
    // 初始化 I2C 和 OLED
    Wire.begin(_sda, _scl);
    _display.init();
    _display.flipScreenVertically();    // 根據需要翻轉屏幕
    _display.setFont(ArialMT_Plain_10); // 設置默認字體
    _display.setTextAlignment(TEXT_ALIGN_LEFT);

    // Startup animation
    _display.clear();

    // Draw animated boot screen
    for (int i = 0; i <= 100; i += 5)
    {
        _display.clear();
        _display.setFont(ArialMT_Plain_16);
        _display.setTextAlignment(TEXT_ALIGN_CENTER);
        _display.drawString(64, 0, "Project G");
        _display.setFont(ArialMT_Plain_10);
        _display.setTextAlignment(TEXT_ALIGN_CENTER);
        _display.drawString(64, 20, "LED Controller");

        // Progress bar
        drawProgressBar(14, 40, 100, 8, i);

        // Progress percentage
        _display.setTextAlignment(TEXT_ALIGN_CENTER);
        _display.drawString(64, 52, "Booting: " + String(i) + "%");

        _display.display();
        delay(10);
    }

    // Return to default text alignment
    _display.setTextAlignment(TEXT_ALIGN_LEFT);
    delay(300);
}

void OledDisplay::clear()
{
    _display.clear();
}

void OledDisplay::display()
{
    _display.display();
}

void OledDisplay::showText(const String &text, int x, int y, int size)
{
    switch (size)
    {
    case 1:
        _display.setFont(ArialMT_Plain_10);
        break;
    case 2:
        _display.setFont(ArialMT_Plain_16);
        break;
    case 3:
        _display.setFont(ArialMT_Plain_24);
        break;
    default:
        _display.setFont(ArialMT_Plain_10);
    }

    _display.drawString(x, y, text);
}

void OledDisplay::showTitle(const String &title)
{
    _display.setFont(ArialMT_Plain_16);
    _display.setTextAlignment(TEXT_ALIGN_CENTER);
    _display.drawString(64, 0, title);
    _display.setTextAlignment(TEXT_ALIGN_LEFT);
    _display.setFont(ArialMT_Plain_10);
}

void OledDisplay::showStatus(const String &status)
{
    // 在屏幕底部顯示狀態信息
    _display.setTextAlignment(TEXT_ALIGN_LEFT);
    _display.drawString(0, 54, status);
}

void OledDisplay::showLedColor(uint8_t r, uint8_t g, uint8_t b)
{
    // 顯示 RGB 值
    String colorText = "RGB: " + String(r) + "," + String(g) + "," + String(b);
    _display.drawString(0, 18, colorText);

    // 繪製顏色預覽
    drawRgbPreview(100, 18, 28, 10, r, g, b);
}

void OledDisplay::showLedBrightness(uint8_t brightness)
{
    String brightnessText = "Bright: " + String(brightness);
    _display.drawString(0, 30, brightnessText);

    // 繪製亮度條
    drawProgressBar(60, 30, 68, 10, (brightness * 100) / 255);
}

void OledDisplay::showLedMode(const String &mode)
{
    // 顯示當前模式
    String modeText = "Mode: " + mode;
    _display.drawString(0, 42, modeText);
}

void OledDisplay::showSystemInfo()
{
    // 顯示系統信息，如上電時間、WiFi狀態等
    String uptimeString = "Uptime: " + String(millis() / 1000) + "s";
    _display.drawString(0, 18, uptimeString);

    String memString = "Free RAM: " + String(ESP.getFreeHeap() / 1024) + "kB";
    _display.drawString(0, 30, memString);

    // 可以添加更多系統信息
}

void OledDisplay::showSimpleSystemInfo()
{
    // 清除屏幕並設置標題
    clear();
    showTitle("System Status");

    // 顯示上電時間
    String uptimeString = "Uptime: " + String(millis() / 1000) + "s";
    _display.drawString(0, 18, uptimeString);

    // 顯示可用內存
    String memString = "RAM: " + String(ESP.getFreeHeap() / 1024) + "kB";
    _display.drawString(0, 30, memString);

    // 顯示CPU頻率
    String cpuFreq = "CPU: " + String(ESP.getCpuFreqMHz()) + "MHz";
    _display.drawString(0, 42, cpuFreq);

    // 顯示日期和時間信息
    _display.drawString(0, 54, "Date: 2025-06-08");

    // 完成顯示
    display();
}

void OledDisplay::showWiFiStatus(bool connected, IPAddress ip)
{
    if (connected)
    {
        String ipStr = ip.toString();
        _display.drawString(0, 42, "WiFi: Connected");
        _display.drawString(0, 54, "IP: " + ipStr);
    }
    else
    {
        _display.drawString(0, 42, "WiFi: Disconnected");
        _display.drawString(0, 54, "IP: Not available");
    }
}

void OledDisplay::showNetworkSystemInfo(bool wifiConnected, IPAddress ip)
{
    // 清除屏幕並設置標題
    clear();
    showTitle("System Status");

    // 顯示上電時間
    String uptimeString = "Uptime: " + String(millis() / 1000) + "s";
    _display.drawString(0, 18, uptimeString);

    // 顯示可用內存
    String memString = "RAM: " + String(ESP.getFreeHeap() / 1024) + "kB";
    _display.drawString(0, 30, memString);

    // 顯示WiFi狀態和IP地址
    showWiFiStatus(wifiConnected, ip);

    // 完成顯示
    display();
}

void OledDisplay::showInfoPage(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, const String &mode)
{
    // Clear and set title
    clear();
    showTitle("System Info");

    // Draw a box around the RGB preview for better visibility
    int previewX = 95;
    int previewY = 18;
    int previewSize = 28;

    // Draw RGB color preview (larger)
    drawRgbPreview(previewX, previewY, previewSize, previewSize, r, g, b);

    // Display RGB values
    String rgbText = String(r) + "," + String(g) + "," + String(b);
    _display.drawString(previewX, previewY + previewSize + 2, rgbText);

    // System stats on left side
    _display.drawString(0, 18, "Mode: " + mode);
    _display.drawString(0, 30, "Bright: " + String(brightness));
    _display.drawString(0, 42, "Uptime: " + String(millis() / 1000) + "s");

    // Add info icon at bottom left
    drawIcon(0, 54, ICON_INFO);

    // Status at bottom - moved slightly to the right to make room for icon
    _display.drawString(20, 54, "RAM: " + String(ESP.getFreeHeap() / 1024) + "kB");
}

void OledDisplay::drawProgressBar(int x, int y, int width, int height, int progress)
{
    // 繪製進度條
    _display.drawRect(x, y, width, height);
    _display.fillRect(x + 1, y + 1, (width - 2) * progress / 100, height - 2);
}

void OledDisplay::drawRgbPreview(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
    // 繪製 RGB 顏色預覽
    // 由於OLED是單色的，我們用不同的填充模式來表示顏色

    // 繪製外框
    _display.drawRect(x, y, width, height);

    int rWidth = (width - 2) / 3;
    int gWidth = (width - 2) / 3;
    int bWidth = (width - 2) - rWidth - gWidth;

    // 紅色部分 - 根據紅色值填充
    int rFill = map(r, 0, 255, 0, rWidth);
    _display.fillRect(x + 1, y + 1, rFill, height - 2);

    // 綠色部分 - 根據綠色值填充
    int gFill = map(g, 0, 255, 0, gWidth);
    _display.fillRect(x + 1 + rWidth, y + 1, gFill, height - 2);

    // 藍色部分 - 根據藍色值填充
    int bFill = map(b, 0, 255, 0, bWidth);
    _display.fillRect(x + 1 + rWidth + gWidth, y + 1, bFill, height - 2);
}

void OledDisplay::drawIcon(int x, int y, int iconType)
{
    // Size of the icon
    const int size = 16;

    switch (iconType)
    {
    case ICON_BREATHING:
        // Breathing icon - wave pattern
        for (int i = 0; i < size; i++)
        {
            int height = (sin(i * 3.14159 / (size / 2)) + 1) * size / 4;
            _display.drawVerticalLine(x + i, y + size / 2 - height, height * 2);
        }
        break;

    case ICON_SOLID:
        // Solid icon - filled rectangle
        _display.fillRect(x, y, size, size);
        break;

    case ICON_GRADIENT:
        // Gradient icon - lines with varying spacing
        for (int i = 0; i < size; i += 2)
        {
            _display.drawHorizontalLine(x, y + i, size);
        }
        break;

    case ICON_INFO:
        // Info icon - letter 'i'
        _display.drawCircle(x + size / 2, y + size / 2, size / 2);
        _display.fillRect(x + size / 2 - 1, y + size / 3, 2, size / 2);
        _display.fillRect(x + size / 2 - 1, y + size / 6, 2, 2);
        break;

    case ICON_WIFI:
        // WiFi icon - Connected
        _display.drawCircle(x + size / 2, y + size / 2, size / 3);
        _display.drawCircle(x + size / 2, y + size / 2, size / 2);
        _display.fillCircle(x + size / 2, y + size / 2, 2);
        break;

    case ICON_WIFI_OFF:
        // WiFi icon - Disconnected
        _display.drawCircle(x + size / 2, y + size / 2, size / 3);
        _display.drawCircle(x + size / 2, y + size / 2, size / 2);
        _display.drawLine(x + 2, y + size - 2, x + size - 2, y + 2);
        break;

    default:
        // Default icon - empty rectangle
        _display.drawRect(x, y, size, size);
        break;
    }
}
