#include "include/flutter_f2f_sound/flutter_f2f_sound_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <cstring>
#include <memory>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "flutter_f2f_sound_plugin_private.h"

#define FLUTTER_F2F_SOUND_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), flutter_f2f_sound_plugin_get_type(), \
                              FlutterF2fSoundPlugin))

// Audio context structure with enhanced features
struct AudioContext {
  // PulseAudio components
  pa_mainloop* mainloop = nullptr;
  pa_mainloop_api* mainloop_api = nullptr;
  pa_context* context = nullptr;
  pa_stream* playback_stream = nullptr;
  pa_stream* record_stream = nullptr;
  pa_stream* monitor_stream = nullptr;  // For system audio capture

  // State flags
  std::atomic<bool> is_playing{false};
  std::atomic<bool> is_recording{false};
  std::atomic<bool> is_capturing_system{false};
  std::atomic<double> volume{1.0};
  std::atomic<double> current_position{0.0};
  std::atomic<double> duration{0.0};

  // Audio data
  std::vector<uint8_t> audio_data;
  std::vector<uint8_t> recorded_data;
  size_t playback_index = 0;

  // Audio format
  pa_sample_format_t sample_format = PA_SAMPLE_S16LE;
  int sample_rate = 44100;
  int channels = 2;

  // Event channels
  FlEventChannel* recording_event_channel = nullptr;
  FlEventChannel* system_sound_event_channel = nullptr;
  FlEventChannel* playback_event_channel = nullptr;

  // Seeking support
  std::atomic<bool> seek_requested{false};
  std::atomic<double> seek_position{0.0};
};

struct _FlutterF2fSoundPlugin {
  GObject parent_instance;
  AudioContext* audio_ctx = nullptr;
  FlMethodChannel* method_channel = nullptr;
  FlEventChannel* recording_event_channel = nullptr;
  FlEventChannel* system_sound_event_channel = nullptr;
  FlEventChannel* playback_event_channel = nullptr;
};

G_DEFINE_TYPE(FlutterF2fSoundPlugin, flutter_f2f_sound_plugin, g_object_get_type())

// ==================== libcurl Download Callback ====================

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t totalSize = size * nmemb;
  std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(userp);

  size_t oldSize = buffer->size();
  buffer->resize(oldSize + totalSize);
  std::memcpy(buffer->data() + oldSize, contents, totalSize);

  return totalSize;
}

// ==================== Network Audio Download with libcurl ====================

static bool download_audio_file(const std::string& url, std::vector<uint8_t>& audio_data) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    g_printerr("Failed to initialize libcurl\n");
    return false;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audio_data);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "FlutterF2FSound/1.0");

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    g_printerr("Failed to download audio: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return false;
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (http_code != 200) {
    g_printerr("HTTP error: %ld\n", http_code);
    return false;
  }

  g_print("Downloaded %zu bytes from %s\n", audio_data.size(), url.c_str());
  return true;
}

// ==================== Audio Format Support with libsndfile ====================

static bool load_audio_file(const std::string& path, std::vector<uint8_t>& raw_data,
                           int& sample_rate, int& channels) {
  SF_INFO sfinfo;
  memset(&sfinfo, 0, sizeof(sfinfo));

  SNDFILE* sndfile = sf_open(path.c_str(), SFM_READ, &sfinfo);
  if (!sndfile) {
    g_printerr("Failed to open audio file: %s\n", path.c_str());
    return false;
  }

  sample_rate = sfinfo.samplerate;
  channels = sfinfo.channels;

  // Read audio data
  std::vector<short> samples(sfinfo.frames * channels);
  sf_count_t count = sf_readf_short(sndfile, samples.data(), sfinfo.frames);

  // Convert to bytes (16-bit PCM)
  raw_data.resize(samples.size() * sizeof(short));
  std::memcpy(raw_data.data(), samples.data(), raw_data.size());

  sf_close(sndfile);

  g_print("Loaded audio: %d Hz, %d channels, %zu frames\n",
          sample_rate, channels, (size_t)sfinfo.frames);

  return true;
}

