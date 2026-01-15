
import 'flutter_f2f_sound_platform_interface.dart';

class FlutterF2fSound {
  Future<String?> getPlatformVersion() {
    return FlutterF2fSoundPlatform.instance.getPlatformVersion();
  }
}
