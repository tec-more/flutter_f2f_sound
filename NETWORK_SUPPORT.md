# Windows网络音频支持

## 功能说明

Windows平台现在支持从网络URL播放音频文件！

### 支持的音频源

1. **本地文件路径**：
   ```
   C:\\path\\to\\audio.wav
   ```

2. **网络URL**：
   ```
   https://example.com/audio.wav
   http://example.com/audio.wav
   ```

### 工作原理

当检测到URL（`http://` 或 `https://`）时：

1. 使用WinHTTP API自动下载音频文件
2. 保存到Windows临时文件夹（`%TEMP%`）
3. 使用WASAPI播放下载的文件
4. 播放完成后临时文件会被系统自动清理

### 使用示例

```dart
// 本地文件
await _flutterF2fSoundPlugin.play(
  path: 'C:\\Windows\\Media\\Alarm01.wav',
  volume: 1.0,
  loop: false,
);

// 网络URL
await _flutterF2fSoundPlugin.play(
  path: 'https://example.com/audio.wav',
  volume: 1.0,
  loop: false,
);
```

### 技术细节

- **下载API**: WinHTTP (Windows HTTP API)
- **音频格式**: 支持WAV格式（其他格式需要格式转换）
- **临时文件**: 保存在系统临时文件夹
- **缓冲**: 8KB缓冲区进行分块下载
- **进度跟踪**: 每下载100KB输出调试信息

### 错误处理

如果下载失败，会返回错误码 `DOWNLOAD_FAILED`：

```dart
try {
  await _flutterF2fSoundPlugin.play(
    path: 'https://example.com/audio.wav',
    volume: 1.0,
    loop: false,
  );
} catch (e) {
  print('播放失败: $e');
  // 可能的错误原因：
  // - 网络连接失败
  // - URL不存在
  // - 服务器返回非200状态码
  // - 文件格式不支持
}
```

### 调试

使用DebugView或Visual Studio输出窗口查看调试信息：

```
Downloading URL: https://example.com/audio.wav to: C:\Users\...\audio123.wav
Connecting to: example.com:443
HTTP 200 OK, starting download...
Content length: 123456 bytes
Downloaded: 102400 bytes
Downloaded: 204800 bytes
Download complete: 123456 bytes
```

### 限制

1. **格式限制**: 目前主要支持WAV格式
2. **下载大小**: 受限于可用磁盘空间
3. **网络要求**: 需要稳定的网络连接
4. **超时**: 使用系统默认的HTTP超时设置

### 未来改进

- [ ] 支持流式播放（无需完全下载）
- [ ] 支持更多音频格式（MP3, AAC等）
- [ ] 添加下载进度回调
- [ ] 支持断点续传
- [ ] 缓存机制避免重复下载
