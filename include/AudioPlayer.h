#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "Arduino.h"
#include "Audio.h"

class AudioPlayer
{
private:
    Audio audio;
    int i2s_bclk;
    int i2s_lrc;
    int i2s_dout;
    int volume;

public:
    // Constructor
    AudioPlayer(int bclk = 16, int lrc = 17, int dout = 15);

    // Initialize audio system
    void begin();

    // Volume control
    void setVolume(int vol);
    int getVolume(); // Playback control
    void playURL(const char *url);
    void stop();
    void stopAndPlay(const char *url);
    void loop();

    // Audio callbacks (static functions)
    static void onInfo(const char *info);
    static void onId3Data(const char *info);
    static void onEofMp3(const char *info);
    static void onShowStation(const char *info);
    static void onShowStreamTitle(const char *info);
    static void onBitrate(const char *info);
    static void onCommercial(const char *info);
    static void onIcyUrl(const char *info);
    static void onLastHost(const char *info);
};

#endif
