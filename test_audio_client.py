#!/usr/bin/env python3
"""
ESP32 音訊流測試客戶端
用於測試音訊流傳遞和特徵提取功能
"""

import asyncio
import websockets
import json
import matplotlib.pyplot as plt
import numpy as np
from collections import deque
import time

class ESP32AudioClient:
    def __init__(self, host="192.168.137.229", port=81):
        self.host = host
        self.port = port
        self.websocket = None
        self.running = False
        
        # 數據緩衝區
        self.audio_buffer = deque(maxlen=5000)
        self.mfcc_buffer = deque(maxlen=100)
        self.mel_buffer = deque(maxlen=100)
        self.feature_history = {
            'spectralCentroid': deque(maxlen=200),
            'spectralBandwidth': deque(maxlen=200),
            'zeroCrossingRate': deque(maxlen=200),
            'rmsEnergy': deque(maxlen=200)
        }
        
        # 統計信息
        self.stats = {
            'audio_packets': 0,
            'feature_packets': 0,
            'start_time': time.time()
        }
        
    async def connect(self):
        """連接到 ESP32 WebSocket"""
        uri = f"ws://{self.host}:{self.port}"
        print(f"正在連接到 {uri}...")
        
        try:
            self.websocket = await websockets.connect(uri)
            self.running = True
            print("✓ 已連接到 ESP32")
            return True
        except Exception as e:
            print(f"✗ 連接失敗: {e}")
            return False
    
    async def disconnect(self):
        """斷開連接"""
        self.running = False
        if self.websocket:
            await self.websocket.close()
            print("已斷開連接")
    
    async def subscribe_audio(self):
        """訂閱音訊數據"""
        if self.websocket:
            message = json.dumps({
                "command": "subscribe",
                "dataType": "audio"
            })
            await self.websocket.send(message)
            print("已訂閱音訊數據")
    
    async def subscribe_features(self):
        """訂閱特徵數據"""
        if self.websocket:
            message = json.dumps({
                "command": "subscribe",
                "dataType": "features"
            })
            await self.websocket.send(message)
            print("已訂閱特徵數據")
    
    async def send_ping(self):
        """發送心跳包"""
        if self.websocket:
            message = json.dumps({"command": "ping"})
            await self.websocket.send(message)
    
    async def listen(self):
        """監聽來自 ESP32 的消息"""
        try:
            async for message in self.websocket:
                data = json.loads(message)
                await self.handle_message(data)
        except websockets.exceptions.ConnectionClosed:
            print("連接已關閉")
            self.running = False
        except Exception as e:
            print(f"監聽錯誤: {e}")
    
    async def handle_message(self, data):
        """處理收到的消息"""
        message_type = data.get('type', '')
        
        if message_type == 'welcome':
            print(f"收到歡迎消息，客戶端 ID: {data.get('clientId', 'unknown')}")
            
        elif message_type == 'audio':
            await self.handle_audio_data(data)
            
        elif message_type == 'features':
            await self.handle_feature_data(data)
            
        elif message_type == 'pong':
            print("收到 pong 回應")
    
    async def handle_audio_data(self, data):
        """處理音訊數據"""
        audio_samples = data.get('data', [])
        timestamp = data.get('timestamp', 0)
        sequence = data.get('sequence', 0)
        
        # 添加到緩衝區
        self.audio_buffer.extend(audio_samples)
        self.stats['audio_packets'] += 1
        
        if self.stats['audio_packets'] % 50 == 0:
            print(f"已接收 {self.stats['audio_packets']} 個音訊包")
    
    async def handle_feature_data(self, data):
        """處理特徵數據"""
        timestamp = data.get('timestamp', 0)
        
        # MFCC 係數
        mfcc = data.get('mfcc', [])
        if mfcc:
            self.mfcc_buffer.append(mfcc)
        
        # 梅爾能量
        mel = data.get('mel', [])
        if mel:
            self.mel_buffer.append(mel)
        
        # 額外特徵
        self.feature_history['spectralCentroid'].append(data.get('spectralCentroid', 0))
        self.feature_history['spectralBandwidth'].append(data.get('spectralBandwidth', 0))
        self.feature_history['zeroCrossingRate'].append(data.get('zeroCrossingRate', 0))
        self.feature_history['rmsEnergy'].append(data.get('rmsEnergy', 0))
        
        self.stats['feature_packets'] += 1
        
        if self.stats['feature_packets'] % 10 == 0:
            print(f"已接收 {self.stats['feature_packets']} 個特徵包")
            self.print_latest_features(data)
    
    def print_latest_features(self, data):
        """列印最新特徵"""
        print("--- 最新特徵 ---")
        print(f"頻譜重心: {data.get('spectralCentroid', 0):.2f} Hz")
        print(f"頻譜帶寬: {data.get('spectralBandwidth', 0):.2f} Hz")
        print(f"過零率: {data.get('zeroCrossingRate', 0):.4f}")
        print(f"RMS能量: {data.get('rmsEnergy', 0):.4f}")
        print("-------------")
    
    def plot_audio_waveform(self):
        """繪製音訊波形"""
        if len(self.audio_buffer) > 0:
            plt.figure(figsize=(12, 4))
            audio_data = list(self.audio_buffer)[-1000:]  # 最近 1000 個樣本
            time_axis = np.arange(len(audio_data)) / 16000  # 假設 16kHz 採樣率
            
            plt.plot(time_axis, audio_data)
            plt.title('音訊波形 (最近 1000 樣本)')
            plt.xlabel('時間 (秒)')
            plt.ylabel('振幅')
            plt.grid(True)
            plt.tight_layout()
            plt.show()
    
    def plot_mfcc_spectrogram(self):
        """繪製 MFCC 頻譜圖"""
        if len(self.mfcc_buffer) > 0:
            mfcc_matrix = np.array(list(self.mfcc_buffer)).T
            
            plt.figure(figsize=(12, 6))
            plt.imshow(mfcc_matrix, aspect='auto', origin='lower', cmap='viridis')
            plt.colorbar(label='MFCC 係數值')
            plt.title('MFCC 頻譜圖')
            plt.xlabel('時間幀')
            plt.ylabel('MFCC 係數')
            plt.tight_layout()
            plt.show()
    
    def plot_mel_spectrogram(self):
        """繪製梅爾頻譜圖"""
        if len(self.mel_buffer) > 0:
            mel_matrix = np.array(list(self.mel_buffer)).T
            
            plt.figure(figsize=(12, 6))
            plt.imshow(mel_matrix, aspect='auto', origin='lower', cmap='plasma')
            plt.colorbar(label='梅爾能量 (log)')
            plt.title('梅爾頻譜圖')
            plt.xlabel('時間幀')
            plt.ylabel('梅爾濾波器')
            plt.tight_layout()
            plt.show()
    
    def plot_features_timeline(self):
        """繪製特徵時間線"""
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        
        # 頻譜重心
        if self.feature_history['spectralCentroid']:
            axes[0, 0].plot(list(self.feature_history['spectralCentroid']))
            axes[0, 0].set_title('頻譜重心')
            axes[0, 0].set_ylabel('頻率 (Hz)')
        
        # 頻譜帶寬
        if self.feature_history['spectralBandwidth']:
            axes[0, 1].plot(list(self.feature_history['spectralBandwidth']))
            axes[0, 1].set_title('頻譜帶寬')
            axes[0, 1].set_ylabel('頻率 (Hz)')
        
        # 過零率
        if self.feature_history['zeroCrossingRate']:
            axes[1, 0].plot(list(self.feature_history['zeroCrossingRate']))
            axes[1, 0].set_title('過零率')
            axes[1, 0].set_ylabel('比率')
        
        # RMS 能量
        if self.feature_history['rmsEnergy']:
            axes[1, 1].plot(list(self.feature_history['rmsEnergy']))
            axes[1, 1].set_title('RMS 能量')
            axes[1, 1].set_ylabel('能量')
        
        for ax in axes.flat:
            ax.grid(True)
            ax.set_xlabel('時間幀')
        
        plt.tight_layout()
        plt.show()
    
    def print_statistics(self):
        """列印統計信息"""
        runtime = time.time() - self.stats['start_time']
        print("\n=== 統計信息 ===")
        print(f"運行時間: {runtime:.1f} 秒")
        print(f"音訊包: {self.stats['audio_packets']}")
        print(f"特徵包: {self.stats['feature_packets']}")
        print(f"音訊包速率: {self.stats['audio_packets']/runtime:.2f} pps")
        print(f"特徵包速率: {self.stats['feature_packets']/runtime:.2f} pps")
        print("===============")