// ==================== Sample Rate Conversion with libsamplerate ====================

static bool convert_sample_rate(const std::vector<int16_t>& input,
                               int input_rate,
                               int output_rate,
                               std::vector<int16_t>& output) {
  if (input_rate == output_rate) {
    output = input;
    return true;
  }

  SRC_DATA src_data;
  double ratio = (double)output_rate / (double)input_rate;

  src_data.data_in = input.data();
  src_data.input_frames = input.size();
  src_data.src_ratio = ratio;

  size_t output_size = (size_t)(input.size() * ratio) + 1;
  src_data.data_out = new float[output_size];
  src_data.output_frames = output_size;
  src_data.end_of_input = 0;

  SRC_STATE* src_state = src_new(SRC_SINC_BEST_QUALITY, 1, nullptr);
  if (!src_state) {
    g_printerr("Failed to create sample rate converter\n");
    return false;
  }

  int error = src_process(src_state, &src_data);
  src_delete(src_state);

  if (error) {
    g_printerr("Sample rate conversion error: %s\n", src_strerror(error));
    delete[] src_data.data_out;
    return false;
  }

  // Convert float back to int16
  output.resize(src_data.output_frames_gen);
  for (size_t i = 0; i < src_data.output_frames_gen; i++) {
    float sample = src_data.data_out[i];
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    output[i] = (int16_t)(sample * 32767.0f);
  }

  delete[] src_data.data_out;
  return true;
}

// ==================== PulseAudio Callbacks ====================

static void context_state_cb(pa_context* c, void* userdata) {
  auto* audio_ctx = static_cast<AudioContext*>(userdata);

  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
      g_print("PulseAudio context ready\n");
      break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      g_printerr("PulseAudio context failed or terminated\n");
      if (audio_ctx->mainloop) {
        pa_mainloop_quit(audio_ctx->mainloop, 1);
      }
      break;
    default:
      break;
  }
}

static void stream_write_cb(pa_stream* s, size_t nbytes, void* userdata) {
  auto* audio_ctx = static_cast<AudioContext*>(userdata);

  if (!audio_ctx || audio_ctx->audio_data.empty() ||
      audio_ctx->playback_index >= audio_ctx->audio_data.size()) {
    // No more data - write silence
    uint8_t* silence = new uint8_t[nbytes];
    memset(silence, 0, nbytes);
    pa_stream_write(s, silence, nbytes, [](void* p) { delete[] (uint8_t*)p; }, 0, PA_SEEK_RELATIVE);

    // Mark as finished
    if (audio_ctx) {
      audio_ctx->is_playing = false;
    }
    return;
  }

  // Check for seek request
  if (audio_ctx->seek_requested) {
    size_t new_index = (size_t)(audio_ctx->seek_position * audio_ctx->sample_rate *
                               audio_ctx->channels * sizeof(int16_t));
    audio_ctx->playback_index = new_index;
    audio_ctx->seek_requested = false;
    g_print("Seeked to position: %.2f seconds\n", audio_ctx->seek_position.load());
  }

  // Calculate how much data we can write
  size_t bytes_available = audio_ctx->audio_data.size() - audio_ctx->playback_index;
  size_t bytes_to_write = std::min(nbytes, bytes_available);

  // Get the current audio data
  const uint8_t* data_to_write = audio_ctx->audio_data.data() + audio_ctx->playback_index;

  // Write data to stream
  pa_stream_write(s,
                  data_to_write,
                  bytes_to_write,
                  nullptr,
                  0,
                  PA_SEEK_RELATIVE);

  // Send audio data to Flutter via event channel
  if (audio_ctx->playback_event_channel) {
    // Create a list of integers from the audio data
    g_autoptr(FlValue) list = fl_value_new_list();
    for (size_t i = 0; i < bytes_to_write; i++) {
      fl_value_append_take(list, fl_value_new_int(data_to_write[i]));
    }

    // Create event data
    g_autoptr(FlValue) event = fl_value_new_map();
    fl_value_set_string_take(event, "event", fl_value_new_string("data"));
    fl_value_set_string_take(event, "data", g_steal_pointer(&list));

    // Send the event
    fl_event_channel_send(audio_ctx->playback_event_channel, event, nullptr, nullptr, nullptr);
  }

  audio_ctx->playback_index += bytes_to_write;

  // Update position
  double bytes_per_second = audio_ctx->sample_rate * audio_ctx->channels * sizeof(int16_t);
  audio_ctx->current_position = audio_ctx->playback_index / bytes_per_second;
}

