#ifndef FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/event_channel.h>

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>

// Windows headers for REFERENCE_TIME, ERole, etc.
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

// WASAPI related forward declarations
struct IAudioClient;
struct IAudioCaptureClient;
struct IAudioRenderClient;
struct IAudioSessionControl;
struct IMMDevice;
struct IMMDeviceEnumerator;
struct IAudioVolumeLevel;

namespace flutter_f2f_sound {

// Custom window messages for audio data
const UINT WM_RECORDING_DATA = WM_USER + 100;
const UINT WM_SYSTEM_SOUND_DATA = WM_USER + 101;

// Audio recording configuration
struct AudioConfig {
  int sample_rate = 44100;
  int channels = 1;  // 1 = mono, 2 = stereo
  int bits_per_sample = 16;
  bool is_system_sound = false;
};

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

  // EventChannel handler
  void OnListenRecording(const flutter::EncodableValue *arguments,
                        std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events);
  void OnCancelRecording(const flutter::EncodableValue *arguments);
  void OnListenSystemSound(const flutter::EncodableValue *arguments,
                           std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events);
  void OnCancelSystemSound(const flutter::EncodableValue *arguments);
  void OnListenPlaybackStream(const flutter::EncodableValue *arguments,
                             std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events);
  void OnCancelPlaybackStream(const flutter::EncodableValue *arguments);

 private:
  // Audio recording variables
  std::atomic<bool> is_recording_{false};
  std::thread recording_thread_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> recording_event_sink_;
  std::mutex recording_mutex_;

  // System sound capture variables (separate from recording)
  std::atomic<bool> is_capturing_system_sound_{false};
  std::thread system_sound_thread_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> system_sound_event_sink_;
  std::mutex system_sound_mutex_;

  // Audio playback stream variables
  std::atomic<bool> is_playback_streaming_{false};
  std::thread playback_stream_thread_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> playback_stream_event_sink_;
  std::mutex playback_stream_mutex_;

  HWND message_window_ = nullptr;  // Hidden window for thread-safe message dispatching
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  void ProcessRecordingData(const std::vector<uint8_t>& audio_data);
  void ProcessSystemSoundData(const std::vector<uint8_t>& audio_data);
  void ProcessPlaybackStreamData(const std::vector<uint8_t>& audio_data);

  // WASAPI interfaces for recording
  IMMDeviceEnumerator* device_enumerator_ = nullptr;
  IMMDevice* recording_device_ = nullptr;
  IAudioClient* audio_client_ = nullptr;
  IAudioCaptureClient* capture_client_ = nullptr;

  // WASAPI interfaces for system sound capture (separate)
  IMMDevice* system_sound_device_ = nullptr;
  IAudioClient* system_sound_audio_client_ = nullptr;
  IAudioCaptureClient* system_sound_capture_client_ = nullptr;
  WAVEFORMATEX *system_sound_wave_format_ = nullptr;
  WAVEFORMATEXTENSIBLE *system_sound_wave_format_extensible_ = nullptr;
  UINT32 system_sound_buffer_frame_count_ = 0;

  // WASAPI interfaces for playback
  IMMDevice* playback_device_ = nullptr;
  IAudioClient* playback_audio_client_ = nullptr;
  IAudioRenderClient* render_client_ = nullptr;
  IAudioSessionControl* audio_session_control_ = nullptr;
  ISimpleAudioVolume* simple_audio_volume_ = nullptr;

  // Recording parameters
  AudioConfig recording_config_;
  WAVEFORMATEX *wave_format_ = nullptr;
  WAVEFORMATEXTENSIBLE *wave_format_extensible_ = nullptr;
  UINT32 buffer_frame_count_ = 0;
  REFERENCE_TIME hns_requested_duration_ = 10000000;  // 1 second

  // Playback parameters
  WAVEFORMATEX *playback_wave_format_ = nullptr;
  WAVEFORMATEXTENSIBLE *playback_wave_format_extensible_ = nullptr;
  UINT32 playback_buffer_frame_count_ = 0;

  // Temporary format for Media Foundation decoding
  WAVEFORMATEX *wave_format_ex_ = nullptr;

  // Audio control variables
  std::atomic<bool> is_playing_{false};
  std::atomic<bool> is_paused_{false};
  std::atomic<double> current_volume_{1.0};
  std::atomic<bool> is_looping_{false};
  std::string current_playback_path_;
  std::atomic<double> current_position_{0.0};
  std::atomic<double> current_duration_{0.0};

  // Playback thread
  std::thread playback_thread_;
  std::mutex playback_mutex_;

  // COM initialization helper
  std::unique_ptr<class ComInit> com_init_;

  // Helper methods
  HRESULT InitializeWASAPI(const AudioConfig& config);
  void StartRecordingThread();
  void StopRecording();
  HRESULT CleanupWASAPI();

  // System sound capture methods (separate from recording)
  HRESULT InitializeSystemSoundWASAPI(const AudioConfig& config);
  void StartSystemSoundThread();
  void StopSystemSoundCapture();
  HRESULT CleanupSystemSoundWASAPI();

  // Playback methods
  HRESULT InitializePlaybackWASAPI();
  void StartPlaybackThread();
  void StopPlayback();
  HRESULT CleanupPlaybackWASAPI();
  
  // Playback stream methods
  void PlaybackStreamThread(const std::string& path);

  // Audio file methods
  HRESULT ReadAudioFile(const std::string& path, std::vector<uint8_t>& audio_data, WAVEFORMATEX** format);
  HRESULT ReadAudioFileWithMF(const std::string& path, std::vector<uint8_t>& audio_data, WAVEFORMATEX** format);
  HRESULT ConvertAudioFormat(const WAVEFORMATEX* input_format, const std::vector<uint8_t>& input_data,
                             const WAVEFORMATEX* output_format, std::vector<uint8_t>& output_data);
  HRESULT DownloadAudioFile(const std::string& url, std::string& local_path);
  bool IsURL(const std::string& path);
  bool IsMP3(const std::string& path);

  // Format conversion helpers
  HRESULT CreateFormatForConfig(const AudioConfig& config, WAVEFORMATEX** format);
  bool IsFormatSupported(const WAVEFORMATEX* format, IMMDevice* device);

  // Volume control
  HRESULT SetPlaybackVolume(double volume);
  HRESULT GetPlaybackVolume(double* volume);
};

// COM initialization helper
class ComInit {
public:
    ComInit() {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }
    ~ComInit() {
        CoUninitialize();
    }
};

}  // namespace flutter_f2f_sound

#endif  // FLUTTER_PLUGIN_FLUTTER_F2F_SOUND_PLUGIN_H_
