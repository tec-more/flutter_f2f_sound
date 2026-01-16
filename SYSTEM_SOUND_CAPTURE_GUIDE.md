# 系统音频捕获测试指南

## 功能说明

Windows平台支持捕获系统音频输出（loopback recording），现在**已启用静音过滤**，只会发送实际的音频数据。

## 工作原理

系统音频捕获使用WASAPI的loopback模式，可以捕获系统音频引擎的输出：

- **有音频播放时**：捕获实际的音频数据并发送到Flutter
- **无音频播放时**：系统返回静音数据，我们**自动过滤掉**这些数据

## 测试步骤

### 1. 启动系统音频捕获

1. 运行应用程序
2. 点击"Start System Sound Capture"按钮
3. 等待几秒钟让系统音频初始化

### 2. 播放系统音频

**方法A：使用应用内播放器**
1. 在音频路径输入框中输入音频文件路径或URL
2. 点击"Play"按钮播放音频
3. 观察系统音频捕获的数据计数器

**方法B：使用系统其他应用**
1. 在浏览器中播放视频/音乐（YouTube、网易云音乐等）
2. 或者在其他应用中播放音频（VLC、Windows Media Player等）
3. 观察系统音频捕获的数据计数器

### 3. 观察数据接收

- **没有音频播放时**：数据计数器保持不变（静音被过滤）
- **播放音频时**：数据计数器开始增长（接收到实际音频数据）

## 调试输出

使用DebugView查看详细的调试信息：

### 下载DebugView
- 访问：https://docs.microsoft.com/en-us/sysinternals/downloads/debugview
- 下载并运行DebugView
- 启用"Capture" > "Capture Kernel"

### 日志输出说明

**初始化日志**：
```
Initializing system sound WASAPI...
Got default render endpoint successfully
Activated audio client successfully
Got mix format: 48000 Hz, 2 channels, 32 bits
Initialized audio client for loopback successfully
Buffer frame count: 48000
System sound WASAPI initialization complete
Starting system sound capture thread...
System sound audio client started successfully
```

**数据捕获日志**（每100个包）：
```
System sound: packet 100, size: 4096 bytes, flags: 0x00000002, silence: 1
System sound: skipping silent packet 100
```
这表示第100个包是静音数据，已被跳过。

```
System sound: packet 200, size: 4096 bytes, flags: 0x00000000, silence: 0
System sound: sent non-silent packet 200, size: 4096 bytes
```
这表示第200个包是实际音频数据，已发送到Flutter。

**Flutter层日志**（每50个包）：
```
ProcessSystemSoundData: Sending packet 50 to Flutter, size: 4096 bytes
```
这表示数据已成功发送到Flutter层。

## 预期行为

### 正常情况

✅ **启动捕获后立即**：
- 没有音频播放时，数据计数器保持为0
- DebugView显示"skipping silent packet"

✅ **播放音频时**：
- 数据计数器开始增长
- DebugView显示"sent non-silent packet"
- Flutter层收到数据

✅ **停止播放后**：
- 数据计数器停止增长
- DebugView再次显示"skipping silent packet"

### 可能的问题

❌ **问题1：初始化失败**
- 错误：`SYSTEM_SOUND_CAPTURE_ERROR`
- 原因：音频设备不支持或被其他应用占用
- 解决：关闭其他音频应用，重试

❌ **问题2：收不到数据**
- 现象：播放音频但数据计数器不增长
- 检查：DebugView中是否有"sent non-silent packet"日志
- 原因：可能是EventChannel未正确连接
- 解决：检查Flutter代码中的EventChannel监听

❌ **问题3：一直收到静音数据**
- 现象：即使播放音频也只收到静音
- 原因：音频设备配置问题
- 解决：检查系统默认音频输出设备

## 技术细节

### 音频格式

系统音频捕获使用设备的mix format，通常是：
- 采样率：48000 Hz
- 声道：2（立体声）
- 位深度：32-bit float
- 数据格式：IEEE Float

