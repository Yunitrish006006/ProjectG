#include "AudioFeatureExtractor.h"

AudioFeatureExtractor::AudioFeatureExtractor()
{
    // 初始化指針
    vReal = nullptr;
    vImag = nullptr;
    powerSpectrum = nullptr;
    bufferIndex = 0;

    // 清零緩衝區
    memset(audioBuffer, 0, sizeof(audioBuffer));
    memset(melEnergies, 0, sizeof(melEnergies));
    memset(mfccCoeffs, 0, sizeof(mfccCoeffs));
}

AudioFeatureExtractor::~AudioFeatureExtractor()
{
    if (vReal)
    {
        delete[] vReal;
        vReal = nullptr;
    }
    if (vImag)
    {
        delete[] vImag;
        vImag = nullptr;
    }
    if (powerSpectrum)
    {
        delete[] powerSpectrum;
        powerSpectrum = nullptr;
    }
}

bool AudioFeatureExtractor::begin()
{
    Serial.println("正在初始化音訊特徵提取器...");

    // 分配FFT內存
    vReal = new (std::nothrow) double[FFT_SIZE];
    vImag = new (std::nothrow) double[FFT_SIZE];
    powerSpectrum = new (std::nothrow) double[FFT_SIZE / 2 + 1];

    if (!vReal || !vImag || !powerSpectrum)
    {
        Serial.println("✗ FFT 內存分配失敗");
        return false;
    }

    // 初始化FFT數組
    for (int i = 0; i < FFT_SIZE; i++)
    {
        vReal[i] = 0.0;
        vImag[i] = 0.0;
    }

    for (int i = 0; i <= FFT_SIZE / 2; i++)
    {
        powerSpectrum[i] = 0.0;
    }

    // 初始化梅爾濾波器組
    initMelFilters();

    Serial.println("✓ 音訊特徵提取器初始化成功");
    return true;
}

void AudioFeatureExtractor::initMelFilters()
{
    Serial.println("正在初始化梅爾濾波器組...");

    // 清零梅爾濾波器
    for (int i = 0; i < MEL_FILTER_BANKS; i++)
    {
        for (int j = 0; j <= FFT_SIZE / 2; j++)
        {
            melFilters[i][j] = 0.0;
        }
    }

    // 計算梅爾刻度的頻率邊界
    double lowFreq = 0.0;
    double highFreq = SAMPLE_RATE / 2.0;
    double lowMel = melScale(lowFreq);
    double highMel = melScale(highFreq);

    // 等間距梅爾點
    double melPoints[MEL_FILTER_BANKS + 2];
    for (int i = 0; i < MEL_FILTER_BANKS + 2; i++)
    {
        melPoints[i] = lowMel + (highMel - lowMel) * i / (MEL_FILTER_BANKS + 1);
    }

    // 轉換回頻率
    double freqPoints[MEL_FILTER_BANKS + 2];
    for (int i = 0; i < MEL_FILTER_BANKS + 2; i++)
    {
        freqPoints[i] = invMelScale(melPoints[i]);
    }

    // 創建三角形濾波器
    for (int m = 0; m < MEL_FILTER_BANKS; m++)
    {
        double leftFreq = freqPoints[m];
        double centerFreq = freqPoints[m + 1];
        double rightFreq = freqPoints[m + 2];

        for (int k = 0; k <= FFT_SIZE / 2; k++)
        {
            double freq = k * SAMPLE_RATE / (double)FFT_SIZE;

            if (freq >= leftFreq && freq <= centerFreq)
            {
                melFilters[m][k] = (freq - leftFreq) / (centerFreq - leftFreq);
            }
            else if (freq >= centerFreq && freq <= rightFreq)
            {
                melFilters[m][k] = (rightFreq - freq) / (rightFreq - centerFreq);
            }
        }
    }

    Serial.printf("✓ %d 個梅爾濾波器初始化完成\n", MEL_FILTER_BANKS);
}

double AudioFeatureExtractor::melScale(double freq)
{
    return 2595.0 * log10(1.0 + freq / 700.0);
}

