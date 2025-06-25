#!/usr/bin/env python3
"""
ESP32 MQTT 音訊客戶端
用於接收和處理 ESP32 發送的音訊和 MFCC 數據
"""

import paho.mqtt.client as mqtt
import json
import numpy as np
import matplotlib.pyplot as plt
from collections import deque
import time
import threading
from datetime import datetime

class ESP32MqttAudioClient:
    def __init__(self, mqtt_host="192.168.137.1", mqtt_port=1883, 
                 mqtt_user=None, mqtt_password=None):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.mqtt_user = mqtt_user
        self.mqtt_password = mqtt_password
        
        # MQTT 客戶端
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        if mqtt_user and mqtt_password:
            self.client.username_pw_set(mqtt_user, mqtt_password)
        
        # 數據緩衝區
        self.audio_buffer = deque(maxlen=8000)  # 約 0.5 秒的音訊 (16kHz)
        self.mfcc_buffer = deque(maxlen=100)    # 最近 100 個 MFCC 幀
        self.mel_buffer = deque(maxlen=100)     # 最近 100 個梅爾能量幀
        self.features_buffer = deque(maxlen=200) # 特徵歷史
        
        # 統計信息
        self.stats = {
            'audio_packets': 0,
            'mfcc_packets': 0,
            'mel_packets': 0,
            'feature_packets': 0,
            'start_time': time.time(),
            'last_audio_time': 0,
            'last_mfcc_time': 0,
            'last_mel_time': 0,
            'last_feature_time': 0
        }
        
        # 控制變量
        self.connected = False
        self.running = False
        self.plot_enabled = True
        
        # 圖形相關
        self.fig = None
        self.axes = None
        self.plot_thread = None
        
    def on_connect(self, client, userdata, flags, rc):
        """MQTT 連接回調"""
        if rc == 0:
            self.connected = True
            print(f"✓ 已連接到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}")
            
            # 訂閱所有音訊相關主題
            topics = [
                ("esp32/audio/raw", 0),
                ("esp32/audio/mfcc", 0),
                ("esp32/audio/mel", 0),
                ("esp32/audio/features", 0),
                ("esp32/audio/status", 0)
            ]
            
            for topic, qos in topics:
                client.subscribe(topic, qos)
                print(f"✓ 訂閱主題: {topic}")
                
        else:
            print(f"✗ MQTT 連接失敗，錯誤碼: {rc}")
    
    def on_disconnect(self, client, userdata, rc):
        """MQTT 斷開連接回調"""
        self.connected = False
        print(f"MQTT 連接已斷開，錯誤碼: {rc}")
    
    def on_message(self, client, userdata, msg):
        """MQTT 消息處理回調"""
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            data = json.loads(payload)
            
            if topic == "esp32/audio/raw":
                self.handle_audio_data(data)
            elif topic == "esp32/audio/mfcc":
                self.handle_mfcc_data(data)
            elif topic == "esp32/audio/mel":
                self.handle_mel_data(data)
            elif topic == "esp32/audio/features":
                self.handle_features_data(data)
            elif topic == "esp32/audio/status":
                self.handle_status_data(data)
                
        except Exception as e:
            print(f"處理消息時出錯: {e}")
            print(f"主題: {topic}, 載荷: {msg.payload[:100]}...")
    
    def handle_audio_data(self, data):
        """處理音訊數據"""
        if 'audio' in data and 'timestamp' in data:
            audio_samples = np.array(data['audio'], dtype=np.int16)
            timestamp = data['timestamp']
            
            # 添加到緩衝區
            self.audio_buffer.extend(audio_samples)
            
            # 更新統計
            self.stats['audio_packets'] += 1
            self.stats['last_audio_time'] = timestamp
            
            # 每 100 個包輸出一次音訊統計
            if self.stats['audio_packets'] % 100 == 0:
                print(f"音訊 - 包: {self.stats['audio_packets']}, "
                      f"樣本: {len(audio_samples)}, "
                      f"RMS: {np.sqrt(np.mean(audio_samples**2)):.1f}")
    
    def handle_mfcc_data(self, data):
        """處理 MFCC 數據"""
        if 'mfcc' in data and 'timestamp' in data:
            mfcc_coeffs = np.array(data['mfcc'])
            timestamp = data['timestamp']
            
            # 添加到緩衝區
            self.mfcc_buffer.append({
                'timestamp': timestamp,
                'mfcc': mfcc_coeffs
            })
            
            # 更新統計
            self.stats['mfcc_packets'] += 1
            self.stats['last_mfcc_time'] = timestamp
            
            # 每 20 個包輸出一次 MFCC 統計
            if self.stats['mfcc_packets'] % 20 == 0:
                print(f"MFCC - 包: {self.stats['mfcc_packets']}, "
                      f"係數: {len(mfcc_coeffs)}, "
                      f"範圍: [{mfcc_coeffs.min():.2f}, {mfcc_coeffs.max():.2f}]")
    
    def handle_mel_data(self, data):
        """處理梅爾能量數據"""
        if 'mel' in data and 'timestamp' in data:
            mel_energies = np.array(data['mel'])
            timestamp = data['timestamp']
            
            # 添加到緩衝區
            self.mel_buffer.append({
                'timestamp': timestamp,
                'mel': mel_energies
            })
            
            # 更新統計
            self.stats['mel_packets'] += 1
            self.stats['last_mel_time'] = timestamp
            
            # 每 20 個包輸出一次梅爾統計
            if self.stats['mel_packets'] % 20 == 0:
                mean_energy = np.mean(mel_energies)
                print(f"梅爾 - 包: {self.stats['mel_packets']}, "
                      f"濾波器: {len(mel_energies)}, "
                      f"平均能量: {mean_energy:.2f}")
    
    def handle_features_data(self, data):
        """處理其他特徵數據"""
        if 'timestamp' in data:
            features = {
                'timestamp': data['timestamp'],
                'spectralCentroid': data.get('spectralCentroid', 0),
                'spectralBandwidth': data.get('spectralBandwidth', 0),
                'zeroCrossingRate': data.get('zeroCrossingRate', 0),
                'rmsEnergy': data.get('rmsEnergy', 0)
            }
            
            # 添加到緩衝區
            self.features_buffer.append(features)
            
            # 更新統計
            self.stats['feature_packets'] += 1
            self.stats['last_feature_time'] = data['timestamp']
            
            # 每 50 個包輸出一次特徵統計
            if self.stats['feature_packets'] % 50 == 0:
                print(f"特徵 - 包: {self.stats['feature_packets']}, "
                      f"重心: {features['spectralCentroid']:.1f}, "
                      f"帶寬: {features['spectralBandwidth']:.1f}, "
                      f"過零率: {features['zeroCrossingRate']:.3f}")
    
    def handle_status_data(self, data):
        """處理狀態數據"""
        print(f"ESP32 狀態: {json.dumps(data, indent=2)}")
    
    def send_control_command(self, command):
        """發送控制命令到 ESP32"""
        if self.connected:
            topic = "esp32/audio/control"
            payload = json.dumps({"command": command, "timestamp": int(time.time() * 1000)})
            self.client.publish(topic, payload)
            print(f"發送命令: {command}")
        else:
            print("未連接到 MQTT，無法發送命令")
    
    def start_publishing(self):
        """啟動 ESP32 音訊發布"""
        self.send_control_command("startPublishing")
    
    def stop_publishing(self):
        """停止 ESP32 音訊發布"""
        self.send_control_command("stopPublishing")
    
    def enable_features(self):
        """啟用特徵提取"""
        self.send_control_command("enableFeatures")
    
    def disable_features(self):
        """停用特徵提取"""
        self.send_control_command("disableFeatures")
    
    def get_status(self):
        """獲取 ESP32 狀態"""
        self.send_control_command("getStatus")
    
    def connect(self):
        """連接到 MQTT 服務器"""
        try:
            print(f"正在連接到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}...")
            self.client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.running = True
            
            # 啟動 MQTT 循環線程
            self.client.loop_start()
            
            # 等待連接
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.1)
                timeout -= 0.1
            
            if self.connected:
                print("✓ MQTT 連接成功")
                return True
            else:
                print("✗ MQTT 連接超時")
                return False
                
        except Exception as e:
            print(f"✗ MQTT 連接失敗: {e}")
            return False
    
    def disconnect(self):
        """斷開 MQTT 連接"""
        self.running = False
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
        print("已斷開 MQTT 連接")
    
    def start_plotting(self):
        """啟動即時繪圖"""
        if self.plot_enabled:
            self.plot_thread = threading.Thread(target=self.plot_loop, daemon=True)
            self.plot_thread.start()
    
    def plot_loop(self):
        """繪圖循環"""
        plt.ion()
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('ESP32 MQTT 音訊監控')
        
        while self.running and self.plot_enabled:
            try:
                # 音訊波形
                if len(self.audio_buffer) > 0:
                    self.axes[0, 0].clear()
                    audio_data = np.array(list(self.audio_buffer))[-1600:]  # 最近 0.1 秒
                    self.axes[0, 0].plot(audio_data)
                    self.axes[0, 0].set_title('音訊波形')
                    self.axes[0, 0].set_ylabel('振幅')
                
                # MFCC 係數
                if len(self.mfcc_buffer) > 0:
                    self.axes[0, 1].clear()
                    mfcc_matrix = np.array([frame['mfcc'] for frame in list(self.mfcc_buffer)[-50:]])
                    if mfcc_matrix.size > 0:
                        im = self.axes[0, 1].imshow(mfcc_matrix.T, aspect='auto', origin='lower')
                        self.axes[0, 1].set_title('MFCC 係數')
                        self.axes[0, 1].set_ylabel('係數索引')
                        self.axes[0, 1].set_xlabel('時間幀')
                
                # 梅爾頻譜
                if len(self.mel_buffer) > 0:
                    self.axes[1, 0].clear()
                    mel_matrix = np.array([frame['mel'] for frame in list(self.mel_buffer)[-50:]])
                    if mel_matrix.size > 0:
                        im = self.axes[1, 0].imshow(mel_matrix.T, aspect='auto', origin='lower')
                        self.axes[1, 0].set_title('梅爾頻譜')
                        self.axes[1, 0].set_ylabel('梅爾濾波器')
                        self.axes[1, 0].set_xlabel('時間幀')
                
                # 特徵時序
                if len(self.features_buffer) > 0:
                    self.axes[1, 1].clear()
                    features = list(self.features_buffer)[-100:]
                    times = [f['timestamp'] for f in features]
                    centroids = [f['spectralCentroid'] for f in features]
                    energies = [f['rmsEnergy'] for f in features]
                    
                    self.axes[1, 1].plot(times, centroids, label='頻譜重心', alpha=0.7)
                    self.axes[1, 1].plot(times, np.array(energies) * 100, label='RMS能量×100', alpha=0.7)
                    self.axes[1, 1].set_title('特徵時序')
                    self.axes[1, 1].set_ylabel('值')
                    self.axes[1, 1].legend()
                
                plt.tight_layout()
                plt.pause(0.1)
                
            except Exception as e:
                print(f"繪圖錯誤: {e}")
                
            time.sleep(0.5)
    
    def print_stats(self):
        """輸出統計信息"""
        runtime = time.time() - self.stats['start_time']
        print("\n=== MQTT 音訊客戶端統計 ===")
        print(f"運行時間: {runtime:.1f} 秒")
        print(f"音訊包: {self.stats['audio_packets']}")
        print(f"MFCC 包: {self.stats['mfcc_packets']}")
        print(f"梅爾包: {self.stats['mel_packets']}")
        print(f"特徵包: {self.stats['feature_packets']}")
        
        if runtime > 0:
            print(f"音訊包速率: {self.stats['audio_packets']/runtime:.1f} 包/秒")
            print(f"MFCC 包速率: {self.stats['mfcc_packets']/runtime:.1f} 包/秒")
        
        print(f"音訊緩衝區: {len(self.audio_buffer)} 樣本")
        print(f"MFCC 緩衝區: {len(self.mfcc_buffer)} 幀")
        print(f"梅爾緩衝區: {len(self.mel_buffer)} 幀")
        print("========================\n")

