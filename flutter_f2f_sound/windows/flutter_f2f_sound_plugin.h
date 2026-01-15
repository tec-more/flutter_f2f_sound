#ifndef FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace flutter_f2f_sound {

class FlutterF2fSoundPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  FlutterF2fSoundPlugin();

  virtual ~FlutterF2fSoundPlugin();

  // Disallow copy and assign.
  FlutterF2fSoundPlugin(const FlutterF2fSoundPlugin&) = delete;
  FlutterF2fSoundPlugin& operator=(const FlutterF2fSoundPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace flutter_f2f_sound

#endif  // FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
