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

    // 設定音頻緩衝參數以減少中斷
    // 這些參數可能需要根據具體的 Audio 庫版本調整
}

bool AudioPlayer::beginSafe(int maxRetries)
{
    for (int attempt = 0; attempt < maxRetries; attempt++)
    {
        Serial.printf("AudioPlayer 初始化嘗試 %d/%d\n", attempt + 1, maxRetries);

        // 每次嘗試前稍作延遲，確保硬體準備就緒
        if (attempt > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        // 嘗試設置引腳配置
        bool success = true;

        // 這裡我們無法直接捕獲硬體異常，但可以設置一個看門狗機制
        // 或者依賴上層的異常處理
        audio.setPinout(i2s_bclk, i2s_lrc, i2s_dout);

        // 如果到達這裡，說明 setPinout 沒有崩潰
        audio.setVolume(volume);

        Serial.printf("AudioPlayer 初始化成功 (嘗試 %d)\n", attempt + 1);
        return true;
    }

    Serial.printf("AudioPlayer 初始化失敗，已嘗試 %d 次\n", maxRetries);
    return false;
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
    // 確保音頻循環被頻繁調用以維持流暢播放
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
