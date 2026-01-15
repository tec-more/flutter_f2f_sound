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
}