static void stream_read_cb(pa_stream* s, size_t nbytes, void* userdata) {
  auto* audio_ctx = static_cast<AudioContext*>(userdata);

  if (!audio_ctx || !audio_ctx->is_recording) {
    return;
  }

  const void* data;
  if (pa_stream_peek(s, &data, &nbytes) < 0) {
    return;
  }

  if (data) {
    // Send audio data to Flutter via event channel
    if (audio_ctx->recording_event_channel) {
      const uint8_t* byte_data = static_cast<const uint8_t*>(data);

      g_autoptr(FlValue) list = fl_value_new_list();
      for (size_t i = 0; i < nbytes; i++) {
        fl_value_append_take(list, fl_value_new_int(byte_data[i]));
      }

      g_autoptr(FlValue) event = fl_value_new_map();
      fl_value_set_string_take(event, "event", fl_value_new_string("data"));
      fl_value_set_string_take(event, "data", g_steal_pointer(&list));

      fl_event_channel_send(audio_ctx->recording_event_channel, event, nullptr, nullptr, nullptr);
    }

    // Store recorded data
    size_t old_size = audio_ctx->recorded_data.size();
    audio_ctx->recorded_data.resize(old_size + nbytes);
    std::memcpy(audio_ctx->recorded_data.data() + old_size, data, nbytes);
  }

  pa_stream_drop(s);
}

// System audio capture callback (using PulseAudio monitor)
static void monitor_read_cb(pa_stream* s, size_t nbytes, void* userdata) {
  auto* audio_ctx = static_cast<AudioContext*>(userdata);

  if (!audio_ctx || !audio_ctx->is_capturing_system) {
    return;
  }

  const void* data;
  if (pa_stream_peek(s, &data, &nbytes) < 0) {
    return;
  }

  if (data) {
    // Send system audio data to Flutter
    if (audio_ctx->system_sound_event_channel) {
      const uint8_t* byte_data = static_cast<const uint8_t*>(data);

      g_autoptr(FlValue) list = fl_value_new_list();
      for (size_t i = 0; i < nbytes; i++) {
        fl_value_append_take(list, fl_value_new_int(byte_data[i]));
      }

      g_autoptr(FlValue) event = fl_value_new_map();
      fl_value_set_string_take(event, "event", fl_value_new_string("data"));
      fl_value_set_string_take(event, "data", g_steal_pointer(&list));

      fl_event_channel_send(audio_ctx->system_sound_event_channel, event, nullptr, nullptr, nullptr);
    }
  }

  pa_stream_drop(s);
}

// ==================== PulseAudio Initialization ====================

