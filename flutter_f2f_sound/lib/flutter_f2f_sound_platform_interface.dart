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
}
