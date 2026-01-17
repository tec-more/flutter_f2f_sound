// In order to *not* need this ignore, consider extracting the "web" version
// of your plugin as a separate package, instead of inlining it in the same
// package as the core of your plugin.
// ignore: avoid_web_libraries_in_flutter

import 'dart:async';
import 'dart:js_interop';
import 'package:web/web.dart';
import 'package:flutter_web_plugins/flutter_web_plugins.dart';

import 'flutter_f2f_sound_platform_interface.dart';

// JS interop extensions for audio processing
extension type AudioProcessingEvent._(JSObject _) implements JSObject {
  external AudioBuffer get inputBuffer;
}

extension type AudioBuffer._(JSObject _) implements JSObject {
  external JSFloat32Array getChannelData(int channel);
}

extension type JSFloat32Array._(JSObject _) implements JSObject {
  external int get length;
  external double operator [](int index);
}

/// A web implementation of the FlutterF2fSoundPlatform of the FlutterF2fSound plugin.
///
/// Note: Web implementation uses Web Audio API to provide PCM audio data,
/// making it more compatible with native platforms.
class FlutterF2fSoundWeb extends FlutterF2fSoundPlatform {
  /// Constructs a FlutterF2fSoundWeb
  FlutterF2fSoundWeb();

  static void registerWith(Registrar registrar) {
    FlutterF2fSoundPlatform.instance = FlutterF2fSoundWeb();
  }

  // Web Audio API
  AudioContext? _audioContext;
  HTMLAudioElement? _audioElement;
  MediaStream? _mediaStream;
  ScriptProcessorNode? _scriptProcessor;
  MediaStreamAudioSourceNode? _sourceNode;

  // Recording state
  final StreamController<List<int>> _recordingController =
      StreamController<List<int>>.broadcast();

  // Playback state
  double _currentPosition = 0.0;
  double _currentDuration = 0.0;
  Timer? _positionTimer;
  bool _isPlaying = false;
  bool _isPaused = false;

  // Static event handlers for audio element
  static final Map<HTMLAudioElement, FlutterF2fSoundWeb> _instances = {};

  // Create AudioContext
  AudioContext _createAudioContext() {
    return AudioContext();
  }

  // Static event handlers
  static void _handleTimeUpdate(Event event) {
    final element = event.currentTarget as HTMLAudioElement;
    final instance = _instances[element];
    if (instance != null) {
      instance._currentPosition = element.currentTime.toDouble();
    }
  }

  static void _handleDurationChange(Event event) {
    final element = event.currentTarget as HTMLAudioElement;
    final instance = _instances[element];
    if (instance != null) {
      instance._currentDuration = element.duration.toDouble();
    }
  }

  static void _handleEnded(Event event) {
    final element = event.currentTarget as HTMLAudioElement;
    final instance = _instances[element];
    if (instance != null) {
      instance._isPlaying = false;
      instance._positionTimer?.cancel();
    }
  }

  static void _handlePlay(Event event) {
    final element = event.currentTarget as HTMLAudioElement;
    final instance = _instances[element];
    if (instance != null) {
      instance._isPlaying = true;
      instance._isPaused = false;
      instance._startPositionTimer();
    }
  }

  static void _handlePause(Event event) {
    final element = event.currentTarget as HTMLAudioElement;
    final instance = _instances[element];
    if (instance != null) {
      instance._isPlaying = false;
      instance._isPaused = true;
      instance._positionTimer?.cancel();
    }
  }

  /// Returns a [String] containing the version of the platform.
  @override
  Future<String?> getPlatformVersion() async {
    final version = window.navigator.userAgent;
    return 'Web - $version';
  }

