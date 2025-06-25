#!/usr/bin/env python3
"""
免費 MQTT 服務器連接測試腳本
測試多個公共 MQTT 服務器的可用性和延遲
"""

import paho.mqtt.client as mqtt
import time
import threading
import json

class MqttServerTester:
    def __init__(self):
        self.test_results = {}
        self.test_servers = [
            {
                "name": "Eclipse Mosquitto 公共測試服務器",
                "host": "test.mosquitto.org",
                "port": 1883,
                "description": "Eclipse 基金會官方維護，最穩定"
            },
            {
                "name": "HiveMQ 公共服務器",
                "host": "broker.hivemq.com", 
                "port": 1883,
                "description": "高性能工業級服務器"
            },
            {
                "name": "EMQX 公共服務器",
                "host": "broker.emqx.io",
                "port": 1883,
                "description": "支持多種協議"
            },
            {
                "name": "Flespi 公共服務器",
                "host": "mqtt.flespi.io",
                "port": 1883,
                "description": "需要註冊但功能豐富"
            }
        ]
    
    def test_server(self, server_info):
        """測試單個 MQTT 服務器"""
        print(f"正在測試: {server_info['name']} ({server_info['host']}:{server_info['port']})")
        
        result = {
            "server": server_info['name'],
            "host": server_info['host'],
            "port": server_info['port'],
            "description": server_info['description'],
            "connected": False,
            "connect_time": None,
            "ping_time": None,
            "error": None
        }
        
        try:
            # 創建客戶端
            client = mqtt.Client()
            client.on_connect = self.on_connect
            client.on_disconnect = self.on_disconnect
            client.on_message = self.on_message
            
            # 設置用戶數據
            client.user_data_set(result)
            
            # 記錄連接開始時間
            start_time = time.time()
            
            # 嘗試連接
            client.connect(server_info['host'], server_info['port'], 10)
            client.loop_start()
            
            # 等待連接結果
            timeout = 10
            while timeout > 0 and not result['connected'] and not result['error']:
                time.sleep(0.1)
                timeout -= 0.1
            
            if result['connected']:
                # 測試發布/訂閱
                test_topic = f"test/mqtt_tester/{int(time.time())}"
                
                # 訂閱測試主題
                client.subscribe(test_topic)
                time.sleep(0.5)
                
                # 發布測試消息
                ping_start = time.time()
                client.publish(test_topic, "ping")
                
                # 等待回音
                timeout = 5
                while timeout > 0 and not result['ping_time']:
                    time.sleep(0.1)
                    timeout -= 0.1
                
                if not result['ping_time']:
                    result['ping_time'] = time.time() - ping_start
                
                print(f"✓ {server_info['name']} - 連接成功")
                print(f"  連接時間: {result['connect_time']:.2f}s")
                if result['ping_time']:
                    print(f"  往返時間: {result['ping_time']*1000:.1f}ms")
            else:
                print(f"✗ {server_info['name']} - 連接失敗: {result['error']}")
            
            # 清理連接
            client.loop_stop()
            client.disconnect()
            
        except Exception as e:
            result['error'] = str(e)
            print(f"✗ {server_info['name']} - 連接錯誤: {e}")
        
        self.test_results[server_info['host']] = result
        return result
    
    def on_connect(self, client, userdata, flags, rc):
        """連接回調"""
        if rc == 0:
            userdata['connected'] = True
            userdata['connect_time'] = time.time() - userdata.get('start_time', time.time())
        else:
            userdata['error'] = f"連接失敗，錯誤碼: {rc}"
    
    def on_disconnect(self, client, userdata, rc):
        """斷開連接回調"""
        pass
    
    def on_message(self, client, userdata, msg):
        """消息回調"""
        if msg.payload.decode() == "ping":
            userdata['ping_time'] = time.time() - userdata.get('ping_start', time.time())
    
    def run_all_tests(self):
        """運行所有服務器測試"""
        print("=== 免費 MQTT 服務器測試 ===\n")
        
        for server in self.test_servers:
            self.test_server(server)
            print()
        
        self.print_summary()
    
    def print_summary(self):
        """輸出測試摘要"""
        print("=== 測試結果摘要 ===")
        
        successful_servers = []
        failed_servers = []
        
        for result in self.test_results.values():
            if result['connected']:
                successful_servers.append(result)
            else:
                failed_servers.append(result)
        
        print(f"\n✓ 可用服務器 ({len(successful_servers)}):")
        for result in successful_servers:
            print(f"  {result['server']}")
            print(f"    地址: {result['host']}:{result['port']}")
            print(f"    連接時間: {result['connect_time']:.2f}s")
            if result['ping_time']:
                print(f"    往返時間: {result['ping_time']*1000:.1f}ms")
            print(f"    說明: {result['description']}")
            print()
        
        if failed_servers:
            print(f"✗ 不可用服務器 ({len(failed_servers)}):")
            for result in failed_servers:
                print(f"  {result['server']}: {result['error']}")
            print()
        
        # 推薦最佳服務器
        if successful_servers:
            # 按連接時間排序
            best_server = min(successful_servers, key=lambda x: x['connect_time'] or 999)
            print("🏆 推薦服務器:")
            print(f"  {best_server['server']}")
            print(f"  地址: {best_server['host']}:{best_server['port']}")
            print(f"  理由: {best_server['description']}")
            
            print("\n📝 ESP32 程式碼配置:")
            print(f'const char *mqttServer = "{best_server["host"]}";')
            print(f'const int mqttPort = {best_server["port"]};')
            print('const char *mqttUser = nullptr;')
            print('const char *mqttPassword = nullptr;')

def main():
    print("免費 MQTT 服務器測試工具")
    print("此工具將測試多個公共 MQTT 服務器的可用性")
    print("請確保您的網絡連接正常\n")
    
    tester = MqttServerTester()
    
    try:
        tester.run_all_tests()
    except KeyboardInterrupt:
        print("\n測試被用戶中斷")
    except Exception as e:
        print(f"測試過程中出現錯誤: {e}")

if __name__ == "__main__":
    main()
