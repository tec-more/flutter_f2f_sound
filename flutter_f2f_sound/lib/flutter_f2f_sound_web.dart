// In order to *not* need this ignore, consider extracting the "web" version
// of your plugin as a separate package, instead of inlining it in the same
// package as the core of your plugin.
// ignore: avoid_web_libraries_in_flutter

import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:web/web.dart' as web;

import 'flutter_f2f_sound_platform_interface.dart';

/// A web implementation of the FlutterF2fSoundPlatform of the FlutterF2fSound plugin.
class FlutterF2fSoundWeb extends FlutterF2fSoundPlatform {
  /// Constructs a FlutterF2fSoundWeb
  FlutterF2fSoundWeb();

  static void registerWith(Registrar registrar) {
    FlutterF2fSoundPlatform.instance = FlutterF2fSoundWeb();
  }

  /// Returns a [String] containing the version of the platform.
  @override
  Future<String?> getPlatformVersion() async {
    final version = web.window.navigator.userAgent;
    return version;
  }

  /// Returns the duration in seconds
  @override
  Future<double> getDuration(String path) async {
    return 0.0;
  }

  /// Start audio recording and get a stream of recorded audio data
  /// 
  /// Returns a stream of audio data as List<int> (PCM samples)
  @override
  Stream<List<int>> startRecording() async* {
    yield* Stream.empty();
  }

  /// Stop audio recording
  @override
  Future<void> stopRecording() async {
    return Future.value();
  }

  /// Start audio playback stream
  @override
  Stream<List<int>> startPlaybackStream(String path) async* {
    yield* Stream.empty();
  }

  /// Play audio from the given path
  @override
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) async {
    // Web implementation for play
  }

  /// Pause the currently playing audio
  @override
  Future<void> pause() async {
    // Web implementation for pause
  }

  /// Stop the currently playing audio
  @override
  Future<void> stop() async {
    // Web implementation for stop
  }

  /// Resume playback of paused audio
  @override
  Future<void> resume() async {
    // Web implementation for resume
  }

  /// Set the volume of the currently playing audio (0.0 to 1.0)
  @override
  Future<void> setVolume(double volume) async {
    // Web implementation for setVolume
  }

  /// Check if audio is currently playing
  @override
  Future<bool> isPlaying() async {
    // Web implementation for isPlaying
    return false;
  }

  /// Get the current playback position in seconds
  @override
  Future<double> getCurrentPosition() async {
    // Web implementation for getCurrentPosition
    return 0.0;
  }

}
