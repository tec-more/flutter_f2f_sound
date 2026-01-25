# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.0.4] - 2026-01-25

### Fixed
- Play 1channel,16-bit,16kHz on Windows (fixed in 1.0.4)


## [1.0.3] - 2026-01-25

### Fixed
- Minor 10000000 duration issue on Windows (fixed in 1.0.3)

## [1.0.2] - 2026-01-25

### Fixed
- Playback stream functionality across all platforms
- Network URL streaming support
- Event channel conflicts resolved
- Java 23 compatibility issues
- Windows CMake cache path mismatch error
- TLS certificate errors during dependency download

### Improvements
- Enhanced network streaming performance
- Better error handling for audio playback
- Improved cross-platform compatibility

## [1.0.1] - 2026-01-20

### Fixed
- Minor bug fixes and performance improvements

## [1.0.0] - 2025-01-17

### Added
- Audio Playback
  - Play audio from local files (MP3, WAV, OGG, FLAC, AAC, M4A)
  - Play audio from network URLs with non-blocking async streaming
  - Support for all major platforms: Android, iOS, macOS, Windows, Linux

- Audio Recording
  - Microphone recording support on Android, iOS, macOS, Windows, Linux
  - Real-time audio streaming to Flutter layer via EventChannel
  - Configurable sample rate and channels

- System Audio Capture
  - Windows: WASAPI loopback recording for system audio
  - Linux: PulseAudio monitor stream for system audio

- Playback Controls
  - Play, pause, stop, resume controls
  - Loop playback support
  - Volume control (0.0 to 1.0)
  - Seeking support (Linux)

- Playback Information
  - Current position tracking
  - Duration retrieval
  - Playback state query

- Network Streaming
  - Non-blocking async playback from HTTP/HTTPS URLs
  - Background downloading (Windows, Linux)
  - AVPlayer for iOS network streams
  - MediaPlayer prepareAsync for Android

- Format Conversion
  - Automatic sample rate conversion on all platforms
  - Android/iOS: Built-in conversion via MediaPlayer/AVFoundation
  - Windows: Custom linear interpolation resampling
  - Linux: Professional quality conversion using libsamplerate (SRC_SINC_BEST_QUALITY)
  - macOS: Built-in conversion via AVFoundation

- Real-time Audio Streams
  - Recording stream: Microphone audio data streaming
  - Playback stream: Audio data streaming during playback (macOS only)
  - System sound stream: System audio capture (Windows/Linux only)

### Platform-Specific Features

#### Android
- MediaPlayer for playback
- AudioRecord for recording
- prepareAsync() for non-blocking network playback
- Format support: MP3, WAV, AAC, FLAC, OGG, M4A

#### iOS
- AVPlayer for network URLs (non-blocking streaming)
- AVAudioPlayer for local files (fast loading)
- AudioToolbox AudioQueue for recording with real-time streaming
- Automatic sample rate conversion via AVFoundation
- Format support: MP3, WAV, AAC, ALAC, M4A, OGG, FLAC

#### macOS
- AVAudioPlayer and AVAudioEngine for playback
- AVAudioRecorder for recording
- AVAudioEngine for playback streaming (unique feature)
- Automatic sample rate conversion via AVFoundation
- Format support: MP3, WAV, AAC, M4A, OGG, FLAC

#### Windows
- Media Foundation for MP3/audio decoding
- WASAPI (Windows Audio Session API) for low-latency audio
- Background thread download with WinHTTP
- ISimpleAudioVolume for volume control
- Loopback recording for system audio capture
- Format support: MP3, WAV, WMA, AAC

#### Linux
- PulseAudio for audio I/O
- libcurl for network downloads
- libsndfile for audio format support (MP3, OGG, FLAC, OPUS, VOC, etc.)
- libsamplerate for professional sample rate conversion
- PulseAudio monitor stream for system audio capture
- Seeking support in playback

### Documentation
- Comprehensive README.md with usage examples
- Platform support matrix
- API reference documentation
- Technical architecture details
- Troubleshooting guide
- Platform-specific setup instructions

### Example App
- Complete example demonstrating all features
- Audio playback with controls
- Real-time position tracking
- Volume control
- Audio recording
- System audio capture (Windows/Linux)
- Network URL streaming

## [Unreleased]

### Planned Features
- Web platform support enhancements
- Audio effects (EQ, reverb, etc.)
- Audio visualization support
- More format support
- Performance optimizations

---

**Note**: This project follows Semantic Versioning. For more information, see [semver.org](https://semver.org/).
