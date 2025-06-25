#!/usr/bin/env python3
"""
ESP32 MQTT 音訊診斷工具
檢查 ESP32 是否正在發送音訊數據
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

class ESP32MQTTDiagnostic:
    def __init__(self, mqtt_host="broker.emqx.io", mqtt_port=1883):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.connected = False
        
        # 統計計數器
        self.stats = {
            'audio_raw': 0,
            'audio_mfcc': 0,
            'audio_mel': 0,
            'audio_features': 0,
            'audio_status': 0,
            'total_messages': 0,
            'first_message_time': None,
            'last_message_time': None
        }
        
        # MQTT 客戶端設定
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"✅ 已連接到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}")
            
            # 訂閱所有 ESP32 相關主題
            topics = [
                "esp32/+/+",           # 所有 ESP32 主題
                "esp32/audio/+",       # 音訊相關主題
                "ESP32_ProjectG/+",    # 項目專用主題
                "+/audio/+",           # 任何音訊主題
            ]
            
            for topic in topics:
                client.subscribe(topic)
                print(f"📡 訂閱: {topic}")
                
        else:
            print(f"❌ MQTT 連接失敗，錯誤碼: {rc}")
    
    def on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"🔌 MQTT 連接已斷開 (rc: {rc})")
    
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        current_time = time.time()
        
        # 更新統計
        self.stats['total_messages'] += 1
        if self.stats['first_message_time'] is None:
            self.stats['first_message_time'] = current_time
        self.stats['last_message_time'] = current_time
        
        # 分類統計
        if 'audio/raw' in topic:
            self.stats['audio_raw'] += 1
        elif 'audio/mfcc' in topic:
            self.stats['audio_mfcc'] += 1
        elif 'audio/mel' in topic:
            self.stats['audio_mel'] += 1
        elif 'audio/features' in topic:
            self.stats['audio_features'] += 1
        elif 'audio/status' in topic:
            self.stats['audio_status'] += 1
        
        # 顯示訊息詳情
        timestamp = datetime.now().strftime("%H:%M:%S")
        payload_size = len(msg.payload)
        
        print(f"[{timestamp}] 📨 {topic} ({payload_size} bytes)")
        
        # 嘗試解析 JSON 內容
        try:
            if payload_size < 1000:  # 只解析較小的訊息
                data = json.loads(msg.payload.decode('utf-8'))
                if 'timestamp' in data:
                    print(f"    ⏰ ESP32 時間戳: {data['timestamp']}")
                if 'status' in data:
                    print(f"    📊 狀態: {data['status']}")
                if 'audio' in data:
                    audio_len = len(data['audio']) if isinstance(data['audio'], list) else 0
                    print(f"    🎵 音訊樣本: {audio_len}")
        except:
            print(f"    📦 二進位數據或非 JSON 格式")
        
        print()  # 空行分隔
    
    def print_statistics(self):
        print("\n" + "="*60)
        print("📊 MQTT 音訊診斷統計報告")
        print("="*60)
        print(f"連接狀態: {'🟢 已連接' if self.connected else '🔴 未連接'}")
        print(f"服務器: {self.mqtt_host}:{self.mqtt_port}")
        print(f"總訊息數: {self.stats['total_messages']}")
        print()
        print("📈 訊息分類統計:")
        print(f"  🎵 音訊原始數據: {self.stats['audio_raw']}")
        print(f"  🔬 MFCC 特徵: {self.stats['audio_mfcc']}")
        print(f"  📊 梅爾能量: {self.stats['audio_mel']}")
        print(f"  📋 綜合特徵: {self.stats['audio_features']}")
        print(f"  📡 狀態訊息: {self.stats['audio_status']}")
        
        if self.stats['first_message_time']:
            duration = self.stats['last_message_time'] - self.stats['first_message_time']
            rate = self.stats['total_messages'] / duration if duration > 0 else 0
            print(f"\n⏱️ 時間統計:")
            print(f"  首次訊息: {datetime.fromtimestamp(self.stats['first_message_time']).strftime('%H:%M:%S')}")
            print(f"  最後訊息: {datetime.fromtimestamp(self.stats['last_message_time']).strftime('%H:%M:%S')}")
            print(f"  持續時間: {duration:.1f} 秒")
            print(f"  訊息速率: {rate:.1f} 訊息/秒")
        
        print("="*60)
    
    def run_diagnostic(self, duration=30):
        print("🔍 ESP32 MQTT 音訊診斷工具")
        print("="*40)
        print(f"🎯 目標: 檢查 ESP32 音訊數據傳輸")
        print(f"⏰ 監控時間: {duration} 秒")
        print(f"🌐 MQTT 服務器: {self.mqtt_host}")
        print()
        
        try:
            # 連接 MQTT
            print(f"🔌 正在連接到 {self.mqtt_host}...")
            self.client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.client.loop_start()
            
            # 等待連接
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.1)
                timeout -= 0.1
            
            if not self.connected:
                print("❌ 無法連接到 MQTT 服務器")
                return
            
            print(f"✅ 開始監控 {duration} 秒...")
            print("-" * 40)
            
            # 監控指定時間
            start_time = time.time()
            while time.time() - start_time < duration:
                time.sleep(0.1)
                
                # 每 5 秒顯示一次進度
                elapsed = time.time() - start_time
                if int(elapsed) % 5 == 0 and elapsed > 0:
                    remaining = duration - elapsed
                    print(f"⏳ 已監控 {elapsed:.0f}s，剩餘 {remaining:.0f}s，收到 {self.stats['total_messages']} 條訊息")
                    time.sleep(1)  # 避免重複顯示
            
            print("-" * 40)
            print("✅ 監控完成")
            
        except Exception as e:
            print(f"❌ 診斷過程中出錯: {e}")
        
        finally:
            self.client.loop_stop()
            self.client.disconnect()
            self.print_statistics()
            
            # 診斷建議
            self.print_recommendations()
    
    def print_recommendations(self):
        print("\n🎯 診斷建議:")
        print("-" * 30)
        
        if self.stats['total_messages'] == 0:
            print("❌ 未收到任何 MQTT 訊息")
            print("建議:")
            print("  1. 檢查 ESP32 是否正常運行")
            print("  2. 確認 ESP32 WiFi 連接狀態")
            print("  3. 驗證 ESP32 MQTT 連接設定")
            print("  4. 檢查防火牆設定")
            
        elif self.stats['audio_raw'] == 0:
            print("⚠️  收到訊息但無音訊數據")
            print("建議:")
            print("  1. 檢查 ESP32 麥克風是否正常工作")
            print("  2. 確認 MQTT 音訊發布是否啟用")
            print("  3. 檢查 I2S 麥克風設定")
            
        else:
            print("✅ 系統正常，有音訊數據傳輸")
            rate = self.stats['audio_raw'] / 30 if self.stats['audio_raw'] > 0 else 0
            print(f"📊 音訊數據速率: {rate:.1f} 包/秒")
            
            if rate < 5:
                print("⚠️  音訊數據速率較低，可能影響品質")

def main():
    # 可以測試多個 MQTT 服務器
    servers = ["broker.emqx.io", "test.mosquitto.org"]
    
    for server in servers:
        print(f"\n🌐 測試服務器: {server}")
        diagnostic = ESP32MQTTDiagnostic(mqtt_host=server)
        diagnostic.run_diagnostic(duration=15)  # 每個服務器監控 15 秒
        print("\n" + "="*60 + "\n")

if __name__ == "__main__":
    main()
