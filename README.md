# flutter_f2f_sound

A powerful cross-platform audio plugin for Flutter that provides comprehensive audio playback, recording, and streaming capabilities across multiple platforms.

## Features

- 🎵 **Audio Playback** - Play audio from local files or network URLs (MP3, WAV, OGG, FLAC, etc.)
- 🎙️ **Audio Recording** - Record audio from microphone with real-time streaming
- 🖥️ **System Audio Capture** - Capture system audio output (Windows/Linux)
- 🎚️ **Volume Control** - Real-time volume adjustment (0.0 to 1.0)
- ⏯️ **Playback Controls** - Play, pause, stop, and resume audio
- 🔄 **Loop Playback** - Support for looping audio playback
- 📍 **Seeking Support** - Seek to specific positions (Linux)
- 🎚️ **Format Conversion** - Automatic sample rate conversion across all platforms
- 📊 **Playback Information** - Get current position and duration of audio
- 🌐 **Network Streaming** - Non-blocking async playback from network URLs
- 📱 **Cross-Platform** - Android, iOS, Windows, Linux

## Platform Support

| Platform | Playback | Recording | System Audio | Streaming | Format Support | Audio Engine |
|----------|----------|-----------|--------------|-----------|----------------|--------------|
| Android  | ✅ | ✅ | ❌ | ✅ | MP3, WAV, AAC, FLAC, OGG | MediaPlayer |
| iOS      | ✅ | ❌ | ❌ | ✅ | MP3, WAV, AAC, ALAC, M4A | AVFoundation |
| Windows  | ✅ | ✅ | ✅ | ❌ | MP3, WAV, WMA, AAC | Media Foundation + WASAPI |
| Linux    | ✅ | ✅ | ✅ | ✅ | MP3, WAV, OGG, FLAC, OPUS | PulseAudio + libsndfile |

## Installation

Add `flutter_f2f_sound` to your `pubspec.yaml` file:

```yaml
dependencies:
  flutter_f2f_sound:
    path: ./flutter_f2f_sound
```

Or install from pub.dev (when published):

```yaml
dependencies:
  flutter_f2f_sound: ^1.0.0
```

Then run:

```bash
flutter pub get
```

## Platform-Specific Setup

### Android

No special setup required. The plugin uses `MediaPlayer` which is built into Android.

**Permissions** (optional, for network audio):
```xml
<uses-permission android:name="android.permission.INTERNET" />
```

### iOS

No special setup required. The plugin uses `AVFoundation` which is built into iOS.

### Windows

No special setup required. The plugin uses `Media Foundation` and `WASAPI` which are built into Windows.

### Linux

**Required System Libraries:**

```bash
# Ubuntu/Debian
sudo apt-get install libpulse-dev libcurl4-openssl-dev libsndfile1-dev libsamplerate0-dev

# Fedora/RHEL
sudo dnf install pulseaudio-devel libcurl-devel libsndfile-devel libsamplerate-devel

# Arch Linux
sudo pacman -S pulseaudio libsndfile curl samplerate
```

## Usage

Import the package in your Dart code:

```dart
import 'package:flutter_f2f_sound/flutter_f2f_sound.dart';
```

Create an instance of the plugin:

```dart
final f2fSound = FlutterF2fSound();
```

### Basic Audio Playback

```dart
// Play audio from a local file
await f2fSound.play(
  path: '/path/to/audio/file.mp3',
  volume: 1.0,
  loop: false,
);

// Play audio from a network URL (non-blocking)
await f2fSound.play(
  path: 'https://example.com/audio/file.mp3',
  volume: 0.8,
  loop: true,
);

// Pause audio
await f2fSound.pause();

// Resume audio
await f2fSound.resume();

// Stop audio
await f2fSound.stop();

// Set volume (0.0 to 1.0)
await f2fSound.setVolume(0.5);

// Check if audio is playing
bool isPlaying = await f2fSound.isPlaying();

// Get current position in seconds
double position = await f2fSound.getCurrentPosition();

// Get duration of audio file in seconds
double duration = await f2fSound.getDuration('/path/to/audio/file.mp3');
```

