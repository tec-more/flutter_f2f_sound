import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'flutter_f2f_sound_platform_interface.dart';

/// An implementation of [FlutterF2fSoundPlatform] that uses method channels.
class MethodChannelFlutterF2fSound extends FlutterF2fSoundPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('flutter_f2f_sound');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }

  @override
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) async {
    await methodChannel.invokeMethod('play', {
      'path': path,
      'volume': volume,
      'loop': loop,
    });
  }

  @override
  Future<void> pause() async {
    await methodChannel.invokeMethod('pause');
  }

  @override
  Future<void> stop() async {
    await methodChannel.invokeMethod('stop');
  }

  @override
  Future<void> resume() async {
    await methodChannel.invokeMethod('resume');
  }

  @override
  Future<void> setVolume(double volume) async {
    await methodChannel.invokeMethod('setVolume', {'volume': volume});
  }

  @override
  Future<bool> isPlaying() async {
    return await methodChannel.invokeMethod<bool>('isPlaying') ?? false;
  }

  @override
  Future<double> getCurrentPosition() async {
    return await methodChannel.invokeMethod<double>('getCurrentPosition') ?? 0.0;
  }

  @override
  Future<double> getDuration(String path) async {
    return await methodChannel.invokeMethod<double>('getDuration', {'path': path}) ?? 0.0;
  }
}
