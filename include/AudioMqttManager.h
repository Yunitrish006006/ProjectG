#ifndef AUDIO_MQTT_MANAGER_H
#define AUDIO_MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "AudioFeatureExtractor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// MQTT 配置
#define MQTT_BUFFER_SIZE 2048
#define MQTT_RECONNECT_INTERVAL 5000
#define AUDIO_PUBLISH_INTERVAL 100   // ms
#define FEATURE_PUBLISH_INTERVAL 200 // ms

// MQTT 主題定義
#define MQTT_TOPIC_AUDIO "esp32/audio/raw"
#define MQTT_TOPIC_MFCC "esp32/audio/mfcc"
#define MQTT_TOPIC_MEL "esp32/audio/mel"
#define MQTT_TOPIC_FEATURES "esp32/audio/features"
#define MQTT_TOPIC_STATUS "esp32/audio/status"
#define MQTT_TOPIC_CONTROL "esp32/audio/control"

// 音訊數據包結構 (優化為 MQTT 傳輸)
struct MqttAudioPacket
{
    uint32_t timestamp;
    uint16_t sequenceNumber;
    uint16_t dataLength;
    int16_t audioData[256]; // 減少大小以適應 MQTT
};

// MFCC 特徵包結構
struct MqttMfccPacket
{
    uint32_t timestamp;
    double mfccCoeffs[MFCC_COEFFS];
    bool isValid;
};

// 梅爾能量包結構
struct MqttMelPacket
{
    uint32_t timestamp;
    double melEnergies[MEL_FILTER_BANKS];
    bool isValid;
};

// 其他音訊特徵包結構
struct MqttFeaturesPacket
{
    uint32_t timestamp;
    double spectralCentroid;
    double spectralBandwidth;
    double zeroCrossingRate;
    double rmsEnergy;
    bool isValid;
};

class AudioMqttManager
{
private:
    // MQTT 客戶端
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    // 音訊特徵提取器
    AudioFeatureExtractor *featureExtractor;

    // 音訊緩衝區和隊列
    QueueHandle_t audioQueue;
    QueueHandle_t mfccQueue;
    QueueHandle_t melQueue;
    QueueHandle_t featuresQueue;
    SemaphoreHandle_t mqttMutex;

    // 任務控制柄
    TaskHandle_t mqttTaskHandle;
    TaskHandle_t audioPublishTaskHandle;
    TaskHandle_t featurePublishTaskHandle;

    // MQTT 連接參數
    const char *mqttServer;
    int mqttPort;
    const char *mqttUser;
    const char *mqttPassword;
    const char *clientId;

    // 狀態變量
    bool isConnected;
    bool isPublishing;
    bool isFeatureExtractionEnabled;
    uint16_t currentSequence;
    uint32_t lastAudioPublish;
    uint32_t lastFeaturePublish;
    uint32_t lastReconnectAttempt;

    // 統計信息
    struct
    {
        uint32_t audioPacketsPublished;
        uint32_t mfccPacketsPublished;
        uint32_t melPacketsPublished;
        uint32_t featurePacketsPublished;
        uint32_t reconnectCount;
        uint32_t publishErrors;
    } stats;

    // 內部方法
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool reconnectMqtt();
    bool publishAudioPacket(MqttAudioPacket *packet);
    bool publishMfccPacket(MqttMfccPacket *packet);
    bool publishMelPacket(MqttMelPacket *packet);
    bool publishFeaturesPacket(MqttFeaturesPacket *packet);
    void handleControlMessage(const char *message);

    // 靜態任務函數
    static void mqttTask(void *parameter);
    static void audioPublishTask(void *parameter);
    static void featurePublishTask(void *parameter);

    // 靜態回調函數
    static void staticMqttCallback(char *topic, byte *payload, unsigned int length);

public:
    // 建構函數和解構函數
    AudioMqttManager();
    ~AudioMqttManager();

    // 初始化和清理
    bool begin(const char *server, int port = 1883,
               const char *user = nullptr, const char *password = nullptr,
               const char *clientId = "ESP32_Audio");
    void end();

    // MQTT 連接控制
    bool connect();
    void disconnect();
    bool isConnectedToMqtt() { return isConnected; }

    // 音訊發布控制
    bool startPublishing();
    bool stopPublishing();
    bool isPublishingActive() { return isPublishing; }

    // 特徵提取控制
    bool enableFeatureExtraction();
    bool disableFeatureExtraction();
    bool isFeatureExtractionActive() { return isFeatureExtractionEnabled; }

    // 音訊數據輸入
    bool pushAudioData(int16_t *audioData, size_t length);

    // 配置方法
    void setPublishIntervals(int audioInterval, int featureInterval);
    void setMqttKeepAlive(int keepAlive);

    // 統計信息
    void getPublishStats(uint32_t *audioPackets, uint32_t *mfccPackets,
                         uint32_t *melPackets, uint32_t *featurePackets);
    uint32_t getReconnectCount() { return stats.reconnectCount; }
    uint32_t getPublishErrors() { return stats.publishErrors; }

    // 狀態發布
    void publishStatus();

    // 工具方法
    void printStatus();
    void loop(); // 需要在主循環中調用
};

#endif // AUDIO_MQTT_MANAGER_H
