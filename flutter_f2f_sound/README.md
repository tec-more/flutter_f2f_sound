# F2F Sound Plugin

A cross-platform audio plugin for Flutter that provides audio playback control across multiple platforms including Android, iOS, Windows, macOS, Linux, and Web.

## Features

- 🎵 **Audio Playback** - Play audio from local files or network URLs
- 🎚️ **Volume Control** - Adjust audio volume from 0.0 to 1.0
- ⏯️ **Playback Controls** - Play, pause, stop, and resume audio
- 🔄 **Loop Playback** - Support for looping audio playback
- 📊 **Playback Information** - Get current position and duration of audio
- 📱 **Cross-Platform** - Works on Android, iOS, Windows, macOS, Linux, and Web

## Installation

Add `flutter_f2f_sound` to your `pubspec.yaml` file:

```yaml
dependencies:
  flutter_f2f_sound: ^1.0.0
```

Then run:

```bash
flutter pub get
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

### Basic Usage

```dart
// Play audio from a file path
await f2fSound.play(
  path: '/path/to/audio/file.mp3',
  volume: 1.0,
  loop: false,
);

// Play audio from a URL
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

// Get duration of audio in seconds
double duration = await f2fSound.getDuration('/path/to/audio/file.mp3');
```

### Complete Example

```dart
import 'package:flutter/material.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final _f2fSound = FlutterF2fSound();
  bool _isPlaying = false;
  double _volume = 1.0;
  String _audioPath = 'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3';

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('F2F Sound Example')),
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              TextField(
                decoration: const InputDecoration(labelText: 'Audio Path'),
                onChanged: (value) => _audioPath = value,
                controller: TextEditingController(text: _audioPath),
              ),
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  ElevatedButton(
                    onPressed: () async {
                      await _f2fSound.play(
                        path: _audioPath,
                        volume: _volume,
                        loop: false,
                      );
                      setState(() => _isPlaying = true);
                    },
                    child: const Text('Play'),
                  ),
                  ElevatedButton(
                    onPressed: () async {
                      await _f2fSound.pause();
                      setState(() => _isPlaying = false);
                    },
                    child: const Text('Pause'),
                  ),
                  ElevatedButton(
                    onPressed: () async {
                      await _f2fSound.stop();
                      setState(() => _isPlaying = false);
                    },
                    child: const Text('Stop'),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Slider(
                value: _volume,
                min: 0.0,
                max: 1.0,
                onChanged: (value) async {
                  await _f2fSound.setVolume(value);
                  setState(() => _volume = value);
                },
              ),
              Text('Volume: ${(_volume * 100).toInt()}%'),
              Text('Is Playing: $_isPlaying'),
            ],
          ),
        ),
      ),
    );
  }
}
```

## API Reference

### FlutterF2fSound

#### `Future<String?> getPlatformVersion()`
Get the platform version.

#### `Future<void> play({required String path, double volume = 1.0, bool loop = false})`
Play audio from the given path.

- `path`: The path to the audio file (local file path or URL)
- `volume`: The volume level (0.0 to 1.0)
- `loop`: Whether to loop the audio playback

#### `Future<void> pause()`
Pause the currently playing audio.

#### `Future<void> resume()`
Resume playback of paused audio.

#### `Future<void> stop()`
Stop the currently playing audio.

#### `Future<void> setVolume(double volume)`
Set the volume of the currently playing audio.

- `volume`: The volume level (0.0 to 1.0)

#### `Future<bool> isPlaying()`
Check if audio is currently playing.

#### `Future<double> getCurrentPosition()`
Get the current playback position in seconds.

#### `Future<double> getDuration(String path)`
Get the duration of the audio file in seconds.

- `path`: The path to the audio file (local file path or URL)

## Platform Support

| Platform | Supported | Notes |
|----------|-----------|-------|
| Android  | ✅         | Uses MediaPlayer API |
| iOS      | ✅         | Uses AVAudioPlayer API |
| Windows  | ❌         | Coming soon |
| macOS    | ❌         | Coming soon |
| Linux    | ❌         | Coming soon |
| Web      | ❌         | Coming soon |

## Permissions

### Android

Add the following permissions to your `AndroidManifest.xml` file:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
```

### iOS

Add the following to your `Info.plist` file:

```xml
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSAllowsArbitraryLoads</key>
    <true/>
</dict>
```

## Troubleshooting

### Audio not playing

1. Check if the audio path is correct
2. Ensure the file format is supported (MP3, WAV, AAC, etc.)
3. Check permissions for accessing the file
4. For network URLs, ensure internet connection is available

### Volume control not working

1. Ensure the volume is set between 0.0 and 1.0
2. Check if the audio is actually playing when setting volume

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT License - see the [LICENSE](LICENSE) file for details.


