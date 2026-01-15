
import 'flutter_f2f_sound_platform_interface.dart';

/// Flutter F2F Sound Plugin
/// 
/// A cross-platform audio plugin that supports playback control across multiple platforms.
class FlutterF2fSound {
  /// Get the platform version
  Future<String?> getPlatformVersion() {
    return FlutterF2fSoundPlatform.instance.getPlatformVersion();
  }

  /// Play audio from the given path
  /// 
  /// [path] - The path to the audio file
  /// [volume] - The volume level (0.0 to 1.0)
  /// [loop] - Whether to loop the audio playback
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) {
    return FlutterF2fSoundPlatform.instance.play(
      path: path,
      volume: volume,
      loop: loop,
    );
  }

  /// Pause the currently playing audio
  Future<void> pause() {
    return FlutterF2fSoundPlatform.instance.pause();
  }

  /// Stop the currently playing audio
  Future<void> stop() {
    return FlutterF2fSoundPlatform.instance.stop();
  }

  /// Resume playback of paused audio
  Future<void> resume() {
    return FlutterF2fSoundPlatform.instance.resume();
  }

  /// Set the volume of the currently playing audio
  /// 
  /// [volume] - The volume level (0.0 to 1.0)
  Future<void> setVolume(double volume) {
    return FlutterF2fSoundPlatform.instance.setVolume(volume);
  }

  /// Check if audio is currently playing
  /// 
  /// Returns true if audio is playing, false otherwise
  Future<bool> isPlaying() {
    return FlutterF2fSoundPlatform.instance.isPlaying();
  }

  /// Get the current playback position in seconds
  /// 
  /// Returns the current position in seconds
  Future<double> getCurrentPosition() {
    return FlutterF2fSoundPlatform.instance.getCurrentPosition();
  }

  /// Get the duration of the audio file in seconds
  /// 
  /// [path] - The path to the audio file
  /// Returns the duration in seconds
  Future<double> getDuration(String path) {
    return FlutterF2fSoundPlatform.instance.getDuration(path);
  }
}
