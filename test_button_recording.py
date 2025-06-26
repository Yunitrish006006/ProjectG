#!/usr/bin/env python3
"""
ESP32 ProjectG 按鈕錄音功能測試工具
測試按鈕錄音模式下的音訊MQTT傳輸功能
"""

import paho.mqtt.client as mqtt
import time
import json
import struct
from datetime import datetime
import wave
import numpy as np

class ButtonRecordingTester:
    def __init__(self):
        self.mqtt_broker = "broker.emqx.io"
        self.mqtt_port = 1883
        self.client_id = "ButtonRecordingTester"
        
        # MQTT主題
        self.audio_topic = "esp32/audio/raw"
        self.mfcc_topic = "esp32/audio/mfcc"
        self.mel_topic = "esp32/audio/mel"
        self.features_topic = "esp32/audio/features"
        self.status_topic = "esp32/status"
        
        # 錄音數據收集
        self.audio_packets = []
        self.recording_session = None
        self.start_time = None
        
        self.client = mqtt.Client(client_id=self.client_id)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
    def on_connect(self, client, userdata, flags, rc):
        print(f"🔗 連接到MQTT服務器: {self.mqtt_broker}")
        if rc == 0:
            print("✅ MQTT連接成功")
            # 訂閱所有相關主題
            topics = [
                self.audio_topic,
                self.mfcc_topic, 
                self.mel_topic,
                self.features_topic,
                self.status_topic
            ]
            
            for topic in topics:
                client.subscribe(topic)
                print(f"📡 已訂閱主題: {topic}")
        else:
            print(f"❌ MQTT連接失敗，錯誤碼: {rc}")
    
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        current_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        
        if topic == self.audio_topic:
            self.handle_audio_message(msg, current_time)
        elif topic == self.mfcc_topic:
            self.handle_mfcc_message(msg, current_time)
        elif topic == self.mel_topic:
            self.handle_mel_message(msg, current_time)
        elif topic == self.features_topic:
            self.handle_features_message(msg, current_time)
        elif topic == self.status_topic:
            self.handle_status_message(msg, current_time)
    
    def handle_audio_message(self, msg, timestamp):
        """處理音訊原始數據"""
        try:
            if len(msg.payload) < 8:  # 最小包頭大小
                return
                
            # 解析包頭 (32bit timestamp + 16bit sequence + 16bit samples)
            header = struct.unpack('<IHH', msg.payload[:8])
            esp32_timestamp, sequence, num_samples = header
            
            # 提取音訊數據
            audio_data = struct.unpack(f'<{num_samples}h', msg.payload[8:8+num_samples*2])
            
            # 檢測新錄音會話
            if self.recording_session is None or len(self.audio_packets) == 0:
                self.recording_session = timestamp
                self.start_time = time.time()
                self.audio_packets = []
                print(f"\n🎙️ [{timestamp}] 檢測到新錄音會話開始")
                print("=" * 60)
            
            self.audio_packets.append({
                'timestamp': esp32_timestamp,
                'sequence': sequence,
                'data': audio_data,
                'receive_time': timestamp
            })
            
            # 計算音訊統計
            audio_level = int(np.mean(np.abs(audio_data)))
            max_level = int(np.max(np.abs(audio_data)))
            
            print(f"🔊 [{timestamp}] 序列: {sequence:04d} | 樣本: {num_samples:3d} | "
                  f"平均: {audio_level:4d} | 峰值: {max_level:4d}")
            
        except Exception as e:
            print(f"❌ 音訊數據解析錯誤: {e}")
    
    def handle_mfcc_message(self, msg, timestamp):
        """處理MFCC特徵數據"""
        try:
            # 解析MFCC數據結構
            if len(msg.payload) >= 8:
                header = struct.unpack('<IH', msg.payload[:6])
                esp32_timestamp, num_features = header
                
                print(f"🧮 [{timestamp}] MFCC特徵: {num_features}個係數")
        except Exception as e:
            print(f"❌ MFCC數據解析錯誤: {e}")
    
    def handle_mel_message(self, msg, timestamp):
        """處理梅爾頻譜數據"""
        try:
            if len(msg.payload) >= 6:
                header = struct.unpack('<IH', msg.payload[:6])
                esp32_timestamp, num_bands = header
                
                print(f"📊 [{timestamp}] 梅爾頻譜: {num_bands}個頻帶")
        except Exception as e:
            print(f"❌ 梅爾頻譜數據解析錯誤: {e}")
    
    def handle_features_message(self, msg, timestamp):
        """處理其他音訊特徵"""
        try:
            if len(msg.payload) >= 4:
                print(f"📈 [{timestamp}] 音訊特徵數據: {len(msg.payload)}字節")
        except Exception as e:
            print(f"❌ 特徵數據解析錯誤: {e}")
    
    def handle_status_message(self, msg, timestamp):
        """處理ESP32狀態消息"""
        try:
            status_text = msg.payload.decode('utf-8')
            print(f"📟 [{timestamp}] ESP32狀態: {status_text}")
        except Exception as e:
            print(f"❌ 狀態消息解析錯誤: {e}")
    
    def save_recording_summary(self):
        """保存錄音會話摘要"""
        if not self.audio_packets:
            return
            
        duration = time.time() - self.start_time
        total_samples = sum(len(packet['data']) for packet in self.audio_packets)
        
        print("\n" + "=" * 60)
        print("📊 錄音會話摘要")
        print("=" * 60)
        print(f"🕐 錄音時長: {duration:.2f}秒")
        print(f"📦 收到包數: {len(self.audio_packets)}")
        print(f"🎵 總樣本數: {total_samples}")
        print(f"📊 平均包大小: {total_samples/len(self.audio_packets):.1f}樣本")
        print(f"📡 數據率: {total_samples*2/duration:.0f} 字節/秒")
        
        # 檢查序列號連續性
        sequences = [p['sequence'] for p in self.audio_packets]
        missing_sequences = []
        for i in range(1, len(sequences)):
            expected = (sequences[i-1] + 1) % 65536
            if sequences[i] != expected:
                missing_sequences.append(expected)
        
        if missing_sequences:
            print(f"⚠️ 丟失包序列: {len(missing_sequences)}個")
        else:
            print("✅ 所有音訊包按序接收")
        
        print("=" * 60)
    
    def run(self):
        """運行測試工具"""
        print("🎯 ESP32 ProjectG 按鈕錄音功能測試工具")
        print("=" * 60)
        print("📋 使用說明:")
        print("   1. 確保ESP32已連接並處於按鈕錄音模式")
        print("   2. 按下ESP32的麥克風按鈕開始錄音")
        print("   3. 再按一次按鈕停止錄音")
        print("   4. 觀察本工具的數據接收情況")
        print("   5. 按Ctrl+C退出測試")
        print("=" * 60)
        
        try:
            # 連接MQTT服務器
            print(f"🔗 正在連接MQTT服務器: {self.mqtt_broker}:{self.mqtt_port}")
            self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
            
            # 開始監聽
            print("👂 開始監聽MQTT消息...")
            print("💡 提示: 現在可以操作ESP32按鈕進行錄音測試")
            print("-" * 60)
            
            last_packet_time = time.time()
            packet_timeout = 5.0  # 5秒無數據則認為錄音結束
            
            self.client.loop_start()
            
            while True:
                time.sleep(0.1)
                
                # 檢查錄音會話是否結束
                if (self.audio_packets and 
                    time.time() - last_packet_time > packet_timeout):
                    
                    self.save_recording_summary()
                    self.audio_packets = []
                    self.recording_session = None
                    last_packet_time = time.time()
                
                # 更新最後收包時間
                if self.audio_packets:
                    last_packet_time = time.time()
                    
        except KeyboardInterrupt:
            print("\n\n🛑 用戶中斷測試")
            if self.audio_packets:
                self.save_recording_summary()
        except Exception as e:
            print(f"\n❌ 測試過程中發生錯誤: {e}")
        finally:
            self.client.loop_stop()
            self.client.disconnect()
            print("👋 測試工具已退出")

if __name__ == "__main__":
    tester = ButtonRecordingTester()
    tester.run()