### Audio Recording

```dart
// Start recording and get audio stream
final recordingStream = f2fSound.startRecording();

final subscription = recordingStream.listen(
  (audioData) {
    // audioData contains raw PCM samples (List<int>)
    print('Received ${audioData.length} bytes of audio data');
    // Process or save audio data as needed
  },
);

// Stop recording
await f2fSound.stopRecording();
subscription.cancel();
```

### System Audio Capture (Windows/Linux)

```dart
// Start capturing system audio output
final systemSoundStream = f2fSound.startSystemSoundCapture();

final subscription = systemSoundStream.listen(
  (audioData) {
    // audioData contains raw PCM samples from system audio
    print('Received ${audioData.length} bytes of system audio');
    // Process or save system audio as needed
  },
);

// Stop capturing (automatically stops when cancelled)
subscription.cancel();
await f2fSound.stopRecording();
```

### Audio Playback Streaming

```dart
// Stream audio data while playing (for processing)
final playbackStream = f2fSound.startPlaybackStream('/path/to/audio.wav');

final subscription = playbackStream.listen(
  (audioData) {
    // audioData contains raw PCM samples (16-bit)
    print('Received ${audioData.length} bytes');
  },
);
```

## API Reference

### Methods

#### `Future<String?> getPlatformVersion()`
Get the platform version string.

#### `Future<void> play({required String path, double volume = 1.0, bool loop = false})`
Play audio from the given path.

**Parameters:**
- `path`: The path to the audio file (local file path or network URL)
- `volume`: The volume level (0.0 to 1.0, default: 1.0)
- `loop`: Whether to loop the audio playback (default: false)

**Supported Path Formats:**
- **Local files**: `/path/to/audio.mp3` (Linux/Android), `C:\path\to\audio.mp3` (Windows)
- **Network URLs**: `https://example.com/audio.mp3`, `http://example.com/audio.wav`

#### `Future<void> pause()`
Pause the currently playing audio.

#### `Future<void> resume()`
Resume playback of paused audio.

#### `Future<void> stop()`
Stop the currently playing audio.

#### `Future<void> setVolume(double volume)`
Set the volume of the currently playing audio.

**Parameters:**
- `volume`: The volume level (0.0 to 1.0)

#### `Future<bool> isPlaying()`
Check if audio is currently playing.

**Returns:** `true` if audio is playing, `false` otherwise

#### `Future<double> getCurrentPosition()`
Get the current playback position in seconds.

**Returns:** Current position in seconds (0.0 to duration)

#### `Future<double> getDuration(String path)`
Get the duration of the audio file in seconds.

**Parameters:**
- `path`: The path to the audio file (local file path or network URL)

**Returns:** Duration in seconds, or 0.0 for network URLs (not immediately available)

#### `Stream<List<int>> startRecording()`
Start audio recording and get a stream of recorded audio data.

**Returns:** Stream of audio data as `List<int>` (PCM samples)

#### `Future<void> stopRecording()`
Stop audio recording.

#### `Stream<List<int>> startPlaybackStream(String path)`
Start audio playback and get a stream of playback audio data.

**Parameters:**
- `path`: The path to the audio file

**Returns:** Stream of audio data as `List<int>` (PCM samples)

#### `Stream<List<int>> startSystemSoundCapture()`
Start system sound capture and get a stream of captured audio data.

**Returns:** Stream of audio data as `List<int>` (PCM samples)

**Note:** Only available on Windows and Linux

## Technical Details

### Audio Format Support

**Platform-specific capabilities:**

| Platform | MP3 | WAV | OGG | FLAC | AAC | Network | Sample Rate Conversion |
|----------|-----|-----|-----|------|-----|---------|----------------------|
| Android  | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ (Automatic) |
| iOS      | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ (Automatic) |
| Windows  | ✅ | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ (Custom) |
| Linux    | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ (libsamplerate) |

### Sample Rate Conversion

All platforms support automatic sample rate conversion:

