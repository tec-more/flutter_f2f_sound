import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound_platform_interface.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockFlutterF2fSoundPlatform
    with MockPlatformInterfaceMixin
    implements FlutterF2fSoundPlatform {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');

  @override
  Future<void> play({
    required String path,
    double volume = 1.0,
    bool loop = false,
  }) => Future.value();

  @override
  Future<void> pause() => Future.value();

  @override
  Future<void> stop() => Future.value();

  @override
  Future<void> resume() => Future.value();

  @override
  Future<void> setVolume(double volume) => Future.value();

  @override
  Future<bool> isPlaying() => Future.value(false);

  @override
  Future<double> getCurrentPosition() => Future.value(0.0);

  @override
  Future<double> getDuration(String path) => Future.value(0.0);
}

void main() {
  final FlutterF2fSoundPlatform initialPlatform =
      FlutterF2fSoundPlatform.instance;

  test('$MethodChannelFlutterF2fSound is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelFlutterF2fSound>());
  });

  test('getPlatformVersion', () async {
    FlutterF2fSound flutterF2fSoundPlugin = FlutterF2fSound();
    MockFlutterF2fSoundPlatform fakePlatform = MockFlutterF2fSoundPlatform();
    FlutterF2fSoundPlatform.instance = fakePlatform;

    expect(await flutterF2fSoundPlugin.getPlatformVersion(), '42');
  });
}
