# Windows独立频道架构

## 功能说明

Windows平台现在完全支持**同时录音和捕获系统音频**！

### 架构改进

之前，录音和系统音频捕获共享同一个EventChannel，导致它们无法同时工作。现在已经实现为完全独立的两个频道。

### 独立的EventChannel

1. **录音频道** (`recording_stream`):
   - 用于麦克风录音
   - 使用独立的WASAPI接口
   - 独立的线程和状态管理

2. **系统音频频道** (`system_sound_stream`):
   - 用于捕获系统音频输出（loopback recording）
   - 使用完全独立的WASAPI接口
   - 独立的线程和状态管理

### 技术实现

#### 分离的EventChannel注册

```cpp
// 注册录音流频道
auto recording_event_channel =
    std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
        registrar->messenger(), "com.tecmore.flutter_f2f_sound/recording_stream",
        &flutter::StandardMethodCodec::GetInstance());

// 注册系统音频流频道（独立）
auto system_sound_event_channel =
    std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
        registrar->messenger(), "com.tecmore.flutter_f2f_sound/system_sound_stream",
        &flutter::StandardMethodCodec::GetInstance());
```

#### 分离的事件处理器

```cpp
// 录音事件处理器
void OnListenRecording(const flutter::EncodableValue *arguments,
                      std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events);
void OnCancelRecording(const flutter::EncodableValue *arguments);

// 系统音频事件处理器
void OnListenSystemSound(const flutter::EncodableValue *arguments,
                        std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events);
void OnCancelSystemSound(const flutter::EncodableValue *arguments);
```

#### 分离的WASAPI接口

**录音WASAPI接口**:
```cpp
IMMDevice* recording_device_ = nullptr;
IAudioClient* audio_client_ = nullptr;
IAudioCaptureClient* capture_client_ = nullptr;
WAVEFORMATEX *wave_format_ = nullptr;
```

**系统音频WASAPI接口**（完全独立）:
```cpp
IMMDevice* system_sound_device_ = nullptr;
IAudioClient* system_sound_audio_client_ = nullptr;
IAudioCaptureClient* system_sound_capture_client_ = nullptr;
WAVEFORMATEX *system_sound_wave_format_ = nullptr;
```

#### 分离的线程和状态

**录音状态**:
```cpp
std::atomic<bool> is_recording_{false};
std::thread recording_thread_;
std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> recording_event_sink_;
std::mutex recording_mutex_;
```

**系统音频状态**（完全独立）:
```cpp
std::atomic<bool> is_capturing_system_sound_{false};
std::thread system_sound_thread_;
std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> system_sound_event_sink_;
std::mutex system_sound_mutex_;
```

#### 分离的窗口消息

使用不同的窗口消息来区分数据来源:
```cpp
const UINT WM_RECORDING_DATA = WM_USER + 100;      // 录音数据
const UINT WM_SYSTEM_SOUND_DATA = WM_USER + 101;   // 系统音频数据
```

#### 分离的数据处理

```cpp
// 处理录音数据
void ProcessRecordingData(const std::vector<uint8_t>& audio_data) {
  if (recording_event_sink_ && !audio_data.empty()) {
    flutter::EncodableList encodable_audio_data;
    for (uint8_t byte : audio_data) {
      encodable_audio_data.push_back(static_cast<int>(byte));
    }
    recording_event_sink_->Success(flutter::EncodableValue(encodable_audio_data));
  }
}

// 处理系统音频数据（独立）
void ProcessSystemSoundData(const std::vector<uint8_t>& audio_data) {
  if (system_sound_event_sink_ && !audio_data.empty()) {
    flutter::EncodableList encodable_audio_data;
    for (uint8_t byte : audio_data) {
      encodable_audio_data.push_back(static_cast<int>(byte));
    }
    system_sound_event_sink_->Success(flutter::EncodableValue(encodable_audio_data));
  }
}
```

#### 分离的WASAPI初始化方法

**录音初始化**:
```cpp
HRESULT InitializeWASAPI(const AudioConfig& config);
void StartRecordingThread();
void StopRecording();
HRESULT CleanupWASAPI();
```

