#include "AudioPlayer.h"

AudioPlayer::AudioPlayer(int bclk, int lrc, int dout)
{
    i2s_bclk = bclk;
    i2s_lrc = lrc;
    i2s_dout = dout;
    volume = 12; // Default volume
}

void AudioPlayer::begin()
{
    audio.setPinout(i2s_bclk, i2s_lrc, i2s_dout);
    audio.setVolume(volume);
}

void AudioPlayer::setVolume(int vol)
{
    if (vol >= 0 && vol <= 21)
    {
        volume = vol;
        audio.setVolume(volume);
    }
}

int AudioPlayer::getVolume()
{
    return volume;
}

void AudioPlayer::playURL(const char *url)
{
    audio.connecttohost(url);
}

void AudioPlayer::stop()
{
    audio.stopSong();
}

void AudioPlayer::stopAndPlay(const char *url)
{
    audio.stopSong();
    audio.connecttohost(url);
}

void AudioPlayer::loop()
{
    audio.loop();
}

// Static callback functions
void AudioPlayer::onInfo(const char *info)
{
    Serial.print("info        ");
    Serial.println(info);
}

void AudioPlayer::onId3Data(const char *info)
{
    Serial.print("id3data     ");
    Serial.println(info);
}

void AudioPlayer::onEofMp3(const char *info)
{
    Serial.print("eof_mp3     ");
    Serial.println(info);
}

void AudioPlayer::onShowStation(const char *info)
{
    Serial.print("station     ");
    Serial.println(info);
}

void AudioPlayer::onShowStreamTitle(const char *info)
{
    Serial.print("streamtitle ");
    Serial.println(info);
}

void AudioPlayer::onBitrate(const char *info)
{
    Serial.print("bitrate     ");
    Serial.println(info);
}

void AudioPlayer::onCommercial(const char *info)
{
    Serial.print("commercial  ");
    Serial.println(info);
}

void AudioPlayer::onIcyUrl(const char *info)
{
    Serial.print("icyurl      ");
    Serial.println(info);
}

void AudioPlayer::onLastHost(const char *info)
{
    Serial.print("lasthost    ");
    Serial.println(info);
}

// Audio library required global callback functions
void audio_info(const char *info) { AudioPlayer::onInfo(info); }
void audio_id3data(const char *info) { AudioPlayer::onId3Data(info); }
void audio_eof_mp3(const char *info) { AudioPlayer::onEofMp3(info); }
void audio_showstation(const char *info) { AudioPlayer::onShowStation(info); }
void audio_showstreamtitle(const char *info) { AudioPlayer::onShowStreamTitle(info); }
void audio_bitrate(const char *info) { AudioPlayer::onBitrate(info); }
void audio_commercial(const char *info) { AudioPlayer::onCommercial(info); }
void audio_icyurl(const char *info) { AudioPlayer::onIcyUrl(info); }
void audio_lasthost(const char *info) { AudioPlayer::onLastHost(info); }
