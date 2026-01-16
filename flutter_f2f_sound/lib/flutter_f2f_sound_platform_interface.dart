import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'flutter_f2f_sound_method_channel.dart';

abstract class FlutterF2fSoundPlatform extends PlatformInterface {
  /// Constructs a FlutterF2fSoundPlatform.
  FlutterF2fSoundPlatform() : super(token: _token);

  static final Object _token = Object();

  static FlutterF2fSoundPlatform _instance = MethodChannelFlutterF2fSound();

  /// The default instance of [FlutterF2fSoundPlatform] to use.
  ///
  /// Defaults to [MethodChannelFlutterF2fSound].
  static FlutterF2fSoundPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [FlutterF2fSoundPlatform] when
  /// they register themselves.
  static set instance(FlutterF2fSoundPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  /// Play audio from the given path
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) {
    throw UnimplementedError('play() has not been implemented.');
  }

  /// Pause the currently playing audio
  Future<void> pause() {
    throw UnimplementedError('pause() has not been implemented.');
  }

  /// Stop the currently playing audio
  Future<void> stop() {
    throw UnimplementedError('stop() has not been implemented.');
  }

  /// Resume playback of paused audio
  Future<void> resume() {
    throw UnimplementedError('resume() has not been implemented.');
  }

  /// Set the volume of the currently playing audio (0.0 to 1.0)
  Future<void> setVolume(double volume) {
    throw UnimplementedError('setVolume() has not been implemented.');
  }

  /// Check if audio is currently playing
  Future<bool> isPlaying() {
    throw UnimplementedError('isPlaying() has not been implemented.');
  }

  /// Get the current playback position in seconds
  Future<double> getCurrentPosition() {
    throw UnimplementedError('getCurrentPosition() has not been implemented.');
  }

  /// Get the duration of the audio file in seconds
  Future<double> getDuration(String path) {
    throw UnimplementedError('getDuration() has not been implemented.');
  }

  // 音频录制流
  Stream<List<int>> startRecording();
  Future<void> stopRecording();

  // 系统声音捕获流
  Stream<List<int>> startSystemSoundCapture();

  // 音频播放流
  Stream<List<int>> startPlaybackStream(String path);
}
