#include "Arduino.h"
#include "WiFi.h"
#include "AudioPlayer.h"

#define I2S_DOUT 15
#define I2S_BCLK 16
#define I2S_LRC 17

AudioPlayer audioPlayer(I2S_BCLK, I2S_LRC, I2S_DOUT);
const char *ssid = "YunRog";
const char *password = "0937565253";

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP()); // Initialize audio player
    audioPlayer.begin();
    audioPlayer.setVolume(21); // 0...21
    audioPlayer.playURL("http://tangosl.4hotel.tw:8005/play.mp3");
}

void loop()
{
    audioPlayer.loop();
    if (Serial.available())
    {
        String r = Serial.readString();
        r.trim();
        if (r.length() > 5)
            audioPlayer.stopAndPlay(r.c_str());
        log_i("free heap=%i", ESP.getFreeHeap());
    }
}
