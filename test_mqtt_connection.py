#!/usr/bin/env python3
"""
ESP32 MQTT 連接測試腳本
用於測試 ESP32 是否能成功連接到 MQTT 服務器
"""

import paho.mqtt.client as mqtt
import time
import json

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ 成功連接到 MQTT 服務器")
        client.subscribe("esp32/audio/+")
        client.subscribe("esp32/+/status")
    else:
        print(f"❌ MQTT 連接失敗，錯誤碼: {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = msg.payload.decode('utf-8')
        print(f"📨 收到訊息 - 主題: {topic}")
        
        if topic.endswith('/status'):
            try:
                data = json.loads(payload)
                print(f"   狀態: {data}")
            except:
                print(f"   內容: {payload}")
        else:
            print(f"   大小: {len(msg.payload)} bytes")
    except Exception as e:
        print(f"   二進位數據大小: {len(msg.payload)} bytes")

def on_disconnect(client, userdata, rc):
    print(f"🔌 MQTT 連接已斷開，錯誤碼: {rc}")

def main():
    print("🔍 ESP32 MQTT 連接測試")
    print("=" * 40)
    
    # 測試多個 MQTT 服務器
    servers = [
        "broker.emqx.io",
        "test.mosquitto.org", 
        "broker.hivemq.com"
    ]
    
    for server in servers:
        print(f"\n🌐 測試服務器: {server}")
        client = mqtt.Client()
        client.on_connect = on_connect
        client.on_message = on_message
        client.on_disconnect = on_disconnect
        
        try:
            print(f"   正在連接...")
            client.connect(server, 1883, 60)
            client.loop_start()
            
            # 等待連接
            time.sleep(3)
            
            # 發送測試訊息
            test_msg = {
                "timestamp": int(time.time() * 1000),
                "test": "Python 連接測試",
                "server": server
            }
            
            client.publish("esp32/test/connection", json.dumps(test_msg))
            print(f"   📤 已發送測試訊息")
            
            # 等待接收訊息
            time.sleep(2)
            
            client.loop_stop()
            client.disconnect()
            print(f"   ✅ {server} 測試完成")
            
        except Exception as e:
            print(f"   ❌ {server} 連接失敗: {e}")
    
    print("\n" + "=" * 40)
    print("🎯 建議：")
    print("1. 確認 ESP32 WiFi 連接正常")
    print("2. 檢查防火牆設定")
    print("3. 嘗試使用不同的 MQTT 服務器")
    print("4. 驗證 ESP32 程式碼中的 MQTT 設定")

if __name__ == "__main__":
    main()
