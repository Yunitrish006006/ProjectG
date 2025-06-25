#!/usr/bin/env python3
"""
簡單的 MQTT 音訊測試腳本
用於驗證 ESP32 MQTT 音訊功能是否正常工作
"""

import paho.mqtt.client as mqtt
import json
import time

class SimpleMqttTest:
    def __init__(self, mqtt_host="192.168.137.1", mqtt_port=1883):
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.connected = False
        
        # 統計計數器
        self.stats = {
            'audio': 0,
            'mfcc': 0,
            'mel': 0,
            'features': 0,
            'status': 0
        }
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"✓ 連接成功到 MQTT 服務器 {self.mqtt_host}:{self.mqtt_port}")
            
            # 訂閱所有音訊主題
            topics = [
                "esp32/audio/raw",
                "esp32/audio/mfcc", 
                "esp32/audio/mel",
                "esp32/audio/features",
                "esp32/audio/status"
            ]
            
            for topic in topics:
                client.subscribe(topic)
                print(f"✓ 訂閱: {topic}")
        else:
            print(f"✗ 連接失敗，錯誤碼: {rc}")
    
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        
        try:
            data = json.loads(msg.payload.decode('utf-8'))
            
            if topic == "esp32/audio/raw":
                self.stats['audio'] += 1
                if self.stats['audio'] % 10 == 0:
                    print(f"音訊包: {self.stats['audio']}, 樣本數: {data.get('length', 0)}")
                    
            elif topic == "esp32/audio/mfcc":
                self.stats['mfcc'] += 1
                if self.stats['mfcc'] % 5 == 0:
                    mfcc = data.get('mfcc', [])
                    print(f"MFCC包: {self.stats['mfcc']}, 係數數: {len(mfcc)}")
                    
            elif topic == "esp32/audio/mel":
                self.stats['mel'] += 1
                if self.stats['mel'] % 5 == 0:
                    mel = data.get('mel', [])
                    print(f"梅爾包: {self.stats['mel']}, 濾波器數: {len(mel)}")
                    
            elif topic == "esp32/audio/features":
                self.stats['features'] += 1
                if self.stats['features'] % 5 == 0:
                    centroid = data.get('spectralCentroid', 0)
                    energy = data.get('rmsEnergy', 0)
                    print(f"特徵包: {self.stats['features']}, 重心: {centroid:.1f}, 能量: {energy:.2f}")
                    
            elif topic == "esp32/audio/status":
                self.stats['status'] += 1
                status = data.get('status', 'unknown')
                publishing = data.get('publishing', False)
                print(f"狀態: {status}, 發布中: {publishing}")
                if 'stats' in data:
                    esp_stats = data['stats']
                    print(f"  ESP32統計 - 音訊: {esp_stats.get('audioPackets', 0)}, "
                          f"MFCC: {esp_stats.get('mfccPackets', 0)}")
                    
        except Exception as e:
            print(f"解析消息錯誤: {e}")
            print(f"主題: {topic}, 內容: {msg.payload[:100]}...")
    
    def send_command(self, command):
        """發送控制命令"""
        if self.connected:
            payload = json.dumps({
                "command": command,
                "timestamp": int(time.time() * 1000)
            })
            self.client.publish("esp32/audio/control", payload)
            print(f"發送命令: {command}")
        else:
            print("未連接，無法發送命令")
    
    def connect(self):
        """連接到 MQTT 服務器"""
        try:
            print(f"正在連接到 {self.mqtt_host}:{self.mqtt_port}...")
            self.client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.client.loop_start()
            
            # 等待連接
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.1)
                timeout -= 0.1
                
            return self.connected
        except Exception as e:
            print(f"連接錯誤: {e}")
            return False
    
    def disconnect(self):
        """斷開連接"""
        self.client.loop_stop()
        self.client.disconnect()
        print("已斷開連接")
    
    def print_stats(self):
        """輸出統計信息"""
        print("\n=== 接收統計 ===")
        print(f"音訊包: {self.stats['audio']}")
        print(f"MFCC包: {self.stats['mfcc']}")
        print(f"梅爾包: {self.stats['mel']}")
        print(f"特徵包: {self.stats['features']}")
        print(f"狀態包: {self.stats['status']}")
        print("===============\n")

def main():
    print("ESP32 MQTT 音訊測試")
    print("請確保 ESP32 已連接並運行")
    print()
    
    # 使用免費的公共 MQTT 服務器
    # 您可以選擇以下任一服務器進行測試
    mqtt_servers = [
        "test.mosquitto.org",    # Eclipse Mosquitto (推薦)
        "broker.hivemq.com",     # HiveMQ
        "broker.emqx.io"         # EMQX
    ]
    
    print("可用的免費 MQTT 服務器:")
    for i, server in enumerate(mqtt_servers, 1):
        print(f"{i}. {server}")
    
    # 使用第一個服務器 (Eclipse Mosquitto)
    selected_server = mqtt_servers[0]
    print(f"\n使用服務器: {selected_server}")
    
    test = SimpleMqttTest(mqtt_host=selected_server)
    
    try:
        if not test.connect():
            print("連接失敗，請檢查 MQTT 服務器")
            return
        
        print("連接成功！等待 2 秒...")
        time.sleep(2)
        
        print("發送測試命令...")
        test.send_command("getStatus")
        time.sleep(1)
        
        test.send_command("enableFeatures")
        time.sleep(1)
        
        test.send_command("startPublishing")
        time.sleep(1)
        
        print("開始監控數據接收...")
        print("按 Ctrl+C 停止")
        
        # 主監控循環
        while True:
            time.sleep(10)
            test.print_stats()
            
            # 每 30 秒請求一次狀態
            test.send_command("getStatus")
    
    except KeyboardInterrupt:
        print("\n正在停止...")
        test.send_command("stopPublishing")
        time.sleep(1)
        
    except Exception as e:
        print(f"運行錯誤: {e}")
        
    finally:
        test.disconnect()
        print("測試完成")

if __name__ == "__main__":
    main()