**系统音频初始化**（完全独立）:
```cpp
HRESULT InitializeSystemSoundWASAPI(const AudioConfig& config);
void StartSystemSoundThread();
void StopSystemSoundCapture();
HRESULT CleanupSystemSoundWASAPI();
```

### 使用示例

现在可以同时进行录音和系统音频捕获：

```dart
// 同时启动麦克风录音和系统音频捕获
await _flutterF2fSoundPlugin.startRecording(
  sampleRate: 44100,
  channels: 1,
);

await _flutterF2fSoundPlugin.startSystemSoundCapture();

// 分别监听两个数据流
_flutterF2fSoundPlugin.recordingStream.listen((data) {
  // 处理麦克风录音数据
  print('麦克风录音: ${data.length} 字节');
});

_flutterF2fSoundPlugin.systemSoundStream.listen((data) {
  // 处理系统音频数据
  print('系统音频: ${data.length} 字节');
});

// 分别停止
await _flutterF2fSoundPlugin.stopRecording();
// 系统音频通过取消监听自动停止
```

### 线程安全架构

使用隐藏的消息窗口实现线程安全的数据传输：

1. **音频线程**: 捕获音频数据，通过`PostMessage`发送到平台线程
2. **平台线程**: 在`WindowProc`中接收消息，根据消息类型路由到正确的处理函数
3. **数据处理**: 在平台线程上将数据发送到Flutter层

```cpp
// 音频线程发送消息
PostMessage(message_window_, WM_RECORDING_DATA, 0, reinterpret_cast<LPARAM>(audio_data));
PostMessage(message_window_, WM_SYSTEM_SOUND_DATA, 0, reinterpret_cast<LPARAM>(audio_data));

// 平台线程接收和处理
if (uMsg == WM_RECORDING_DATA) {
  plugin->ProcessRecordingData(*audio_data);
} else if (uMsg == WM_SYSTEM_SOUND_DATA) {
  plugin->ProcessSystemSoundData(*audio_data);
}
```

### 优势

1. **完全独立**: 录音和系统音频捕获使用完全独立的资源
2. **同时工作**: 可以同时进行麦克风录音和系统音频捕获
3. **线程安全**: 使用Windows消息机制确保线程安全
4. **资源隔离**: 一个功能的故障不会影响另一个功能
5. **灵活控制**: 可以独立启动、停止任何一个功能

### 系统要求

- **操作系统**: Windows 7或更高版本
- **WASAPI**: Windows自带，无需额外安装
- **权限**: 系统音频捕获需要相应的权限

### 应用场景

1. **屏幕录制软件**: 同时录制麦克风和系统声音
2. **游戏直播**: 捕获游戏音频和麦克风解说
3. **音频混音**: 将麦克风和系统音频混合
4. **音频监控**: 同时监控输入和输出音频
5. **语音通话**: 录制双方的声音（麦克风+系统音频）

### 性能特性

- **独立线程**: 录音和系统音频在独立的线程中运行
- **高优先级**: 音频线程使用`THREAD_PRIORITY_TIME_CRITICAL`确保低延迟
- **高效缓冲**: 使用WASAPI的缓冲机制优化性能
- **内存管理**: 使用智能指针和RAII确保资源正确释放

### 调试信息

使用DebugView或Visual Studio输出窗口查看调试信息：

**录音启动**:
```
Starting recording thread...
Audio client started successfully for recording
```

**系统音频启动**:
```
Starting system sound capture thread...
Audio client started successfully for system sound
```

**数据接收**:
```
Received recording data: 4096 bytes
Received system sound data: 8192 bytes
```

### 错误处理

每个频道都有独立的错误处理：

```dart
try {
  await _flutterF2fSoundPlugin.startRecording();
  await _flutterF2fSoundPlugin.startSystemSoundCapture();
} catch (e) {
  print('启动失败: $e');
  // 录音和系统音频的错误是独立的
  // 一个失败不会影响另一个
}
```

### 未来改进

- [ ] 添加音频混音功能
- [ ] 支持更多音频格式
- [ ] 添加音量控制
- [ ] 支持音频效果处理
- [ ] 添加音频格式转换
- [ ] 支持多设备同时录音