- **Android/iOS**: Built-in conversion via MediaPlayer/AVFoundation
- **Windows**: Custom linear interpolation resampling
- **Linux**: Professional quality conversion using libsamplerate (SRC_SINC_BEST_QUALITY)

### Async/Non-Blocking Behavior

All platforms implement non-blocking network audio playback:

- **Android**: Uses `prepareAsync()` for MediaPlayer
- **iOS**: Uses `AVPlayer` for network streams (non-blocking)
- **Windows**: Downloads on background thread
- **Linux**: Downloads via libcurl in background thread

## Architecture

### Android
- Uses `MediaPlayer` for playback
- Uses `AudioRecord` for recording
- Uses `AudioTrack` for streaming
- `prepareAsync()` ensures non-blocking network playback

### iOS
- Dual-player system:
  - `AVPlayer` for network URLs (non-blocking streaming)
  - `AVAudioPlayer` for local files (fast loading)
- Automatic sample rate conversion via AVFoundation

### Windows
- `Media Foundation` for MP3/audio decoding
- `WASAPI` (Windows Audio Session API) for low-latency audio
- Background thread download with WinHTTP
- `ISimpleAudioVolume` for volume control
- Loopback recording for system audio capture

### Linux
- `PulseAudio` for audio I/O
- `libcurl` for network downloads
- `libsndfile` for audio format support (MP3, OGG, FLAC, etc.)
- `libsamplerate` for professional sample rate conversion
- PulseAudio monitor stream for system audio capture

## Performance Considerations

- **Network URLs**: All platforms use non-blocking async playback
- **Local Files**: Fast loading with minimal latency
- **Memory**: Efficient streaming with configurable buffer sizes
- **CPU**: Optimized for real-time audio processing

## Troubleshooting

### Audio not playing

1. **Check file path**: Ensure the path is correct and accessible
2. **File format**: Verify the format is supported on your platform
3. **Network URLs**: Test internet connection and URL accessibility
4. **Permissions**: Check app has necessary permissions (especially on iOS/Android)

### Volume control not working

1. Ensure volume is set between 0.0 and 1.0
2. Check if audio is actually playing when setting volume
3. On Windows, system volume may override app volume

### System audio capture not working

1. **Windows Only**: Requires WASAPI loopback support (Windows 7+)
2. **Linux Only**: Requires PulseAudio with monitor source
3. Check system audio is actually playing (silent streams won't capture data)

### Duration returns 0.0 for network URLs

This is expected behavior. For network audio:
- Duration becomes available after buffering completes
- Use polling with exponential backoff to retrieve duration
- Example: Poll every 500ms with increasing delays

## Example App

A complete example app is included in the `example` directory demonstrating:
- Audio playback with controls
- Real-time position tracking
- Volume control
- Audio recording
- System audio capture (Windows/Linux)
- Network URL streaming

Run the example:
```bash
cd flutter_f2f_sound/example
flutter run
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## License

MIT License - see the [LICENSE](LICENSE) file for details.

## Support

If you find this plugin useful, please consider supporting its development:

### Buy Me a Coffee ☕

Your support helps maintain and improve this plugin. Any amount is appreciated!

<div align="center">
  <img src="20260117150332_7_111.jpg" alt="收款码" width="200"/>
  <p>微信扫码支持</p>
</div>

## Changelog

### Version 1.0.0
- ✅ Initial release
- ✅ Cross-platform audio playback (Android, iOS, Windows, Linux)
- ✅ Audio recording support (Android, Windows, Linux)
- ✅ System audio capture (Windows, Linux)
- ✅ Network streaming with non-blocking async
- ✅ Automatic sample rate conversion
- ✅ Volume and playback controls
- ✅ Loop playback support
- ✅ Real-time audio streaming
- ✅ Multiple format support (MP3, WAV, OGG, FLAC, etc.)

## Acknowledgments

- **Android**: MediaPlayer API
- **iOS**: AVFoundation framework
- **Windows**: Media Foundation and WASAPI
- **Linux**: PulseAudio, libcurl, libsndfile, libsamplerate

---

Made with ❤️ by the Flutter community
