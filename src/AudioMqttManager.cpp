#include "AudioMqttManager.h"

// 靜態實例指針，用於回調函數
AudioMqttManager *AudioMqttManager_instance = nullptr;

AudioMqttManager::AudioMqttManager()
    : mqttClient(wifiClient), featureExtractor(nullptr), audioQueue(nullptr), mfccQueue(nullptr), melQueue(nullptr), featuresQueue(nullptr), mqttMutex(nullptr), mqttTaskHandle(nullptr), audioPublishTaskHandle(nullptr), featurePublishTaskHandle(nullptr), mqttServer(nullptr), mqttPort(1883), mqttUser(nullptr), mqttPassword(nullptr), clientId("ESP32_Audio"), isConnected(false), isPublishing(false), isFeatureExtractionEnabled(false), currentSequence(0), lastAudioPublish(0), lastFeaturePublish(0), lastReconnectAttempt(0)
{
    // 初始化統計信息
    memset(&stats, 0, sizeof(stats));

    // 設置靜態實例指針
    AudioMqttManager_instance = this;
}

AudioMqttManager::~AudioMqttManager()
{
    end();
    AudioMqttManager_instance = nullptr;
}

bool AudioMqttManager::begin(const char *server, int port,
                             const char *user, const char *password,
                             const char *clientId)
{
    Serial.println("初始化 MQTT 音訊管理器...");

    // 保存連接參數
    this->mqttServer = server;
    this->mqttPort = port;
    this->mqttUser = user;
    this->mqttPassword = password;
    this->clientId = clientId;

    // 創建互斥鎖
    mqttMutex = xSemaphoreCreateMutex();
    if (!mqttMutex)
    {
        Serial.println("✗ 無法創建 MQTT 互斥鎖");
        return false;
    }

    // 創建隊列
    audioQueue = xQueueCreate(10, sizeof(MqttAudioPacket));
    mfccQueue = xQueueCreate(20, sizeof(MqttMfccPacket));
    melQueue = xQueueCreate(20, sizeof(MqttMelPacket));
    featuresQueue = xQueueCreate(20, sizeof(MqttFeaturesPacket));

    if (!audioQueue || !mfccQueue || !melQueue || !featuresQueue)
    {
        Serial.println("✗ 無法創建 MQTT 隊列");
        return false;
    }

    // 初始化特徵提取器
    featureExtractor = new AudioFeatureExtractor();
    if (!featureExtractor || !featureExtractor->begin())
    {
        Serial.println("✗ 無法初始化音訊特徵提取器");
        return false;
    }

    // 設置 MQTT 客戶端
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(staticMqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

    // 創建任務
    BaseType_t result = xTaskCreatePinnedToCore(
        mqttTask,
        "MQTT_Task",
        8192,
        this,
        3,
        &mqttTaskHandle,
        0 // Core 0
    );

    if (result != pdPASS)
    {
        Serial.println("✗ 無法創建 MQTT 任務");
        return false;
    }

    Serial.println("✓ MQTT 音訊管理器初始化完成");
    return true;
}

void AudioMqttManager::end()
{
    // 停止發布
    stopPublishing();

    // 斷開 MQTT 連接
    disconnect();

    // 刪除任務
    if (mqttTaskHandle)
    {
        vTaskDelete(mqttTaskHandle);
        mqttTaskHandle = nullptr;
    }
    if (audioPublishTaskHandle)
    {
        vTaskDelete(audioPublishTaskHandle);
        audioPublishTaskHandle = nullptr;
    }
    if (featurePublishTaskHandle)
    {
        vTaskDelete(featurePublishTaskHandle);
        featurePublishTaskHandle = nullptr;
    }

    // 清理隊列
    if (audioQueue)
    {
        vQueueDelete(audioQueue);
        audioQueue = nullptr;
    }
    if (mfccQueue)
    {
        vQueueDelete(mfccQueue);
        mfccQueue = nullptr;
    }
    if (melQueue)
    {
        vQueueDelete(melQueue);
        melQueue = nullptr;
    }
    if (featuresQueue)
    {
        vQueueDelete(featuresQueue);
        featuresQueue = nullptr;
    }

    // 清理互斥鎖
    if (mqttMutex)
    {
        vSemaphoreDelete(mqttMutex);
        mqttMutex = nullptr;
    }

    // 清理特徵提取器
    if (featureExtractor)
    {
        delete featureExtractor;
        featureExtractor = nullptr;
    }
}

bool AudioMqttManager::connect()
{
    if (isConnected)
        return true;

    Serial.printf("正在連接到 MQTT 服務器 %s:%d...\n", mqttServer, mqttPort);

    // 設置服務器地址和端口
    mqttClient.setServer(mqttServer, mqttPort);

    // 設置回調函數
    mqttClient.setCallback(staticMqttCallback);

    // 短暫延遲確保WiFi穩定
    vTaskDelay(pdMS_TO_TICKS(500));

    // 嘗試連接
    bool connected = false;
    if (mqttUser && mqttPassword)
    {
        connected = mqttClient.connect(clientId, mqttUser, mqttPassword);
    }
    else
    {
        connected = mqttClient.connect(clientId);
    }

    if (connected)
    {
        isConnected = true;
        Serial.println("✓ MQTT 連接成功");

        // 訂閱控制主題
        mqttClient.subscribe(MQTT_TOPIC_CONTROL);

        // 發布上線狀態
        publishStatus();

        return true;
    }
    else
    {
        Serial.printf("✗ MQTT 連接失敗，錯誤碼: %d\n", mqttClient.state());
        return false;
    }
}

void AudioMqttManager::disconnect()
{
    if (isConnected)
    {
        mqttClient.disconnect();
        isConnected = false;
        Serial.println("MQTT 已斷開連接");
    }
}

bool AudioMqttManager::startPublishing()
{
    if (isPublishing)
    {
        Serial.println("ℹ️ 發布已在進行中");
        return true;
    }

    if (!isConnected)
    {
        Serial.println("✗ MQTT 未連接，無法開始發布");
        return false;
    }
    Serial.println("🚀 開始創建 MQTT 發布任務...");

    // 重要：在創建任務之前設置 isPublishing = true
    // 否則任務啟動後會立即退出
    isPublishing = true;

    // 創建音訊發布任務
    BaseType_t result1 = xTaskCreatePinnedToCore(
        audioPublishTask,
        "Audio_Publish",
        8192, // 增加堆疊大小
        this,
        1, // 降低優先級
        &audioPublishTaskHandle,
        0 // Core 0 (與其他任務分開)
    );

    Serial.printf("音訊發布任務創建結果: %s\n", result1 == pdPASS ? "成功" : "失敗");

    // 創建特徵發布任務
    BaseType_t result2 = xTaskCreatePinnedToCore(
        featurePublishTask,
        "Feature_Publish",
        8192, // 增加堆疊大小
        this,
        1, // 降低優先級
        &featurePublishTaskHandle,
        0 // Core 0
    );

    Serial.printf("特徵發布任務創建結果: %s\n", result2 == pdPASS ? "成功" : "失敗");
    if (result1 == pdPASS && result2 == pdPASS)
    {
        Serial.println("✓ 音訊發布已開始");
        return true;
    }
    else
    {
        // 如果任務創建失敗，重置 isPublishing
        isPublishing = false;
        Serial.printf("✗ 無法創建發布任務 - 音訊: %d, 特徵: %d\n", result1, result2);
        return false;
    }
}

bool AudioMqttManager::stopPublishing()
{
    if (!isPublishing)
        return true;

    isPublishing = false;

    // 刪除發布任務
    if (audioPublishTaskHandle)
    {
        vTaskDelete(audioPublishTaskHandle);
        audioPublishTaskHandle = nullptr;
    }
    if (featurePublishTaskHandle)
    {
        vTaskDelete(featurePublishTaskHandle);
        featurePublishTaskHandle = nullptr;
    }

    Serial.println("音訊發布已停止");
    return true;
}

bool AudioMqttManager::enableFeatureExtraction()
{
    isFeatureExtractionEnabled = true;
    Serial.println("特徵提取已啟用");
    return true;
}

bool AudioMqttManager::disableFeatureExtraction()
{
    isFeatureExtractionEnabled = false;
    Serial.println("特徵提取已停用");
    return true;
}

bool AudioMqttManager::pushAudioData(int16_t *audioData, size_t length)
{
    if (!audioData || length == 0)
    {
        Serial.println("⚠️ pushAudioData: 無效的音訊數據或長度為0");
        return false;
    }

    // 創建音訊包
    MqttAudioPacket packet;
    packet.timestamp = millis();
    packet.sequenceNumber = currentSequence++;
    packet.dataLength = min(length, sizeof(packet.audioData) / sizeof(int16_t));

    // 複製音訊數據
    memcpy(packet.audioData, audioData, packet.dataLength * sizeof(int16_t));

    // 發送到音訊隊列
    BaseType_t queueResult = xQueueSend(audioQueue, &packet, 0);
    if (queueResult != pdTRUE)
    {
        static uint32_t queueFailCount = 0;
        queueFailCount++;
        if (queueFailCount % 100 == 0) // 每100次失敗輸出一次
        {
            Serial.printf("⚠️ 音訊隊列發送失敗 %d 次 - 隊列可能已滿\n", queueFailCount);
            Serial.printf("隊列狀態 - 剩餘空間: %d\n", uxQueueSpacesAvailable(audioQueue));
        }
        return false;
    } // 如果啟用特徵提取，處理特徵
    if (isFeatureExtractionEnabled && featureExtractor)
    {
        if (featureExtractor->processAudioFrame(audioData, length))
        {
            // 提取特徵
            featureExtractor->extractFeatures();

            // 創建 MFCC 包
            MqttMfccPacket mfccPacket;
            mfccPacket.timestamp = packet.timestamp;
            double *mfccData = featureExtractor->getMFCCCoeffs();
            if (mfccData)
            {
                memcpy(mfccPacket.mfccCoeffs, mfccData, MFCC_COEFFS * sizeof(double));
                mfccPacket.isValid = true;
                xQueueSend(mfccQueue, &mfccPacket, 0);
            }

            // 創建梅爾能量包
            MqttMelPacket melPacket;
            melPacket.timestamp = packet.timestamp;
            double *melData = featureExtractor->getMelEnergies();
            if (melData)
            {
                memcpy(melPacket.melEnergies, melData, MEL_FILTER_BANKS * sizeof(double));
                melPacket.isValid = true;
                xQueueSend(melQueue, &melPacket, 0);
            }

            // 創建其他特徵包
            MqttFeaturesPacket featuresPacket;
            featuresPacket.timestamp = packet.timestamp;
            featuresPacket.spectralCentroid = featureExtractor->computeSpectralCentroid();
            featuresPacket.spectralBandwidth = featureExtractor->computeSpectralBandwidth();
            featuresPacket.zeroCrossingRate = featureExtractor->computeZeroCrossingRate(audioData, length);
            featuresPacket.rmsEnergy = featureExtractor->computeRMSEnergy(audioData, length);
            featuresPacket.isValid = true;

            xQueueSend(featuresQueue, &featuresPacket, 0);
        }
    }

    return true;
}

bool AudioMqttManager::publishAudioPacket(MqttAudioPacket *packet)
{
    if (!isConnected || !packet)
    {
        Serial.printf("⚠️ 無法發布音訊包 - 連接狀態: %s, 包有效性: %s\n",
                      isConnected ? "已連接" : "未連接",
                      packet ? "有效" : "無效");
        return false;
    }

    // 創建 JSON 格式的音訊數據
    JsonDocument doc;
    doc["timestamp"] = packet->timestamp;
    doc["sequence"] = packet->sequenceNumber;
    doc["length"] = packet->dataLength;

    JsonArray audioArray = doc["audio"].to<JsonArray>();
    for (int i = 0; i < packet->dataLength; i++)
    {
        audioArray.add(packet->audioData[i]);
    }

    String jsonString;
    serializeJson(doc, jsonString);

    bool publishResult = mqttClient.publish(MQTT_TOPIC_AUDIO, jsonString.c_str());

    if (publishResult)
    {
        stats.audioPacketsPublished++;
        Serial.printf("✓ 音訊包已發布 - 序列: %d, 長度: %d\n",
                      packet->sequenceNumber, packet->dataLength);
        return true;
    }
    else
    {
        stats.publishErrors++;
        Serial.printf("✗ 音訊包發布失敗 - 序列: %d\n", packet->sequenceNumber);
        return false;
    }
}

bool AudioMqttManager::publishMfccPacket(MqttMfccPacket *packet)
{
    if (!isConnected || !packet || !packet->isValid)
        return false;

    JsonDocument doc;
    doc["timestamp"] = packet->timestamp;

    JsonArray mfccArray = doc["mfcc"].to<JsonArray>();
    for (int i = 0; i < MFCC_COEFFS; i++)
    {
        mfccArray.add(packet->mfccCoeffs[i]);
    }

    String jsonString;
    serializeJson(doc, jsonString);

    if (mqttClient.publish(MQTT_TOPIC_MFCC, jsonString.c_str()))
    {
        stats.mfccPacketsPublished++;
        return true;
    }
    else
    {
        stats.publishErrors++;
        return false;
    }
}

bool AudioMqttManager::publishMelPacket(MqttMelPacket *packet)
{
    if (!isConnected || !packet || !packet->isValid)
        return false;

    JsonDocument doc;
    doc["timestamp"] = packet->timestamp;

    JsonArray melArray = doc["mel"].to<JsonArray>();
    for (int i = 0; i < MEL_FILTER_BANKS; i++)
    {
        melArray.add(packet->melEnergies[i]);
    }

    String jsonString;
    serializeJson(doc, jsonString);

    if (mqttClient.publish(MQTT_TOPIC_MEL, jsonString.c_str()))
    {
        stats.melPacketsPublished++;
        return true;
    }
    else
    {
        stats.publishErrors++;
        return false;
    }
}

bool AudioMqttManager::publishFeaturesPacket(MqttFeaturesPacket *packet)
{
    if (!isConnected || !packet || !packet->isValid)
        return false;

    JsonDocument doc;
    doc["timestamp"] = packet->timestamp;
    doc["spectralCentroid"] = packet->spectralCentroid;
    doc["spectralBandwidth"] = packet->spectralBandwidth;
    doc["zeroCrossingRate"] = packet->zeroCrossingRate;
    doc["rmsEnergy"] = packet->rmsEnergy;

    String jsonString;
    serializeJson(doc, jsonString);

    if (mqttClient.publish(MQTT_TOPIC_FEATURES, jsonString.c_str()))
    {
        stats.featurePacketsPublished++;
        return true;
    }
    else
    {
        stats.publishErrors++;
        return false;
    }
}

void AudioMqttManager::publishStatus()
{
    if (!isConnected)
        return;

    JsonDocument doc;
    doc["device"] = clientId;
    doc["status"] = "online";
    doc["publishing"] = isPublishing;
    doc["featureExtraction"] = isFeatureExtractionEnabled;
    doc["timestamp"] = millis();
    doc["stats"]["audioPackets"] = stats.audioPacketsPublished;
    doc["stats"]["mfccPackets"] = stats.mfccPacketsPublished;
    doc["stats"]["melPackets"] = stats.melPacketsPublished;
    doc["stats"]["featurePackets"] = stats.featurePacketsPublished;
    doc["stats"]["reconnects"] = stats.reconnectCount;
    doc["stats"]["errors"] = stats.publishErrors;

    String jsonString;
    serializeJson(doc, jsonString);

    mqttClient.publish(MQTT_TOPIC_STATUS, jsonString.c_str());
}

void AudioMqttManager::handleControlMessage(const char *message)
{
    JsonDocument doc;
    if (deserializeJson(doc, message) != DeserializationError::Ok)
    {
        return;
    }

    const char *command = doc["command"];
    if (!command)
        return;

    if (strcmp(command, "startPublishing") == 0)
    {
        startPublishing();
    }
    else if (strcmp(command, "stopPublishing") == 0)
    {
        stopPublishing();
    }
    else if (strcmp(command, "enableFeatures") == 0)
    {
        enableFeatureExtraction();
    }
    else if (strcmp(command, "disableFeatures") == 0)
    {
        disableFeatureExtraction();
    }
    else if (strcmp(command, "getStatus") == 0)
    {
        publishStatus();
    }
}

bool AudioMqttManager::reconnectMqtt()
{
    if (isConnected)
        return true;

    uint32_t now = millis();
    if (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL)
    {
        return false;
    }

    lastReconnectAttempt = now;

    if (connect())
    {
        stats.reconnectCount++;
        return true;
    }

    return false;
}

void AudioMqttManager::loop()
{
    if (isConnected)
    {
        mqttClient.loop();
    }
    else
    {
        reconnectMqtt();
    }
}

void AudioMqttManager::printStatus()
{
    Serial.println("=== MQTT 音訊管理器狀態 ===");
    Serial.printf("MQTT 連接: %s\n", isConnected ? "是" : "否");
    Serial.printf("發布狀態: %s\n", isPublishing ? "是" : "否");
    Serial.printf("特徵提取: %s\n", isFeatureExtractionEnabled ? "是" : "否");
    Serial.printf("音訊包發布: %d\n", stats.audioPacketsPublished);
    Serial.printf("MFCC 包發布: %d\n", stats.mfccPacketsPublished);
    Serial.printf("梅爾包發布: %d\n", stats.melPacketsPublished);
    Serial.printf("特徵包發布: %d\n", stats.featurePacketsPublished);
    Serial.printf("重連次數: %d\n", stats.reconnectCount);
    Serial.printf("發布錯誤: %d\n", stats.publishErrors);
    Serial.println("========================");
}

// 靜態任務函數實現
void AudioMqttManager::mqttTask(void *parameter)
{
    AudioMqttManager *manager = static_cast<AudioMqttManager *>(parameter);

    while (true)
    {
        manager->loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AudioMqttManager::audioPublishTask(void *parameter)
{
    AudioMqttManager *manager = static_cast<AudioMqttManager *>(parameter);
    MqttAudioPacket packet;
    uint32_t processedPackets = 0;

    Serial.println("🎵 音訊發布任務已啟動");
    Serial.printf("🔍 isPublishing 狀態: %s\n", manager->isPublishing ? "true" : "false");

    while (manager->isPublishing)
    {
        if (xQueueReceive(manager->audioQueue, &packet, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            processedPackets++;

            if (xSemaphoreTake(manager->mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                bool publishResult = manager->publishAudioPacket(&packet);
                if (publishResult)
                {
                    // 每100個包輸出一次成功統計
                    if (processedPackets % 100 == 0)
                    {
                        Serial.printf("✓ 已處理 %d 個音訊包\n", processedPackets);
                    }
                }
                xSemaphoreGive(manager->mqttMutex);
            }
            else
            {
                Serial.println("⚠️ 無法獲取 MQTT 互斥鎖");
            }
        }
        else
        {
            // 隊列空的時候，定期輸出狀態
            static uint32_t idleCount = 0;
            idleCount++;
            if (idleCount % 100 == 0) // 每10秒輸出一次（100 * 100ms）
            {
                Serial.printf("🔄 音訊發布任務等待中 - 已處理: %d 包\n", processedPackets);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(AUDIO_PUBLISH_INTERVAL));
    }

    Serial.println("🛑 音訊發布任務結束");
    vTaskDelete(nullptr);
}

void AudioMqttManager::featurePublishTask(void *parameter)
{
    AudioMqttManager *manager = static_cast<AudioMqttManager *>(parameter);
    MqttMfccPacket mfccPacket;
    MqttMelPacket melPacket;
    MqttFeaturesPacket featuresPacket;

    while (manager->isPublishing)
    {
        // 發布 MFCC
        if (xQueueReceive(manager->mfccQueue, &mfccPacket, 0) == pdTRUE)
        {
            if (xSemaphoreTake(manager->mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                manager->publishMfccPacket(&mfccPacket);
                xSemaphoreGive(manager->mqttMutex);
            }
        }

        // 發布梅爾能量
        if (xQueueReceive(manager->melQueue, &melPacket, 0) == pdTRUE)
        {
            if (xSemaphoreTake(manager->mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                manager->publishMelPacket(&melPacket);
                xSemaphoreGive(manager->mqttMutex);
            }
        }

        // 發布其他特徵
        if (xQueueReceive(manager->featuresQueue, &featuresPacket, 0) == pdTRUE)
        {
            if (xSemaphoreTake(manager->mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                manager->publishFeaturesPacket(&featuresPacket);
                xSemaphoreGive(manager->mqttMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(FEATURE_PUBLISH_INTERVAL));
    }

    vTaskDelete(nullptr);
}

// 靜態回調函數
void AudioMqttManager::staticMqttCallback(char *topic, byte *payload, unsigned int length)
{
    if (AudioMqttManager_instance)
    {
        char message[length + 1];
        memcpy(message, payload, length);
        message[length] = '\0';
        AudioMqttManager_instance->handleControlMessage(message);
    }
}