double AudioFeatureExtractor::invMelScale(double mel)
{
    return 700.0 * (pow(10.0, mel / 2595.0) - 1.0);
}

void AudioFeatureExtractor::applyHammingWindow(double *signal, int length)
{
    for (int i = 0; i < length; i++)
    {
        signal[i] *= (0.54 - 0.46 * cos(2.0 * PI * i / (length - 1)));
    }
}

bool AudioFeatureExtractor::processAudioFrame(int16_t *audio, int length)
{
    for (int i = 0; i < length && bufferIndex < FRAME_SIZE; i++)
    {
        audioBuffer[bufferIndex++] = audio[i];
    }

    return isFrameReady();
}

bool AudioFeatureExtractor::isFrameReady()
{
    return bufferIndex >= FRAME_SIZE;
}

void AudioFeatureExtractor::extractFeatures()
{
    if (!isFrameReady())
    {
        return;
    }

    // 將音訊數據轉換為double並應用視窗函數
    for (int i = 0; i < FRAME_SIZE && i < FFT_SIZE; i++)
    {
        vReal[i] = (double)audioBuffer[i] / 32768.0; // 正規化到 [-1, 1]
        vImag[i] = 0.0;
    }

    // 填充零至FFT大小
    for (int i = FRAME_SIZE; i < FFT_SIZE; i++)
    {
        vReal[i] = 0.0;
        vImag[i] = 0.0;
    }

    // 應用漢明視窗
    applyHammingWindow(vReal, FRAME_SIZE);

    // 執行FFT
    FFT.windowing(vReal, FFT_SIZE, FFT_WIN_TYP_RECTANGLE, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SIZE, FFT_FORWARD);

    // 計算功率譜
    computePowerSpectrum();

    // 應用梅爾濾波器
    applyMelFilters();

    // 計算MFCC
    computeMFCC();

    // 移動緩衝區 (重疊處理)
    memmove(audioBuffer, audioBuffer + HOP_SIZE, (FRAME_SIZE - HOP_SIZE) * sizeof(int16_t));
    bufferIndex -= HOP_SIZE;
    if (bufferIndex < 0)
        bufferIndex = 0;
}

void AudioFeatureExtractor::computePowerSpectrum()
{
    for (int i = 0; i <= FFT_SIZE / 2; i++)
    {
        powerSpectrum[i] = vReal[i] * vReal[i] + vImag[i] * vImag[i];

        // 避免對數運算時的零值
        if (powerSpectrum[i] < 1e-10)
        {
            powerSpectrum[i] = 1e-10;
        }
    }
}

void AudioFeatureExtractor::applyMelFilters()
{
    for (int m = 0; m < MEL_FILTER_BANKS; m++)
    {
        melEnergies[m] = 0.0;

        for (int k = 0; k <= FFT_SIZE / 2; k++)
        {
            melEnergies[m] += powerSpectrum[k] * melFilters[m][k];
        }

        // 轉換為對數能量
        if (melEnergies[m] > 0)
        {
            melEnergies[m] = log(melEnergies[m]);
        }
        else
        {
            melEnergies[m] = -10.0; // 避免 log(0)
        }
    }
}

void AudioFeatureExtractor::computeMFCC()
{
    // 離散餘弦變換 (DCT) 計算MFCC係數
    for (int i = 0; i < MFCC_COEFFS; i++)
    {
        mfccCoeffs[i] = 0.0;

        for (int j = 0; j < MEL_FILTER_BANKS; j++)
        {
            mfccCoeffs[i] += melEnergies[j] * cos(PI * i * (j + 0.5) / MEL_FILTER_BANKS);
        }

        if (i == 0)
        {
            mfccCoeffs[i] *= sqrt(1.0 / MEL_FILTER_BANKS);
        }
        else
        {
            mfccCoeffs[i] *= sqrt(2.0 / MEL_FILTER_BANKS);
        }
    }
}

