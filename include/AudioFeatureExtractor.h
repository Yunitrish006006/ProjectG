#ifndef AUDIO_FEATURE_EXTRACTOR_H
#define AUDIO_FEATURE_EXTRACTOR_H

#include <Arduino.h>
#include <math.h>
#include "arduinoFFT.h"

// 音訊特徵提取配置
#define FFT_SIZE 512
#define SAMPLE_RATE 16000
#define MEL_FILTER_BANKS 26
#define MFCC_COEFFS 13
#define FRAME_SIZE 400
#define HOP_SIZE 160

class AudioFeatureExtractor
{
private:
    // FFT 相關
    ArduinoFFT<double> FFT;
    double *vReal;
    double *vImag;
    double *powerSpectrum;

    // 梅爾濾波器組
    double melFilters[MEL_FILTER_BANKS][FFT_SIZE / 2 + 1];
    double melEnergies[MEL_FILTER_BANKS];
    double mfccCoeffs[MFCC_COEFFS];

    // 音訊緩衝區
    int16_t audioBuffer[FRAME_SIZE];
    int bufferIndex;

    // 預處理函數
    void initMelFilters();
    double melScale(double freq);
    double invMelScale(double mel);
    void applyHammingWindow(double *signal, int length);
    void computePowerSpectrum();
    void applyMelFilters();
    void computeMFCC();

public:
    // 建構函數和解構函數
    AudioFeatureExtractor();
    ~AudioFeatureExtractor();

    // 初始化
    bool begin();

    // 音訊數據處理
    bool processAudioFrame(int16_t *audio, int length);
    bool isFrameReady();

    // 特徵提取
    void extractFeatures();

    // 獲取結果
    double *getMelEnergies() { return melEnergies; }
    double *getMFCCCoeffs() { return mfccCoeffs; }
    double *getPowerSpectrum() { return powerSpectrum; }

    // 音訊特徵分析
    double computeSpectralCentroid();
    double computeSpectralBandwidth();
    double computeZeroCrossingRate(int16_t *audio, int length);
    double computeRMSEnergy(int16_t *audio, int length);

    // 特徵向量輸出
    void getFeatureVector(double *features, int maxLength);
    void printFeatures();

    // 重置緩衝區
    void reset();
};

#endif // AUDIO_FEATURE_EXTRACTOR_H