static bool init_pulse_audio(AudioContext* audio_ctx) {
  if (!audio_ctx) return false;

  audio_ctx->mainloop = pa_mainloop_new();
  if (!audio_ctx->mainloop) {
    g_printerr("Failed to create PulseAudio mainloop\n");
    return false;
  }

  audio_ctx->mainloop_api = pa_mainloop_get_api(audio_ctx->mainloop);

  audio_ctx->context = pa_context_new(audio_ctx->mainloop_api, "FlutterF2FSound");
  if (!audio_ctx->context) {
    g_printerr("Failed to create PulseAudio context\n");
    return false;
  }

  pa_context_set_state_callback(audio_ctx->context, context_state_cb, audio_ctx);

  if (pa_context_connect(audio_ctx->context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0) {
    g_printerr("Failed to connect to PulseAudio: %s\n",
               pa_strerror(pa_context_errno(audio_ctx->context)));
    return false;
  }

  // Wait for context to be ready
  while (pa_context_get_state(audio_ctx->context) != PA_CONTEXT_READY) {
    pa_mainloop_iterate(audio_ctx->mainloop, 1, nullptr);
  }

  return true;
}

static void cleanup_pulse_audio(AudioContext* audio_ctx) {
  if (!audio_ctx) return;

  if (audio_ctx->playback_stream) {
    pa_stream_unref(audio_ctx->playback_stream);
    audio_ctx->playback_stream = nullptr;
  }

  if (audio_ctx->record_stream) {
    pa_stream_unref(audio_ctx->record_stream);
    audio_ctx->record_stream = nullptr;
  }

  if (audio_ctx->monitor_stream) {
    pa_stream_unref(audio_ctx->monitor_stream);
    audio_ctx->monitor_stream = nullptr;
  }

  if (audio_ctx->context) {
    pa_context_disconnect(audio_ctx->context);
    pa_context_unref(audio_ctx->context);
    audio_ctx->context = nullptr;
  }

  if (audio_ctx->mainloop) {
    pa_mainloop_free(audio_ctx->mainloop);
    audio_ctx->mainloop = nullptr;
  }
}

// ==================== Helper Functions ====================

static bool is_url(const std::string& path) {
  return (path.find("http://") == 0 || path.find("https://") == 0);
}

// ==================== Method Handler ====================

static void flutter_f2f_sound_plugin_handle_method_call(
    FlutterF2fSoundPlugin* self,
    FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;

  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  if (!self->audio_ctx) {
    self->audio_ctx = new AudioContext();
  }

  if (strcmp(method, "getPlatformVersion") == 0) {
    struct utsname uname_data = {};
    uname(&uname_data);
    g_autofree gchar *version = g_strdup_printf("Linux %s", uname_data.version);
    g_autoptr(FlValue) result = fl_value_new_string(version);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
  }
  else if (strcmp(method, "play") == 0) {
    FlValue* path_value = fl_value_get_map_value(args, fl_value_new_string("path"));
    FlValue* volume_value = fl_value_get_map_value(args, fl_value_new_string("volume"));

    if (!path_value || fl_value_get_type(path_value) != FL_VALUE_TYPE_STRING) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new("INVALID_ARGUMENT", "Path is required"));
    } else {
      const gchar* path = fl_value_get_string(path_value);
      double volume = volume_value ? fl_value_get_float(volume_value) : 1.0;

      // Initialize PulseAudio if needed
      if (!self->audio_ctx->context && !init_pulse_audio(self->audio_ctx)) {
        response = FL_METHOD_RESPONSE(fl_method_error_response_new("AUDIO_INIT_ERROR", "Failed to initialize audio"));
      } else {
        std::vector<uint8_t> audio_data;
        int sample_rate = 44100;
        int channels = 2;

        // Load audio file (local or network)
        if (is_url(path)) {
          g_print("Downloading audio from: %s\n", path);
          
          // Create a copy of the path for thread safety
          std::string url_path = path;
          FlutterF2fSoundPlugin* plugin_ptr = self;
          FlMethodCall* method_call_copy = fl_method_call_ref(method_call);
          
          // Start asynchronous download in a separate thread
          std::thread download_thread([plugin_ptr, url_path, method_call_copy]() {
            std::vector<uint8_t> audio_data;
            int sample_rate = 44100;
            int channels = 2;
            
            bool success = download_audio_file(url_path, audio_data);
            
            // Create a response on the main thread
            g_main_context_invoke(nullptr, [plugin_ptr, url_path, audio_data, sample_rate, channels, success, method_call_copy]() {
              if (!success) {
                g_autoptr(FlMethodResponse) error_response = FL_METHOD_RESPONSE(
                    fl_method_error_response_new("DOWNLOAD_ERROR", "Failed to download audio"));
                fl_method_call_respond(method_call_copy, error_response, nullptr);
              } else {
                // Try to parse as audio file using libsndfile
                // For downloaded data, we'd need to save to temp file first
                // For now, use the raw data
                plugin_ptr->audio_ctx->audio_data = audio_data;
                plugin_ptr->audio_ctx->sample_rate = sample_rate;
                plugin_ptr->audio_ctx->channels = channels;
                
                // Continue with playback setup
                plugin_ptr->audio_ctx->is_playing = true;
                plugin_ptr->audio_ctx->current_position = 0.0;
                plugin_ptr->audio_ctx->volume = fl_value_get_float(fl_value_get_map_value(
                    fl_method_call_get_args(method_call_copy), fl_value_new_string("volume")));
                plugin_ptr->audio_ctx->playback_index = 0;
                
                // Create playback stream
                pa_sample_spec ss;
                ss.format = PA_SAMPLE_S16LE;
                ss.rate = plugin_ptr->audio_ctx->sample_rate;
                ss.channels = plugin_ptr->audio_ctx->channels;
                
                plugin_ptr->audio_ctx->playback_stream = pa_stream_new(
                    plugin_ptr->audio_ctx->context,
                    "FlutterF2FSound Playback",
                    &ss,
                    nullptr);
                
                if (plugin_ptr->audio_ctx->playback_stream) {
                  pa_stream_set_write_callback(plugin_ptr->audio_ctx->playback_stream, stream_write_cb, plugin_ptr->audio_ctx);
                  
                  // Connect stream to default output
                  pa_stream_connect_playback(
                      plugin_ptr->audio_ctx->playback_stream,
                      nullptr,
                      nullptr,
                      PA_STREAM_START_CORKED,
                      nullptr,
                      nullptr);
                  
                  // Start playback
                  pa_stream_cork(plugin_ptr->audio_ctx->playback_stream, 0, nullptr, nullptr);
                  
                  g_print("Playing audio: %s (%.2f seconds)\n", url_path.c_str(),
                          audio_data.size() / (double)(sample_rate * channels * sizeof(int16_t)));
                  
                  g_autoptr(FlMethodResponse) success_response = FL_METHOD_RESPONSE(
                      fl_method_success_response_new(nullptr));
                  fl_method_call_respond(method_call_copy, success_response, nullptr);
                } else {
                  g_autoptr(FlMethodResponse) error_response = FL_METHOD_RESPONSE(
                      fl_method_error_response_new("STREAM_ERROR", "Failed to create playback stream"));
                  fl_method_call_respond(method_call_copy, error_response, nullptr);
                }
              }
              
              fl_method_call_unref(method_call_copy);
            });
          });
          
          // Detach the thread to allow it to run independently
          download_thread.detach();
          
          // Return early since we'll respond asynchronously
          return;
        } else {
          // Load local file using libsndfile
          if (!load_audio_file(path, audio_data, sample_rate, channels)) {
            response = FL_METHOD_RESPONSE(fl_method_error_response_new("LOAD_ERROR", "Failed to load audio file"));
          } else {
            self->audio_ctx->audio_data = audio_data;
            self->audio_ctx->sample_rate = sample_rate;
            self->audio_ctx->channels = channels;
          }
        }

        if (!response) {
          self->audio_ctx->is_playing = true;
          self->audio_ctx->current_position = 0.0;
          self->audio_ctx->volume = volume;
          self->audio_ctx->playback_index = 0;

          // Create playback stream
          pa_sample_spec ss;
          ss.format = PA_SAMPLE_S16LE;
          ss.rate = self->audio_ctx->sample_rate;
          ss.channels = self->audio_ctx->channels;

          self->audio_ctx->playback_stream = pa_stream_new(
              self->audio_ctx->context,
              "FlutterF2FSound Playback",
              &ss,
              nullptr);

          if (!self->audio_ctx->playback_stream) {
            response = FL_METHOD_RESPONSE(fl_method_error_response_new("STREAM_ERROR", "Failed to create playback stream"));
          } else {
            pa_stream_set_write_callback(self->audio_ctx->playback_stream, stream_write_cb, self->audio_ctx);

            // Connect stream to default output
            pa_stream_connect_playback(
                self->audio_ctx->playback_stream,
                nullptr,
                nullptr,
                PA_STREAM_START_CORKED,
                nullptr,
                nullptr);

            // Start playback
            pa_stream_cork(self->audio_ctx->playback_stream, 0, nullptr, nullptr);

          g_print("Playing audio: %s (%.2f seconds)\n", path,
                  audio_data.size() / (double)(sample_rate * channels * sizeof(int16_t)));
          response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
        }
      }
    }
  }
  else if (strcmp(method, "pause") == 0) {
    if (self->audio_ctx->playback_stream) {
      pa_stream_cork(self->audio_ctx->playback_stream, 1, nullptr, nullptr);
    }
    self->audio_ctx->is_playing = false;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "stop") == 0) {
    if (self->audio_ctx->playback_stream) {
      pa_stream_cork(self->audio_ctx->playback_stream, 1, nullptr, nullptr);
    }
    self->audio_ctx->is_playing = false;
    self->audio_ctx->current_position = 0.0;
    self->audio_ctx->playback_index = 0;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "resume") == 0) {
    if (self->audio_ctx->playback_stream) {
      pa_stream_cork(self->audio_ctx->playback_stream, 0, nullptr, nullptr);
    }
    self->audio_ctx->is_playing = true;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "setVolume") == 0) {
    FlValue* volume_value = fl_value_get_map_value(args, fl_value_new_string("volume"));
    if (volume_value) {
      double volume = fl_value_get_float(volume_value);
      self->audio_ctx->volume = volume;

      // Set PulseAudio stream volume
      if (self->audio_ctx->playback_stream) {
        pa_cvolume cvolume;
        pa_cvolume_init(&cvolume);
        cvolume.channels = self->audio_ctx->channels;
        for (int i = 0; i < cvolume.channels; i++) {
          cvolume.values[i] = pa_sw_volume_from_linear(volume);
        }

        pa_context_set_sink_input_volume(
            self->audio_ctx->context,
            pa_stream_get_index(self->audio_ctx->playback_stream),
            &cvolume,
            nullptr,
            nullptr);
      }

      g_print("Volume set to: %.2f\n", volume);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "isPlaying") == 0) {
    bool playing = self->audio_ctx->is_playing.load();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_bool(playing)));
  }
  else if (strcmp(method, "getCurrentPosition") == 0) {
    double pos = self->audio_ctx->current_position.load();
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_float(pos)));
  }
  else if (strcmp(method, "getDuration") == 0) {
    FlValue* path_value = fl_value_get_map_value(args, fl_value_new_string("path"));
    if (path_value && fl_value_get_type(path_value) == FL_VALUE_TYPE_STRING) {
      const gchar* path = fl_value_get_string(path_value);
      if (is_url(path)) {
        response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_float(0.0)));
      } else {
        // Use libsndfile to get duration
        SF_INFO sfinfo;
        memset(&sfinfo, 0, sizeof(sfinfo));
        SNDFILE* sndfile = sf_open(path, SFM_READ, &sfinfo);
        if (sndfile) {
          double duration = (double)sfinfo.frames / sfinfo.samplerate;
          sf_close(sndfile);
          response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_float(duration)));
        } else {
          response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_float(0.0)));
        }
      }
    } else {
      response = FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_float(0.0)));
    }
  }
  else if (strcmp(method, "startRecording") == 0) {
    g_print("Starting recording\n");
    self->audio_ctx->is_recording = true;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "stopRecording") == 0) {
    g_print("Stopping recording\n");
    self->audio_ctx->is_recording = false;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else if (strcmp(method, "startSystemSoundCapture") == 0) {
    g_print("Starting system sound capture\n");

    // Create monitor stream to capture system audio
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 2;

    self->audio_ctx->monitor_stream = pa_stream_new(
        self->audio_ctx->context,
        "FlutterF2FSound Monitor",
        &ss,
        nullptr);

    if (self->audio_ctx->monitor_stream) {
      pa_stream_set_read_callback(self->audio_ctx->monitor_stream, monitor_read_cb, self->audio_ctx);

      // Connect to monitor of default sink
      pa_context_get_sink_info_by_name(
          self->audio_ctx->context,
          nullptr,  // Default sink
          [](pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
            if (i) {
              AudioContext* ctx = static_cast<AudioContext*>(userdata);
              pa_stream_connect_record(
                  ctx->monitor_stream,
                  i->monitor_source_name,
                  nullptr,
                  PA_STREAM_START_CORKED);

              pa_stream_cork(ctx->monitor_stream, 0, nullptr, nullptr);
            }
          },
          self->audio_ctx);

      pa_mainloop_iterate(self->audio_ctx->mainloop, 1, nullptr);
    }

    self->audio_ctx->is_capturing_system = true;
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  }
  else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

// ==================== Plugin Lifecycle ====================

static void flutter_f2f_sound_plugin_dispose(GObject* object) {
  FlutterF2fSoundPlugin* self = FLUTTER_F2F_SOUND_PLUGIN(object);

  if (self->audio_ctx) {
    cleanup_pulse_audio(self->audio_ctx);
    delete self->audio_ctx;
    self->audio_ctx = nullptr;
  }

  g_clear_object(&self->method_channel);
  g_clear_object(&self->recording_event_channel);
  g_clear_object(&self->system_sound_event_channel);
  g_clear_object(&self->playback_event_channel);

  G_OBJECT_CLASS(flutter_f2f_sound_plugin_parent_class)->dispose(object);
}

static void flutter_f2f_sound_plugin_class_init(FlutterF2fSoundPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = flutter_f2f_sound_plugin_dispose;
}

static void flutter_f2f_sound_plugin_init(FlutterF2fSoundPlugin* self) {
  self->audio_ctx = new AudioContext();

  // Initialize libcurl globally
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  FlutterF2fSoundPlugin* plugin = FLUTTER_F2F_SOUND_PLUGIN(user_data);
  flutter_f2f_sound_plugin_handle_method_call(plugin, method_call);
}

void flutter_f2f_sound_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  FlutterF2fSoundPlugin* plugin = FLUTTER_F2F_SOUND_PLUGIN(
      g_object_new(flutter_f2f_sound_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) method_channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "com.tecmore.flutter_f2f_sound",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(method_channel, method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);
  plugin->method_channel = FL_METHOD_CHANNEL(g_steal_pointer(&method_channel));

  // Create recording event channel
  g_autoptr(FlStandardMethodCodec) event_codec = fl_standard_method_codec_new();
  g_autoptr(FlEventChannel) recording_event_channel =
      fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar),
                          "com.tecmore.flutter_f2f_sound/recording_stream",
                          FL_METHOD_CODEC(event_codec));
  plugin->recording_event_channel = FL_EVENT_CHANNEL(g_steal_pointer(&recording_event_channel));
  if (plugin->audio_ctx) {
    plugin->audio_ctx->recording_event_channel = plugin->recording_event_channel;
  }

  // Create system sound event channel
  g_autoptr(FlEventChannel) system_sound_event_channel =
      fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar),
                          "com.tecmore.flutter_f2f_sound/system_sound_stream",
                          FL_METHOD_CODEC(event_codec));
  plugin->system_sound_event_channel = FL_EVENT_CHANNEL(g_steal_pointer(&system_sound_event_channel));
  if (plugin->audio_ctx) {
    plugin->audio_ctx->system_sound_event_channel = plugin->system_sound_event_channel;
  }

  // Create playback event channel
  g_autoptr(FlEventChannel) playback_event_channel =
      fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar),
                          "com.tecmore.flutter_f2f_sound/playback_stream",
                          FL_METHOD_CODEC(event_codec));
  plugin->playback_event_channel = FL_EVENT_CHANNEL(g_steal_pointer(&playback_event_channel));
  if (plugin->audio_ctx) {
    plugin->audio_ctx->playback_event_channel = plugin->playback_event_channel;
  }

  g_object_unref(plugin);
}