double AudioFeatureExtractor::computeSpectralCentroid()
{
    double weightedSum = 0.0;
    double totalEnergy = 0.0;

    for (int i = 1; i <= FFT_SIZE / 2; i++)
    {
        double freq = i * SAMPLE_RATE / (double)FFT_SIZE;
        weightedSum += freq * powerSpectrum[i];
        totalEnergy += powerSpectrum[i];
    }

    return (totalEnergy > 0) ? weightedSum / totalEnergy : 0.0;
}

double AudioFeatureExtractor::computeSpectralBandwidth()
{
    double centroid = computeSpectralCentroid();
    double weightedSum = 0.0;
    double totalEnergy = 0.0;

    for (int i = 1; i <= FFT_SIZE / 2; i++)
    {
        double freq = i * SAMPLE_RATE / (double)FFT_SIZE;
        double deviation = freq - centroid;
        weightedSum += deviation * deviation * powerSpectrum[i];
        totalEnergy += powerSpectrum[i];
    }

    return (totalEnergy > 0) ? sqrt(weightedSum / totalEnergy) : 0.0;
}

double AudioFeatureExtractor::computeZeroCrossingRate(int16_t *audio, int length)
{
    int zeroCrossings = 0;

    for (int i = 1; i < length; i++)
    {
        if ((audio[i] >= 0 && audio[i - 1] < 0) || (audio[i] < 0 && audio[i - 1] >= 0))
        {
            zeroCrossings++;
        }
    }

    return (double)zeroCrossings / (length - 1);
}

double AudioFeatureExtractor::computeRMSEnergy(int16_t *audio, int length)
{
    double sumSquares = 0.0;

    for (int i = 0; i < length; i++)
    {
        double sample = audio[i] / 32768.0;
        sumSquares += sample * sample;
    }

    return sqrt(sumSquares / length);
}

void AudioFeatureExtractor::getFeatureVector(double *features, int maxLength)
{
    int index = 0;

    // MFCC 係數
    for (int i = 0; i < MFCC_COEFFS && index < maxLength; i++)
    {
        features[index++] = mfccCoeffs[i];
    }

    // 梅爾能量 (前幾個)
    for (int i = 0; i < min(6, MEL_FILTER_BANKS) && index < maxLength; i++)
    {
        features[index++] = melEnergies[i];
    }

    // 額外特徵
    if (index < maxLength)
        features[index++] = computeSpectralCentroid();
    if (index < maxLength)
        features[index++] = computeSpectralBandwidth();
    if (index < maxLength)
        features[index++] = computeZeroCrossingRate(audioBuffer, FRAME_SIZE);
    if (index < maxLength)
        features[index++] = computeRMSEnergy(audioBuffer, FRAME_SIZE);
}

void AudioFeatureExtractor::printFeatures()
{
    Serial.println("=== 音訊特徵 ===");

    Serial.print("MFCC: ");
    for (int i = 0; i < MFCC_COEFFS; i++)
    {
        Serial.printf("%.3f ", mfccCoeffs[i]);
    }
    Serial.println();

    Serial.print("梅爾能量 (前8個): ");
    for (int i = 0; i < min(8, MEL_FILTER_BANKS); i++)
    {
        Serial.printf("%.3f ", melEnergies[i]);
    }
    Serial.println();

    Serial.printf("頻譜重心: %.2f Hz\n", computeSpectralCentroid());
    Serial.printf("頻譜帶寬: %.2f Hz\n", computeSpectralBandwidth());
    Serial.printf("過零率: %.4f\n", computeZeroCrossingRate(audioBuffer, FRAME_SIZE));
    Serial.printf("RMS能量: %.4f\n", computeRMSEnergy(audioBuffer, FRAME_SIZE));
    Serial.println("===============");
}

void AudioFeatureExtractor::reset()
{
    bufferIndex = 0;
    memset(audioBuffer, 0, sizeof(audioBuffer));
    memset(melEnergies, 0, sizeof(melEnergies));
    memset(mfccCoeffs, 0, sizeof(mfccCoeffs));
}