### Loopback录制特点

1. **捕获所有系统音频**：包括所有应用的声音输出
2. **混合格式**：使用音频引擎的mix format
3. **静音处理**：无音频时返回静音标志
4. **实时性**：低延迟，适合实时处理

### 静音过滤逻辑

```cpp
bool is_silence = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
if (!is_silence) {
    // 只发送非静音数据
    PostMessage(message_window_, WM_SYSTEM_SOUND_DATA, 0, ...);
} else {
    // 跳过静音数据
}
```

## 与麦克风录音的区别

| 特性 | 麦克风录音 | 系统音频捕获 |
|------|-----------|-------------|
| 数据源 | 麦克风输入 | 系统音频输出 |
| WASAPI标志 | 无 | AUDCLNT_STREAMFLAGS_LOOPBACK |
| 设备类型 | eCapture（录制设备） | eRender（播放设备） |
| 音频格式 | 自定义格式 | 设备mix format |
| 静音处理 | 通常无静音 | 会有静音数据 |
| 同时工作 | ✅ 可以 | ✅ 可以 |

## 性能考虑

- **数据量**：48kHz立体声32-bit float约等于384KB/s
- **CPU使用**：约1-3%（取决于系统配置）
- **内存使用**：约10-20MB（缓冲区）
- **延迟**：约10-50ms（取决于缓冲区大小）

## 常见应用场景

1. **屏幕录制软件**：录制系统声音和麦克风
2. **游戏直播**：捕获游戏音频输出
3. **音频录制**：录制系统播放的音乐
4. **语音通话**：录制双方的对话
5. **音频分析**：分析系统音频输出

## 示例代码

### Dart端监听系统音频

```dart
// 开始监听系统音频流
_flutterF2fSoundPlugin.systemSoundStream.listen((audioData) {
  // audioData是List<int>格式的音频数据
  // 格式：48kHz, 立体声, 32-bit float
  print('收到系统音频数据: ${audioData.length} 字节');

  // 处理音频数据...
});

// 启动系统音频捕获
await _flutterF2fSoundPlugin.startSystemSoundCapture();
```

### 同时录音和捕获系统音频

```dart
// 启动麦克风录音
await _flutterF2fSoundPlugin.startRecording(
  sampleRate: 44100,
  channels: 1,
);

// 启动系统音频捕获
await _flutterF2fSoundPlugin.startSystemSoundCapture();

// 分别监听两个数据流
_flutterF2fSoundPlugin.recordingStream.listen((micData) {
  print('麦克风: ${micData.length} 字节');
});

_flutterF2fSoundPlugin.systemSoundStream.listen((systemData) {
  print('系统音频: ${systemData.length} 字节');
});

// 可以混音或分别处理两个音频流
```

## 故障排除

### 检查EventChannel连接

如果数据计数器不增长，检查：

1. **EventChannel名称是否正确**：
   ```dart
   static const systemSoundStream =
       EventChannel('com.tecmore.flutter_f2f_sound/system_sound_stream');
   ```

2. **是否正确监听**：
   ```dart
   _systemSoundCaptureStreamSubscription =
       _flutterF2fSoundPlugin.systemSoundStream.listen(...)
   ```

3. **是否在dispose中取消订阅**：
   ```dart
   _systemSoundCaptureStreamSubscription?.cancel();
   ```

### 检查WASAPI初始化

查看DebugView日志，确认：
- ✅ "Got default render endpoint successfully"
- ✅ "Initialized audio client for loopback successfully"
- ✅ "System sound audio client started successfully"
- ✅ "System sound: packet X, size: Y bytes"

如果看到错误，检查错误码并查找对应的问题。

## 下一步改进

可能的未来改进：

- [ ] 添加音频格式转换（转换为PCM 16-bit）
- [ ] 支持选择特定音频设备
- [ ] 添加音量控制
- [ ] 支持音频效果处理
- [ ] 添加音频可视化数据
- [ ] 支持多通道混音