def main():
    """主函數"""
    print("ESP32 MQTT 音訊客戶端")
    print("請確保：")
    print("1. ESP32 已連接到 WiFi")
    print("2. MQTT 服務器正在運行")
    print("3. ESP32 程式已上傳並運行")
    print()
    
    # 創建客戶端（請根據您的網絡環境修改 IP 地址）
    client = ESP32MqttAudioClient(
        mqtt_host="192.168.137.1",  # 修改為您的 MQTT 服務器 IP
        mqtt_port=1883
    )
    
    try:
        # 連接到 MQTT
        if not client.connect():
            return
        
        # 等待一段時間讓連接穩定
        time.sleep(2)
        
        # 發送初始命令
        print("發送初始命令...")
        client.get_status()
        time.sleep(1)
        client.enable_features()
        time.sleep(1)
        client.start_publishing()
        
        # 啟動即時繪圖
        client.start_plotting()
        
        print("\n開始接收數據...")
        print("按 Ctrl+C 停止")
        
        # 主循環
        start_time = time.time()
        while True:
            time.sleep(10)
            
            # 每 10 秒輸出統計
            client.print_stats()
            
            # 每 30 秒請求狀態更新
            if (time.time() - start_time) % 30 < 10:
                client.get_status()
    
    except KeyboardInterrupt:
        print("\n正在停止...")
        client.stop_publishing()
        time.sleep(1)
        
    except Exception as e:
        print(f"運行時錯誤: {e}")
        
    finally:
        client.disconnect()
        print("程式已退出")

if __name__ == "__main__":
    main()