async def main():
    # 創建客戶端 (請修改為您的 ESP32 IP 地址)
    client = ESP32AudioClient("192.168.137.229", 81)
    
    # 連接到 ESP32
    if not await client.connect():
        return
    
    try:
        # 訂閱數據
        await client.subscribe_audio()
        await client.subscribe_features()
        
        # 創建監聽任務
        listen_task = asyncio.create_task(client.listen())
        
        # 定期發送心跳
        async def heartbeat():
            while client.running:
                await client.send_ping()
                await asyncio.sleep(10)
        
        heartbeat_task = asyncio.create_task(heartbeat())
        
        print("開始接收數據... (按 Ctrl+C 停止)")
        print("收集數據 30 秒後將顯示圖表...")
        
        # 運行 30 秒
        await asyncio.sleep(30)
        
        # 取消任務
        listen_task.cancel()
        heartbeat_task.cancel()
        
        # 顯示統計信息
        client.print_statistics()
        
        # 繪製圖表
        print("正在生成圖表...")
        client.plot_audio_waveform()
        client.plot_mfcc_spectrogram()
        client.plot_mel_spectrogram()
        client.plot_features_timeline()
        
    except KeyboardInterrupt:
        print("\n用戶中斷")
    except Exception as e:
        print(f"錯誤: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n程式已退出")
