# Windows MP3音频支持

## 功能说明

Windows平台现在完全支持**MP3音频播放**！

### 支持的音频格式

1. **WAV格式** (PCM) - 直接解析WAV文件
2. **MP3格式** - 使用Windows Media Foundation自动解码
3. **网络音频** - 支持从URL下载MP3和WAV文件

### 技术实现

- **WAV解析**: 使用自定义WAV文件解析器
- **MP3解码**: 使用Windows Media Foundation (WMF)进行解码
- **格式转换**: 自动转换为WASAPI兼容的PCM格式

### 使用示例

```dart
// 播放WAV文件
await plugin.play(
  path: 'C:\\path\\to\\audio.wav',
  volume: 1.0,
  loop: false,
);

// 播放MP3文件
await plugin.play(
  path: 'C:\\path\\to\\audio.mp3',
  volume: 1.0,
  loop: false,
);

// 播放网络MP3
await plugin.play(
  path: 'https://example.com/audio.mp3',
  volume: 1.0,
  loop: false,
);
```

### 支持的其他格式

Windows Media Foundation还支持以下格式（未完全测试，但理论上可用）：

- **WMA** (Windows Media Audio)
- **AAC** (Advanced Audio Coding)
- **FLAC** (Free Lossless Audio Codec)
- **ALAC** (Apple Lossless Audio Codec)
- **OGG** (需要安装相应的Media Foundation编解码器)

### 工作流程

#### MP3播放流程

```
MP3文件 → 检测文件扩展名 → Media Foundation解码器 → PCM数据 → WASAPI播放
```

#### WAV播放流程

```
WAV文件 → WAV解析器 → PCM数据 → WASAPI播放
```

### 调试信息

使用DebugView或Visual Studio输出窗口查看调试信息：

**MP3解码**:
```
Detected MP3 file, using Media Foundation decoder
Reading audio file with Media Foundation: C:\path\to\audio.mp3
Audio format: 44100 Hz, 2 channels, 16 bits
Read 1234567 bytes of audio data
Audio data ready for playback, size: 1234567 bytes
```

**WAV解析**:
```
Using WAV parser for file
Successfully read audio file, size: 123456 bytes
```

### 性能特性

- **硬件加速**: Media Foundation使用硬件加速解码（如果可用）
- **内存效率**: 直接解码到内存，无需中间文件
- **流式处理**: 支持大文件，边读边播
- **格式自动检测**: 根据文件扩展名自动选择解码器

### 系统要求

- **操作系统**: Windows 7或更高版本
- **Media Foundation**: Windows 7+自带，无需额外安装
- **编解码器**:
  - MP3: Windows自带支持
  - 其他格式: 可能需要安装相应的Media Foundation编解码器

### 错误处理

如果遇到解码错误：

```dart
try {
  await plugin.play(
    path: 'audio.mp3',
    volume: 1.0,
    loop: false,
  );
} catch (e) {
  print('播放失败: $e');
  // 可能的错误原因：
  // - 文件格式不支持
  // - 文件损坏
  // - 缺少必要的编解码器
  // - Media Foundation初始化失败
}
```

### 常见问题

**Q: 为什么某些MP3文件无法播放？**
A: 可能是MP3编码格式不被支持，或者文件损坏。请尝试使用标准的MP3编码器重新编码。

**Q: 能否播放其他压缩格式（如AAC、FLAC）？**
A: 理论上可以，但需要系统安装相应的Media Foundation编解码器。默认只保证MP3和WAV的完全支持。

**Q: MP3解码会影响性能吗？**
A: Media Foundation使用硬件加速，性能影响很小。对于高比特率或复杂编码的文件，可能会有轻微的性能开销。

**Q: 支持哪些采样率和位深度？**
A: Media Foundation自动处理所有标准格式（8kHz - 192kHz，8-bit - 32-bit）。最终会转换为系统音频引擎支持的格式。

### API参考

新增方法：

```cpp
// 检测文件是否为MP3
bool IsMP3(const std::string& path);

// 使用Media Foundation读取音频文件
HRESULT ReadAudioFileWithMF(const std::string& path,
                            std::vector<uint8_t>& audio_data,
                            WAVEFORMATEX** format);
```

### 未来改进

- [ ] 添加格式自动检测（不仅基于文件扩展名）
- [ ] 支持播放列表（多个文件连续播放）
- [ ] 添加音频元数据读取（标题、艺术家、专辑等）
- [ ] 支持均衡器
- [ ] 支持音效（淡入淡出、交叉淡化等）
- [ ] 添加更多格式支持（OGG、FLAC等）
