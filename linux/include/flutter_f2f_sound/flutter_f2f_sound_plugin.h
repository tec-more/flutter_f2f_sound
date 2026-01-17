#ifndef FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_

#include <flutter_linux/flutter_linux.h>
#include <pulse/pulseaudio.h>
#include <curl/curl.h>
#include <sndfile.h>
#include <samplerate.h>
#include <pthread.h>
#include <atomic>
#include <string>
#include <vector>

G_BEGIN_DECLS

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLUTTER_PLUGIN_EXPORT
#endif

typedef struct _FlutterF2fSoundPlugin FlutterF2fSoundPlugin;
typedef struct {
  GObjectClass parent_class;
} FlutterF2fSoundPluginClass;

FLUTTER_PLUGIN_EXPORT GType flutter_f2f_sound_plugin_get_type();

FLUTTER_PLUGIN_EXPORT void flutter_f2f_sound_plugin_register_with_registrar(
    FlPluginRegistrar* registrar);

G_END_DECLS

#endif  // FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
