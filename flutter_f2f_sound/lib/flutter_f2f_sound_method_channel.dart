import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'flutter_f2f_sound_platform_interface.dart';

/// An implementation of [FlutterF2fSoundPlatform] that uses method channels and event channels.
class MethodChannelFlutterF2fSound extends FlutterF2fSoundPlatform {
  /// The method channel used to interact with the native platform for method calls.
  @visibleForTesting
  final methodChannel = const MethodChannel('com.tecmore.flutter_f2f_sound');

  /// The event channel used to receive real-time audio data streams.
  @visibleForTesting
  final eventChannel = const EventChannel(
    'com.tecmore.flutter_f2f_sound/recording_stream',
  );

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>(
      'getPlatformVersion',
    );
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
    return await methodChannel.invokeMethod<double>('getCurrentPosition') ??
        0.0;
  }

  @override
  Future<double> getDuration(String path) async {
    return await methodChannel.invokeMethod<double>('getDuration', {
          'path': path,
        }) ??
        0.0;
  }

  @override
  Stream<List<int>> startRecording() async* {
    await methodChannel.invokeMethod('startRecording');
    yield* eventChannel.receiveBroadcastStream().map((event) {
      final list = event as List<dynamic>;
      return list.map((e) => e as int).toList();
    });
  }

  @override
  Future<void> stopRecording() async {
    await methodChannel.invokeMethod('stopRecording');
  }

  @override
  Stream<List<int>> startPlaybackStream(String path) async* {
    await methodChannel.invokeMethod('startPlaybackStream', {'path': path});
    yield* eventChannel.receiveBroadcastStream().map((event) {
      final list = event as List<dynamic>;
      return list.map((e) => e as int).toList();
    });
  }
}
