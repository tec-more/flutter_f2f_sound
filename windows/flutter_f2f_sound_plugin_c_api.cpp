#include "include/flutter_f2f_sound/flutter_f2f_sound_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "flutter_f2f_sound_plugin.h"

void FlutterF2fSoundPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  flutter_f2f_sound::FlutterF2fSoundPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