  /// Play audio from the given path
  @override
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) async {
    try {
      // Stop any currently playing audio
      await stop();

      // Create audio element
      _audioElement = HTMLAudioElement()..src = path;
      _audioElement!.volume = volume;
      _audioElement!.loop = loop;

      // Register this instance
      _instances[_audioElement!] = this;

      // Set up event listeners using static handlers
      _audioElement!.ontimeupdate = (_handleTimeUpdate.toJS);
      _audioElement!.ondurationchange = (_handleDurationChange.toJS);
      _audioElement!.onended = (_handleEnded.toJS);
      _audioElement!.onplay = (_handlePlay.toJS);
      _audioElement!.onpause = (_handlePause.toJS);

      // Start playback
      try {
        await _audioElement!.play().toDart;
      } catch (error) {
        throw Exception('Failed to play audio: $error');
      }
    } catch (e) {
      throw Exception('Error playing audio: $e');
    }
  }

  /// Start position timer
  void _startPositionTimer() {
    _positionTimer?.cancel();
    _positionTimer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      if (_audioElement != null && _isPlaying) {
        _currentPosition = _audioElement!.currentTime.toDouble();
      }
    });
  }

  /// Pause the currently playing audio
  @override
  Future<void> pause() async {
    _audioElement?.pause();
  }

  /// Stop the currently playing audio
  @override
  Future<void> stop() async {
    _positionTimer?.cancel();
    _audioElement?.pause();
    if (_audioElement != null) {
      _audioElement!.currentTime = 0.0;
      // Unregister this instance
      _instances.remove(_audioElement);
    }
    _currentPosition = 0.0;
    _isPlaying = false;
    _isPaused = false;
  }

  /// Resume playback of paused audio
  @override
  Future<void> resume() async {
    _audioElement?.play();
  }

  /// Set the volume of the currently playing audio (0.0 to 1.0)
  @override
  Future<void> setVolume(double volume) async {
    if (_audioElement != null) {
      _audioElement!.volume = volume;
    }
  }

  /// Check if audio is currently playing
  @override
  Future<bool> isPlaying() async {
    return _isPlaying && !_isPaused;
  }

  /// Get the current playback position in seconds
  @override
  Future<double> getCurrentPosition() async {
    return _currentPosition;
  }

  /// Get the duration of the audio file in seconds
  @override
  Future<double> getDuration(String path) async {
    if (_audioElement != null) {
      return _currentDuration;
    }
    return 0.0;
  }

  /// Start audio recording and get a stream of recorded audio data
  ///
  /// Uses Web Audio API with ScriptProcessorNode to capture raw PCM audio data.
  /// Returns 16-bit PCM audio samples (mono, typically 44100 or 48000 Hz).
  @override
  Stream<List<int>> startRecording() async* {
    try {
      // Request microphone access
      final stream = await window.navigator.mediaDevices.getUserMedia(
        MediaStreamConstraints(audio: true.toJS),
      ).toDart;

      _mediaStream = stream;

      // Create audio context if needed
      _audioContext ??= _createAudioContext();
      final context = _audioContext!;

      // Create media stream source
      _sourceNode = context.createMediaStreamSource(stream);

      // Create script processor for capturing raw audio
      // Buffer size: 4096 samples
      // Input channels: 1 (mono)
      // Output channels: 1 (mono)
      const bufferSize = 4096;
      _scriptProcessor = context.createScriptProcessor(
        bufferSize,
        1,
        1,
      );

      // Connect nodes: source -> processor -> destination
      _sourceNode!.connect(_scriptProcessor!);
      _scriptProcessor!.connect(context.destination);

      // Create audioprocess handler
      void audioProcessHandler(JSAny event) {
        final audioProcessingEvent = event as AudioProcessingEvent;
        final inputBuffer = audioProcessingEvent.inputBuffer;
        final channelData = inputBuffer.getChannelData(0);

        // Convert Float32Array to 16-bit PCM
        final pcmData = <int>[];
        final length = channelData.length;

        for (var i = 0; i < length; i++) {
          // Get float sample (-1.0 to 1.0)
          final sample = channelData[i];

          // Convert to 16-bit PCM (-32768 to 32767)
          final pcmSample = (sample * 32767.0).clamp(-32768.0, 32767.0).toInt();

          // Add as little-endian bytes
          pcmData.add(pcmSample & 0xFF);
          pcmData.add((pcmSample >> 8) & 0xFF);
        }

        if (!_recordingController.isClosed) {
          _recordingController.add(pcmData);
        }
      }

      // Listen to audioprocess event
      _scriptProcessor!.onaudioprocess = (audioProcessHandler.toJS);

      yield* _recordingController.stream;
    } catch (e) {
      _cleanupRecording();
      throw Exception('Error starting recording: $e');
    }
  }

  /// Stop audio recording
  @override
  Future<void> stopRecording() async {
    _cleanupRecording();

    // Close controller
    if (!_recordingController.isClosed) {
      await _recordingController.close();
    }
  }

  /// Cleanup recording resources
  void _cleanupRecording() {
    if (_sourceNode != null) {
      _sourceNode!.disconnect();
      _sourceNode = null;
    }

    if (_scriptProcessor != null) {
      _scriptProcessor!.disconnect();
      _scriptProcessor = null;
    }

    if (_mediaStream != null) {
      final tracks = _mediaStream!.getAudioTracks();
      for (var i = 0; i < tracks.length; i++) {
        tracks[i].stop();
      }
      _mediaStream = null;
    }
  }

  /// Start system sound capture and get a stream of captured audio data
  ///
  /// Note: On web, system sound capture is not supported due to browser security restrictions.
  @override
  Stream<List<int>> startSystemSoundCapture() async* {
    throw UnimplementedError(
      'System sound capture is not supported on web platform due to browser security restrictions. '
      'This feature requires native platform access (Android/iOS/Windows/Desktop). '
      'Consider using screen capture APIs with appropriate permissions as an alternative.',
    );
  }

  /// Start audio playback stream
  ///
  /// Note: On web, playback streaming is not supported. Use the play() method instead.
  @override
  Stream<List<int>> startPlaybackStream(String path) async* {
    throw UnimplementedError(
      'Playback streaming is not supported on web platform. '
      'Use the play() method for audio playback instead. '
      'The play() method supports local files and network URLs (HTTP/HTTPS).',
    );
  }

  /// Dispose resources
  void dispose() {
    _positionTimer?.cancel();
    _recordingController.close();

    _audioElement?.pause();
    _audioElement = null;

    _cleanupRecording();

    _audioContext?.close();
    _audioContext = null;
  }
}
