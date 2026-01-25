#include "flutter_f2f_sound_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

// WASAPI related headers
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

// COM related headers
#include <comdef.h>

// HTTP download headers
#include <winhttp.h>

// Media Foundation headers for audio decoding
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// For std::min and std::max
#include <algorithm>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>

#include <memory>
#include <sstream>
#include <thread>
#include <vector>
#include <fstream>
#include <string>

// Link against required libraries
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace flutter_f2f_sound {

// static
void FlutterF2fSoundPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  // Method channel for audio control commands
  auto method_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "com.tecmore.flutter_f2f_sound",
          &flutter::StandardMethodCodec::GetInstance());

  // Event channel for recording stream
  auto recording_event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "com.tecmore.flutter_f2f_sound/recording_stream",
          &flutter::StandardMethodCodec::GetInstance());

  // Event channel for system sound capture stream (separate channel)
  auto system_sound_event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "com.tecmore.flutter_f2f_sound/system_sound_stream",
          &flutter::StandardMethodCodec::GetInstance());

  // Event channel for playback stream
  auto playback_event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "com.tecmore.flutter_f2f_sound/playback_stream",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<FlutterF2fSoundPlugin>();

  method_channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  // Set up recording stream handler
  recording_event_channel->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments,
                                          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events) {
            plugin_pointer->OnListenRecording(arguments, std::move(events));
            return nullptr;
          },
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments) {
            plugin_pointer->OnCancelRecording(arguments);
            return nullptr;
          }));

  // Set up system sound stream handler
  system_sound_event_channel->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments,
                                          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events) {
            plugin_pointer->OnListenSystemSound(arguments, std::move(events));
            return nullptr;
          },
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments) {
            plugin_pointer->OnCancelSystemSound(arguments);
            return nullptr;
          }));

  // Set up playback stream handler
  playback_event_channel->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments,
                                          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events) {
            plugin_pointer->OnListenPlaybackStream(arguments, std::move(events));
            return nullptr;
          },
          [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments) {
            plugin_pointer->OnCancelPlaybackStream(arguments);
            return nullptr;
          }));

  registrar->AddPlugin(std::move(plugin));
}

FlutterF2fSoundPlugin::FlutterF2fSoundPlugin() {
  // Initialize COM
  com_init_ = std::make_unique<ComInit>();

  // Initialize Media Foundation
  HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
  if (SUCCEEDED(hr)) {
    OutputDebugStringA("Media Foundation initialized successfully\n");
  } else {
    OutputDebugStringA("Failed to initialize Media Foundation\n");
  }

  // Create a hidden window for message dispatching
  HINSTANCE hInstance = GetModuleHandle(NULL);
  static const wchar_t* CLASS_NAME = L"FlutterF2fSoundMessageWindow";

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;

  RegisterClassW(&wc);

  message_window_ = CreateWindowExW(
      0,
      CLASS_NAME,
      L"Audio Message Window",
      0,
      0, 0, 0, 0,
      HWND_MESSAGE,  // Message-only window
      NULL,
      hInstance,
      this
  );

  // Create device enumerator
  hr = CoCreateInstance(
      __uuidof(MMDeviceEnumerator), NULL,
      CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
      (void**)&device_enumerator_);

  if (FAILED(hr)) {
    // Handle initialization failure
  }
}

FlutterF2fSoundPlugin::~FlutterF2fSoundPlugin() {
  StopRecording();
  StopSystemSoundCapture();
  StopPlayback();
  CleanupWASAPI();
  CleanupSystemSoundWASAPI();
  CleanupPlaybackWASAPI();

  // Destroy message window
  if (message_window_) {
    DestroyWindow(message_window_);
    message_window_ = nullptr;
  }

  // Release device enumerator
  if (device_enumerator_) {
    device_enumerator_->Release();
    device_enumerator_ = nullptr;
  }

  // Shutdown Media Foundation
  MFShutdown();
}

void FlutterF2fSoundPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    result->Success(flutter::EncodableValue(version_stream.str()));
  } else if (method_call.method_name().compare("startRecording") == 0) {
    if (is_recording_) {
      result->Success(flutter::EncodableValue(nullptr));
      return;
    }

    // Parse recording parameters if provided
    const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
    AudioConfig config;
    config.is_system_sound = false;

    if (args) {
      auto sample_rate_it = args->find(flutter::EncodableValue("sampleRate"));
      if (sample_rate_it != args->end()) {
        config.sample_rate = std::get<int>(sample_rate_it->second);
      }

      auto channels_it = args->find(flutter::EncodableValue("channels"));
      if (channels_it != args->end()) {
        config.channels = std::get<int>(channels_it->second);
      }
    }

    HRESULT hr = InitializeWASAPI(config);
    if (SUCCEEDED(hr)) {
      StartRecordingThread();
      result->Success(flutter::EncodableValue(nullptr));
    } else {
      result->Error("RECORD_INIT_ERROR", "Failed to initialize WASAPI for recording");
    }
  } else if (method_call.method_name().compare("stopRecording") == 0) {
    StopRecording();
    result->Success(flutter::EncodableValue(nullptr));
  } else if (method_call.method_name().compare("startSystemSoundCapture") == 0) {
    if (is_capturing_system_sound_) {
      result->Success(flutter::EncodableValue(nullptr));
      return;
    }

    AudioConfig config;
    config.is_system_sound = true;
    config.sample_rate = 44100;
    config.channels = 2;  // Stereo for system sound

    HRESULT hr = InitializeSystemSoundWASAPI(config);
    if (SUCCEEDED(hr)) {
      StartSystemSoundThread();
      result->Success(flutter::EncodableValue(nullptr));
    } else {
      result->Error("SYSTEM_SOUND_CAPTURE_ERROR", "Failed to initialize WASAPI for system sound capture");
    }
  } else if (method_call.method_name().compare("play") == 0) {
    StopPlayback();

    // Parse parameters
    const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (args) {
      auto path = std::get<std::string>(args->at(flutter::EncodableValue("path")));
      auto volume = std::get<double>(args->at(flutter::EncodableValue("volume")));
      auto loop = std::get<bool>(args->at(flutter::EncodableValue("loop")));

      current_volume_ = volume;
      is_looping_ = loop;

      // Reset position when starting new playback
      current_position_ = 0.0;

      char debug_msg[512];
      sprintf_s(debug_msg, sizeof(debug_msg), "Play called with path: %s, volume: %.2f, loop: %d, position reset to 0.0\n",
                path.c_str(), volume, loop);
      OutputDebugStringA(debug_msg);

      current_playback_path_ = path;

      // Initialize playback first (this is fast)
      HRESULT hr = InitializePlaybackWASAPI();
      if (SUCCEEDED(hr)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "WASAPI initialized successfully, starting playback thread\n");
        OutputDebugStringA(debug_msg);

        // Start playback thread (downloads and plays in background)
        StartPlaybackThread();

        // Return immediately - don't wait for playback to actually start
        // This prevents UI blocking for network URLs
        result->Success(flutter::EncodableValue(nullptr));
      } else {
        sprintf_s(debug_msg, sizeof(debug_msg), "WASAPI initialization failed: 0x%08X\n", hr);
        OutputDebugStringA(debug_msg);
        result->Error("PLAY_INIT_ERROR", "Failed to initialize WASAPI for playback");
      }
      return;
    }

    result->Error("INVALID_ARGS", "Invalid arguments for play");
  } else if (method_call.method_name().compare("pause") == 0) {
    if (is_playing_ && !is_paused_) {
      is_paused_ = true;
    }
    result->Success(flutter::EncodableValue(nullptr));
  } else if (method_call.method_name().compare("resume") == 0) {
    if (is_playing_ && is_paused_) {
      is_paused_ = false;
    }
    result->Success(flutter::EncodableValue(nullptr));
  } else if (method_call.method_name().compare("stop") == 0) {
    StopPlayback();
    result->Success(flutter::EncodableValue(nullptr));
  } else if (method_call.method_name().compare("setVolume") == 0) {
    const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (args) {
      auto volume = std::get<double>(args->at(flutter::EncodableValue("volume")));
      current_volume_ = volume;
      SetPlaybackVolume(volume);
      result->Success(flutter::EncodableValue(nullptr));
      return;
    }
    result->Error("INVALID_ARGS", "Invalid arguments for setVolume");
  } else if (method_call.method_name().compare("isPlaying") == 0) {
    result->Success(flutter::EncodableValue(is_playing_ && !is_paused_));
  } else if (method_call.method_name().compare("getCurrentPosition") == 0) {
    double position = current_position_.load();

    // char debug_msg[256];
    // sprintf_s(debug_msg, sizeof(debug_msg),
    //   "getCurrentPosition called: current_position=%.2f, returning=%.2f\n",
    //   position, position);
    // OutputDebugStringA(debug_msg);

    // Explicitly return as double to avoid type confusion
    result->Success(flutter::EncodableValue(static_cast<double>(position)));
  } else if (method_call.method_name().compare("getDuration") == 0) {
    double duration = current_duration_.load();

    char debug_msg[256];
    sprintf_s(debug_msg, sizeof(debug_msg),
      "getDuration called: current_duration=%.2f\n", duration);
    OutputDebugStringA(debug_msg);

    // Explicitly return as double to avoid type confusion
    result->Success(flutter::EncodableValue(static_cast<double>(duration)));
  } else if (method_call.method_name().compare("startPlaybackStream") == 0) {
    // Parse the arguments to get the audio file path
    const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (!args) {
      result->Error("ARGUMENT_ERROR", "Invalid arguments for startPlaybackStream");
      return;
    }

    auto path_value = args->find(flutter::EncodableValue("path"));
    if (path_value == args->end()) {
      result->Error("ARGUMENT_ERROR", "Missing 'path' argument");
      return;
    }

    std::string path = std::get<std::string>(path_value->second);

    // Stop any existing playback stream
    if (is_playback_streaming_) {
      is_playback_streaming_ = false;
      if (playback_stream_thread_.joinable()) {
        playback_stream_thread_.join();
      }
    }

    // Start a new playback stream thread
    is_playback_streaming_ = true;
    playback_stream_thread_ = std::thread([this, path]() {
      PlaybackStreamThread(path);
    });

    result->Success(flutter::EncodableValue(nullptr));
  } else {
    result->NotImplemented();
  }
}

void FlutterF2fSoundPlugin::OnListenRecording(const flutter::EncodableValue *arguments,
                                              std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events) {
  recording_event_sink_ = std::move(events);
}

void FlutterF2fSoundPlugin::OnCancelRecording(const flutter::EncodableValue *arguments) {
  recording_event_sink_.reset();
  StopRecording();
}

void FlutterF2fSoundPlugin::OnListenSystemSound(const flutter::EncodableValue *arguments,
                                                std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events) {
  system_sound_event_sink_ = std::move(events);
}

void FlutterF2fSoundPlugin::OnCancelSystemSound(const flutter::EncodableValue *arguments) {
  system_sound_event_sink_.reset();
  // Stop system sound capture
  if (is_capturing_system_sound_) {
    is_capturing_system_sound_ = false;
    if (system_sound_thread_.joinable()) {
      system_sound_thread_.join();
    }
  }
}

void FlutterF2fSoundPlugin::OnListenPlaybackStream(const flutter::EncodableValue *arguments, 
                                                  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events) {
  std::lock_guard<std::mutex> lock(playback_stream_mutex_);
  playback_stream_event_sink_ = std::move(events);
}

void FlutterF2fSoundPlugin::OnCancelPlaybackStream(const flutter::EncodableValue *arguments) {
  std::lock_guard<std::mutex> lock(playback_stream_mutex_);
  playback_stream_event_sink_.reset();
  
  // Stop playback streaming
  if (is_playback_streaming_) {
    is_playback_streaming_ = false;
    if (playback_stream_thread_.joinable()) {
      playback_stream_thread_.join();
    }
  }
}

void FlutterF2fSoundPlugin::ProcessPlaybackStreamData(const std::vector<uint8_t>& audio_data) {
  std::lock_guard<std::mutex> lock(playback_stream_mutex_);
  
  if (playback_stream_event_sink_ && !audio_data.empty()) {
    // Create EncodableList from audio data
    flutter::EncodableList encodable_audio_data;
    for (uint8_t byte : audio_data) {
      encodable_audio_data.push_back(static_cast<int>(byte));
    }
    
    // Send data to Flutter (now on platform thread)
    playback_stream_event_sink_->Success(flutter::EncodableValue(encodable_audio_data));
  } else {
    if (!playback_stream_event_sink_) {
      OutputDebugStringA("ProcessPlaybackStreamData: No event sink!");
    }
  }
}

HRESULT FlutterF2fSoundPlugin::CreateFormatForConfig(const AudioConfig& config, WAVEFORMATEX** format) {
  // Create WAVEFORMATEXTENSIBLE for better format support
  WAVEFORMATEXTENSIBLE* format_extensible = (WAVEFORMATEXTENSIBLE*)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE));
  if (format_extensible == nullptr) {
    return E_OUTOFMEMORY;
  }

  ZeroMemory(format_extensible, sizeof(WAVEFORMATEXTENSIBLE));

  format_extensible->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format_extensible->Format.nChannels = static_cast<WORD>(config.channels);
  format_extensible->Format.nSamplesPerSec = config.sample_rate;
  format_extensible->Format.wBitsPerSample = static_cast<WORD>(config.bits_per_sample);
  format_extensible->Format.nBlockAlign = format_extensible->Format.nChannels * format_extensible->Format.wBitsPerSample / 8;
  format_extensible->Format.nAvgBytesPerSec = format_extensible->Format.nSamplesPerSec * format_extensible->Format.nBlockAlign;
  format_extensible->Format.cbSize = 22;  // Size of the extra data for WAVEFORMATEXTENSIBLE

  // Set up the extensible part
  format_extensible->Samples.wValidBitsPerSample = static_cast<WORD>(config.bits_per_sample);

  if (config.channels == 1) {
    format_extensible->dwChannelMask = SPEAKER_FRONT_CENTER;
  } else if (config.channels == 2) {
    format_extensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  } else {
    format_extensible->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
  }

  format_extensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  *format = (WAVEFORMATEX*)format_extensible;
  return S_OK;
}

bool FlutterF2fSoundPlugin::IsFormatSupported(const WAVEFORMATEX* format, IMMDevice* device) {
  IAudioClient* temp_audio_client = nullptr;
  HRESULT hr = device->Activate(
      __uuidof(IAudioClient), CLSCTX_ALL, NULL,
      (void**)&temp_audio_client);

  if (FAILED(hr)) {
    return false;
  }

  WAVEFORMATEX* closest_match = nullptr;
  hr = temp_audio_client->IsFormatSupported(
      AUDCLNT_SHAREMODE_SHARED,
      format,
      &closest_match);

  if (closest_match) {
    CoTaskMemFree(closest_match);
  }

  temp_audio_client->Release();

  return (hr == S_OK || hr == AUDCLNT_E_UNSUPPORTED_FORMAT);  // Accept if format is close enough
}

HRESULT FlutterF2fSoundPlugin::InitializeWASAPI(const AudioConfig& config) {
  HRESULT hr = S_OK;

  // Cleanup any existing WASAPI resources
  CleanupWASAPI();

  // Store configuration
  recording_config_ = config;

  // Get default audio device
  EDataFlow data_flow = config.is_system_sound ? eRender : eCapture;
  ERole role = eConsole;

  hr = device_enumerator_->GetDefaultAudioEndpoint(
      data_flow, role, &recording_device_);
  if (FAILED(hr)) {
    return hr;
  }

  // Create format based on configuration
  hr = CreateFormatForConfig(config, &wave_format_);
  if (FAILED(hr)) {
    return hr;
  }

  // Check if format is supported
  if (!IsFormatSupported(wave_format_, recording_device_)) {
    // Fall back to mix format if custom format is not supported
    CoTaskMemFree(wave_format_);
    wave_format_ = nullptr;

    hr = recording_device_->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, NULL,
        (void**)&audio_client_);
    if (FAILED(hr)) {
      return hr;
    }

    hr = audio_client_->GetMixFormat(&wave_format_);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Activate audio client
  if (!audio_client_) {
    hr = recording_device_->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, NULL,
        (void**)&audio_client_);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Create event handle for event-driven capture
  if (!recording_event_) {
    recording_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!recording_event_) {
      CoTaskMemFree(wave_format_);
      wave_format_ = nullptr;
      return HRESULT_FROM_WIN32(GetLastError());
    }
  }

  // Initialize audio client with event callback
  DWORD stream_flags = config.is_system_sound ? AUDCLNT_STREAMFLAGS_LOOPBACK : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

  hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      stream_flags,
      hns_requested_duration_, 0,
      wave_format_, NULL);

  if (FAILED(hr)) {
    CloseHandle(recording_event_);
    recording_event_ = nullptr;
    CoTaskMemFree(wave_format_);
    wave_format_ = nullptr;
    return hr;
  }

  // Set the event handle
  hr = audio_client_->SetEventHandle(recording_event_);
  if (FAILED(hr)) {
    CloseHandle(recording_event_);
    recording_event_ = nullptr;
    CoTaskMemFree(wave_format_);
    wave_format_ = nullptr;
    return hr;
  }

  // Get buffer size
  hr = audio_client_->GetBufferSize(&buffer_frame_count_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get capture client
  hr = audio_client_->GetService(
      __uuidof(IAudioCaptureClient),
      (void**)&capture_client_);
  if (FAILED(hr)) {
    return hr;
  }

  return hr;
}

void FlutterF2fSoundPlugin::StartRecordingThread() {
  is_recording_ = true;

  recording_thread_ = std::thread([this]() {
    // Set thread priority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) {
      if (recording_event_sink_) {
        recording_event_sink_->Error("RECORD_START_ERROR", "Failed to start audio client");
      }
      is_recording_ = false;
      return;
    }

    UINT32 packet_size = 0;
    DWORD flags = 0;
    BYTE *data = nullptr;
    UINT32 num_frames_available = 0;

    while (is_recording_) {
      // Wait for buffer event to be signaled
      DWORD wait_result = WaitForSingleObject(recording_event_, 2000);  // 2 second timeout
      if (wait_result != WAIT_OBJECT_0) {
        // Timeout or error - check if we should continue
        if (!is_recording_) break;
        continue;
      }

      // Get next packet size
      hr = capture_client_->GetNextPacketSize(&packet_size);
      if (FAILED(hr)) {
        continue;
      }

      while (packet_size != 0 && is_recording_) {
        // Get buffer data
        hr = capture_client_->GetBuffer(
            &data, &num_frames_available, &flags, NULL, NULL);

        if (FAILED(hr)) {
          break;
        }

        if (data && num_frames_available > 0) {
          // Calculate buffer size in bytes
          UINT32 buffer_size = num_frames_available * wave_format_->nBlockAlign;

          // Check for silence
          if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
            // Copy audio data and send to platform thread via message
            std::vector<uint8_t>* audio_data = new std::vector<uint8_t>(data, data + buffer_size);

            // Post message to platform thread
            PostMessage(message_window_, WM_RECORDING_DATA, 0, reinterpret_cast<LPARAM>(audio_data));
          }
        }

        // Release buffer
        hr = capture_client_->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) {
          break;
        }

        // Get next packet size
        hr = capture_client_->GetNextPacketSize(&packet_size);
        if (FAILED(hr)) {
          break;
        }
      }
    }

    // Stop audio client
    audio_client_->Stop();
  });
}

void FlutterF2fSoundPlugin::StopRecording() {
  if (is_recording_) {
    is_recording_ = false;
    if (recording_thread_.joinable()) {
      recording_thread_.join();
    }
  }
}

HRESULT FlutterF2fSoundPlugin::CleanupWASAPI() {
  StopRecording();

  // Release WASAPI interfaces
  if (capture_client_) {
    capture_client_->Release();
    capture_client_ = nullptr;
  }
  if (audio_client_) {
    audio_client_->Release();
    audio_client_ = nullptr;
  }
  if (recording_device_) {
    recording_device_->Release();
    recording_device_ = nullptr;
  }

  // Close event handle
  if (recording_event_) {
    CloseHandle(recording_event_);
    recording_event_ = nullptr;
  }

  // Free wave format
  if (wave_format_) {
    CoTaskMemFree(wave_format_);
    wave_format_ = nullptr;
  }

  return S_OK;
}

// System sound capture methods (separate from recording)
HRESULT FlutterF2fSoundPlugin::InitializeSystemSoundWASAPI(const AudioConfig& config) {
  HRESULT hr = S_OK;
  char debug_msg[512];

  sprintf_s(debug_msg, sizeof(debug_msg), "Initializing system sound WASAPI...\n");
  OutputDebugStringA(debug_msg);

  // Cleanup any existing system sound WASAPI resources
  CleanupSystemSoundWASAPI();

  // Get default audio endpoint for loopback recording (render device)
  ERole role = eConsole;
  hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, role, &system_sound_device_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to get default render endpoint: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Got default render endpoint successfully\n");
  OutputDebugStringA(debug_msg);

  // Activate audio client first
  hr = system_sound_device_->Activate(
      __uuidof(IAudioClient), CLSCTX_ALL, NULL,
      (void**)&system_sound_audio_client_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to activate audio client: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Activated audio client successfully\n");
  OutputDebugStringA(debug_msg);

  // For loopback recording, we should use the mix format of the device
  // rather than trying to force our own format
  hr = system_sound_audio_client_->GetMixFormat(&system_sound_wave_format_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to get mix format: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Got mix format: %d Hz, %d channels, %d bits\n",
            system_sound_wave_format_->nSamplesPerSec,
            system_sound_wave_format_->nChannels,
            system_sound_wave_format_->wBitsPerSample);
  OutputDebugStringA(debug_msg);

  // Create event handle for event-driven system sound capture
  if (!system_sound_event_) {
    system_sound_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!system_sound_event_) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to create system sound event: 0x%08X\n", HRESULT_FROM_WIN32(GetLastError()));
      OutputDebugStringA(debug_msg);
      CoTaskMemFree(system_sound_wave_format_);
      system_sound_wave_format_ = nullptr;
      return HRESULT_FROM_WIN32(GetLastError());
    }
  }

  // Initialize audio client for loopback recording with event callback
  hr = system_sound_audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      hns_requested_duration_,  // 1 second
      0,
      system_sound_wave_format_, NULL);

  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to initialize audio client for loopback: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    CloseHandle(system_sound_event_);
    system_sound_event_ = nullptr;
    CoTaskMemFree(system_sound_wave_format_);
    system_sound_wave_format_ = nullptr;
    return hr;
  }

  // Set the event handle for system sound capture
  hr = system_sound_audio_client_->SetEventHandle(system_sound_event_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to set event handle: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    CloseHandle(system_sound_event_);
    system_sound_event_ = nullptr;
    CoTaskMemFree(system_sound_wave_format_);
    system_sound_wave_format_ = nullptr;
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Initialized audio client for loopback successfully\n");
  OutputDebugStringA(debug_msg);

  // Get buffer size
  hr = system_sound_audio_client_->GetBufferSize(&system_sound_buffer_frame_count_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to get buffer size: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Buffer frame count: %d\n", system_sound_buffer_frame_count_);
  OutputDebugStringA(debug_msg);

  // Get capture client
  hr = system_sound_audio_client_->GetService(
      __uuidof(IAudioCaptureClient),
      (void**)&system_sound_capture_client_);
  if (FAILED(hr)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to get capture client: 0x%08X\n", hr);
    OutputDebugStringA(debug_msg);
    return hr;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "System sound WASAPI initialization complete\n");
  OutputDebugStringA(debug_msg);

  return hr;
}

void FlutterF2fSoundPlugin::StartSystemSoundThread() {
  is_capturing_system_sound_ = true;

  system_sound_thread_ = std::thread([this]() {
    char debug_msg[512];

    // Set thread priority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    sprintf_s(debug_msg, sizeof(debug_msg), "Starting system sound capture thread...\n");
    OutputDebugStringA(debug_msg);

    HRESULT hr = system_sound_audio_client_->Start();
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to start system sound audio client: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      if (system_sound_event_sink_) {
        system_sound_event_sink_->Error("SYSTEM_SOUND_START_ERROR", "Failed to start system sound audio client");
      }
      is_capturing_system_sound_ = false;
      return;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "System sound audio client started successfully\n");
    OutputDebugStringA(debug_msg);

    UINT32 packet_size = 0;
    DWORD flags = 0;
    BYTE *data = nullptr;
    UINT32 num_frames_available = 0;
    int packet_count = 0;

    while (is_capturing_system_sound_) {
      // Wait for buffer event to be signaled
      DWORD wait_result = WaitForSingleObject(system_sound_event_, 2000);  // 2 second timeout
      if (wait_result != WAIT_OBJECT_0) {
        // Timeout or error - check if we should continue
        if (!is_capturing_system_sound_) break;
        continue;
      }

      // Get next packet size
      hr = system_sound_capture_client_->GetNextPacketSize(&packet_size);
      if (FAILED(hr)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "GetNextPacketSize failed: 0x%08X\n", hr);
        OutputDebugStringA(debug_msg);
        continue;
      }

      while (packet_size != 0 && is_capturing_system_sound_) {
        // Get buffer data
        hr = system_sound_capture_client_->GetBuffer(
            &data, &num_frames_available, &flags, NULL, NULL);

        if (FAILED(hr)) {
          sprintf_s(debug_msg, sizeof(debug_msg), "GetBuffer failed: 0x%08X\n", hr);
          OutputDebugStringA(debug_msg);
          break;
        }

        if (data && num_frames_available > 0) {
          // Calculate buffer size in bytes
          UINT32 buffer_size = num_frames_available * system_sound_wave_format_->nBlockAlign;

          packet_count++;

          // Log packet info
          bool is_silence = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
          if (packet_count % 100 == 0) {  // Log every 100 packets
            sprintf_s(debug_msg, sizeof(debug_msg), "System sound: packet %d, size: %d bytes, flags: 0x%08X, silence: %d\n",
                      packet_count, buffer_size, flags, is_silence);
            OutputDebugStringA(debug_msg);
          }

          // Only send non-silent data
          if (!is_silence) {
            // Copy audio data and send to platform thread via message
            std::vector<uint8_t>* audio_data = new std::vector<uint8_t>(data, data + buffer_size);

            // Post message to platform thread
            PostMessage(message_window_, WM_SYSTEM_SOUND_DATA, 0, reinterpret_cast<LPARAM>(audio_data));

            if (packet_count % 100 == 0) {
              sprintf_s(debug_msg, sizeof(debug_msg), "System sound: sent non-silent packet %d, size: %d bytes\n",
                        packet_count, buffer_size);
              OutputDebugStringA(debug_msg);
            }
          } else {
            // Log that we're skipping silent data
            if (packet_count % 100 == 0) {
              sprintf_s(debug_msg, sizeof(debug_msg), "System sound: skipping silent packet %d\n", packet_count);
              OutputDebugStringA(debug_msg);
            }
          }
        }

        // Release buffer
        hr = system_sound_capture_client_->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) {
          sprintf_s(debug_msg, sizeof(debug_msg), "ReleaseBuffer failed: 0x%08X\n", hr);
          OutputDebugStringA(debug_msg);
          break;
        }

        // Get next packet size
        hr = system_sound_capture_client_->GetNextPacketSize(&packet_size);
        if (FAILED(hr)) {
          sprintf_s(debug_msg, sizeof(debug_msg), "GetNextPacketSize (inner) failed: 0x%08X\n", hr);
          OutputDebugStringA(debug_msg);
          break;
        }
      }
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Stopping system sound capture, total packets: %d\n", packet_count);
    OutputDebugStringA(debug_msg);

    // Stop audio client
    system_sound_audio_client_->Stop();
  });
}

void FlutterF2fSoundPlugin::StopSystemSoundCapture() {
  if (is_capturing_system_sound_) {
    is_capturing_system_sound_ = false;
    if (system_sound_thread_.joinable()) {
      system_sound_thread_.join();
    }
  }
}

HRESULT FlutterF2fSoundPlugin::CleanupSystemSoundWASAPI() {
  StopSystemSoundCapture();

  // Release system sound WASAPI interfaces
  if (system_sound_capture_client_) {
    system_sound_capture_client_->Release();
    system_sound_capture_client_ = nullptr;
  }
  if (system_sound_audio_client_) {
    system_sound_audio_client_->Release();
    system_sound_audio_client_ = nullptr;
  }
  if (system_sound_device_) {
    system_sound_device_->Release();
    system_sound_device_ = nullptr;
  }

  // Close event handle
  if (system_sound_event_) {
    CloseHandle(system_sound_event_);
    system_sound_event_ = nullptr;
  }

  // Free system sound wave format
  if (system_sound_wave_format_) {
    CoTaskMemFree(system_sound_wave_format_);
    system_sound_wave_format_ = nullptr;
  }

  return S_OK;
}

HRESULT FlutterF2fSoundPlugin::InitializePlaybackWASAPI() {
  HRESULT hr = S_OK;

  // Cleanup any existing playback resources
  CleanupPlaybackWASAPI();

  // Get default playback device
  ERole role = eConsole;

  hr = device_enumerator_->GetDefaultAudioEndpoint(
      eRender, role, &playback_device_);
  if (FAILED(hr)) {
    return hr;
  }

  // Activate audio client
  hr = playback_device_->Activate(
      __uuidof(IAudioClient), CLSCTX_ALL, NULL,
      (void**)&playback_audio_client_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get default format
  hr = playback_audio_client_->GetMixFormat(&playback_wave_format_);
  if (FAILED(hr)) {
    return hr;
  }

  // Initialize audio client for playback
  REFERENCE_TIME requested_duration = 10000000;  // 1 second
  hr = playback_audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      0,  // No special flags for playback
      requested_duration, 0, playback_wave_format_, NULL);
  if (FAILED(hr)) {
    CoTaskMemFree(playback_wave_format_);
    playback_wave_format_ = nullptr;
    return hr;
  }

  // Get buffer size
  hr = playback_audio_client_->GetBufferSize(&playback_buffer_frame_count_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get render client
  hr = playback_audio_client_->GetService(
      __uuidof(IAudioRenderClient),
      (void**)&render_client_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get audio session control for volume
  hr = playback_audio_client_->GetService(
      __uuidof(ISimpleAudioVolume),
      (void**)&simple_audio_volume_);
  if (FAILED(hr)) {
    // Continue even if volume control is not available
  }

  return hr;
}

void FlutterF2fSoundPlugin::StartPlaybackThread() {
  is_playing_ = true;
  is_paused_ = false;
  current_position_ = 0.0;

  OutputDebugStringA("Starting playback thread, position reset to 0.0\n");

  playback_thread_ = std::thread([this]() {
    std::string playback_path = current_playback_path_;

    // Check if this is a URL and download if necessary (now in background thread)
    char debug_msg[512];
    if (IsURL(playback_path)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Detected URL, downloading audio file in background...\n");
      OutputDebugStringA(debug_msg);

      std::string local_path;
      HRESULT hr = DownloadAudioFile(playback_path, local_path);
      if (FAILED(hr)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "Failed to download audio file: 0x%08X\n", hr);
        OutputDebugStringA(debug_msg);
        is_playing_ = false;
        return;
      }

      sprintf_s(debug_msg, sizeof(debug_msg), "Downloaded to: %s\n", local_path.c_str());
      OutputDebugStringA(debug_msg);

      playback_path = local_path;
    }

    // Read audio file first
    std::vector<uint8_t> audio_data;
    WAVEFORMATEX* file_format = nullptr;

    sprintf_s(debug_msg, sizeof(debug_msg), "Attempting to read audio file: %s\n", playback_path.c_str());
    OutputDebugStringA(debug_msg);

    HRESULT hr = ReadAudioFile(playback_path, audio_data, &file_format);
    if (FAILED(hr)) {
      // Log error for debugging
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read audio file, HRESULT: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      is_playing_ = false;
      if (file_format) {
        CoTaskMemFree(file_format);
      }
      return;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Successfully read audio file, size: %zu bytes\n", audio_data.size());
    OutputDebugStringA(debug_msg);

    // Log file format details
    if (file_format) {
      sprintf_s(debug_msg, sizeof(debug_msg),
        "File format: %d Hz, %d channels, %d bits, format tag: 0x%04X\n",
        file_format->nSamplesPerSec,
        file_format->nChannels,
        file_format->wBitsPerSample,
        file_format->wFormatTag);
      OutputDebugStringA(debug_msg);
    }

    // Log playback device format details
    sprintf_s(debug_msg, sizeof(debug_msg),
      "Playback device format: %d Hz, %d channels, %d bits, format tag: 0x%04X\n",
      playback_wave_format_->nSamplesPerSec,
      playback_wave_format_->nChannels,
      playback_wave_format_->wBitsPerSample,
      playback_wave_format_->wFormatTag);
    OutputDebugStringA(debug_msg);

    if (audio_data.empty()) {
      // Log error for debugging
      OutputDebugStringA("Audio file is empty\n");
      is_playing_ = false;
      if (file_format) {
        CoTaskMemFree(file_format);
      }
      return;
    }

    // Convert audio format if needed
    std::vector<uint8_t> converted_data;
    if (file_format) {
      // Check if format conversion is needed
      bool format_match =
          (file_format->nChannels == playback_wave_format_->nChannels) &&
          (file_format->nSamplesPerSec == playback_wave_format_->nSamplesPerSec) &&
          (file_format->wBitsPerSample == playback_wave_format_->wBitsPerSample) &&
          (file_format->wFormatTag == playback_wave_format_->wFormatTag);

      sprintf_s(debug_msg, sizeof(debug_msg), "Format match: %d\n", format_match);
      OutputDebugStringA(debug_msg);

      if (!format_match) {
        OutputDebugStringA("Format mismatch detected, starting conversion...\n");
        // Convert to playback format
        hr = ConvertAudioFormat(file_format, audio_data, playback_wave_format_, converted_data);
        if (FAILED(hr)) {
          sprintf_s(debug_msg, sizeof(debug_msg), "Failed to convert audio format, HRESULT: 0x%08X\n", hr);
          OutputDebugStringA(debug_msg);
          is_playing_ = false;
          CoTaskMemFree(file_format);
          return;
        }
        sprintf_s(debug_msg, sizeof(debug_msg), "Audio format converted successfully, size: %zu bytes\n", converted_data.size());
        OutputDebugStringA(debug_msg);
      } else {
        // Format matches, use original data
        OutputDebugStringA("Format matches, using original data\n");
        converted_data = std::move(audio_data);
      }

      CoTaskMemFree(file_format);
    } else {
      // No format info, use raw data
      converted_data = std::move(audio_data);
    }

    if (converted_data.empty()) {
      OutputDebugStringA("Converted audio data is empty\n");
      is_playing_ = false;
      return;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Audio data ready for playback, size: %zu bytes\n", converted_data.size());
    OutputDebugStringA(debug_msg);

    // Set thread priority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Start audio client
    hr = playback_audio_client_->Start();
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to start audio client, HRESULT: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      is_playing_ = false;
      return;
    }

    OutputDebugStringA("Audio client started successfully, beginning playback...\n");

    // Set volume
    SetPlaybackVolume(current_volume_);

    // Verify volume was set
    double check_volume = 0.0;
    GetPlaybackVolume(&check_volume);
    sprintf_s(debug_msg, sizeof(debug_msg), "Volume verification: requested=%.2f, actual=%.2f\n",
              current_volume_.load(), check_volume);
    OutputDebugStringA(debug_msg);

    UINT32 data_index = 0;
    const UINT32 bytes_per_frame = playback_wave_format_->nBlockAlign;
    const double samples_per_second = static_cast<double>(playback_wave_format_->nSamplesPerSec);

    sprintf_s(debug_msg, sizeof(debug_msg),
      "Starting playback loop: bytes_per_frame=%u, samples_per_sec=%.0f, total_data_size=%zu\n",
      bytes_per_frame, samples_per_second, converted_data.size());
    OutputDebugStringA(debug_msg);

    // Calculate duration in seconds
    double total_frames = static_cast<double>(converted_data.size()) / static_cast<double>(bytes_per_frame);
    double duration = total_frames / samples_per_second;
    current_duration_ = duration;

    sprintf_s(debug_msg, sizeof(debug_msg), "Audio duration calculated: %.2f seconds\n", duration);
    OutputDebugStringA(debug_msg);

    UINT32 buffer_write_count = 0;

    while (is_playing_) {
      if (is_paused_) {
        Sleep(10);
        continue;
      }

      // Calculate buffer available
      UINT32 frames_available = 0;
      hr = playback_audio_client_->GetCurrentPadding(&frames_available);
      if (FAILED(hr)) {
        continue;
      }

      UINT32 frames_to_write = playback_buffer_frame_count_ - frames_available;
      if (frames_to_write == 0) {
        Sleep(10);
        continue;
      }

      // Get buffer
      BYTE* buffer = nullptr;
      hr = render_client_->GetBuffer(frames_to_write, &buffer);
      if (FAILED(hr)) {
        continue;
      }

      // Fill buffer with audio data
      UINT32 bytes_to_write = frames_to_write * bytes_per_frame;
      UINT32 bytes_available = static_cast<UINT32>(converted_data.size() - data_index);

      if (bytes_available > 0) {
        UINT32 bytes_to_copy = (bytes_to_write < bytes_available) ? bytes_to_write : bytes_available;
        memcpy(buffer, &converted_data[data_index], bytes_to_copy);

        // Log first few buffer writes
        if (buffer_write_count < 5) {
          sprintf_s(debug_msg, sizeof(debug_msg),
            "Buffer write #%u: frames=%u, bytes_to_copy=%u, data_index=%u, bytes_available=%u\n",
            buffer_write_count, frames_to_write, bytes_to_copy, data_index, bytes_available);
          OutputDebugStringA(debug_msg);
          buffer_write_count++;
        } else if (buffer_write_count == 5) {
          OutputDebugStringA("Subsequent buffer writes suppressed...\n");
          buffer_write_count++;
        }

        // Update position based on samples written, not bytes
        // Each frame contains one sample per channel
        // Store position in seconds (as integer)
        UINT32 samples_written = bytes_to_copy / bytes_per_frame;
        current_position_ = current_position_ + (samples_written / samples_per_second);

        // Log position updates periodically
        static int position_log_count = 0;
        // if ((position_log_count++ % 100) == 0) {
        //   sprintf_s(debug_msg, sizeof(debug_msg),
        //     "Position update: samples_written=%u, current_position=%.2f\n",
        //     samples_written, current_position_.load());
        //   OutputDebugStringA(debug_msg);
        // }

        data_index += bytes_to_copy;

        // Fill remaining buffer with silence if needed
        if (bytes_to_copy < bytes_to_write) {
          memset(buffer + bytes_to_copy, 0, bytes_to_write - bytes_to_copy);
        }
      } else {
        // No more data
        if (!is_looping_) {
          // Fill remaining with silence and mark for exit
          memset(buffer, 0, bytes_to_write);
          render_client_->ReleaseBuffer(frames_to_write, 0);

          // Wait for buffer to finish playing
          Sleep(500);
          break;
        } else {
          // Loop back to start
          data_index = 0;
          current_position_ = 0.0;
          memset(buffer, 0, bytes_to_write);
        }
      }

      // Release buffer
      hr = render_client_->ReleaseBuffer(frames_to_write, 0);
      if (FAILED(hr)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "ReleaseBuffer failed: 0x%08X\n", hr);
        OutputDebugStringA(debug_msg);
        break;
      }
    }

    // Stop audio client
    playback_audio_client_->Stop();
    is_playing_ = false;
    is_paused_ = false;
    current_position_ = 0.0;
    current_duration_ = 0.0;
  });
}

void FlutterF2fSoundPlugin::StopPlayback() {
  is_playing_ = false;
  if (playback_thread_.joinable()) {
    playback_thread_.join();
  }
  CleanupPlaybackWASAPI();
}

HRESULT FlutterF2fSoundPlugin::CleanupPlaybackWASAPI() {
  is_playing_ = false;
  is_paused_ = false;
  current_position_ = 0.0;
  current_duration_ = 0.0;

  // Release playback interfaces
  if (render_client_) {
    render_client_->Release();
    render_client_ = nullptr;
  }
  if (simple_audio_volume_) {
    simple_audio_volume_->Release();
    simple_audio_volume_ = nullptr;
  }
  if (playback_audio_client_) {
    playback_audio_client_->Release();
    playback_audio_client_ = nullptr;
  }
  if (playback_device_) {
    playback_device_->Release();
    playback_device_ = nullptr;
  }

  // Free playback wave format
  if (playback_wave_format_) {
    CoTaskMemFree(playback_wave_format_);
    playback_wave_format_ = nullptr;
  }

  return S_OK;
}

HRESULT FlutterF2fSoundPlugin::ReadAudioFile(const std::string& path, std::vector<uint8_t>& audio_data, WAVEFORMATEX** format) {
  char debug_msg[512];

  // Check if file is MP3 and use Media Foundation
  if (IsMP3(path)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Detected MP3 file, using Media Foundation decoder\n");
    OutputDebugStringA(debug_msg);
    return ReadAudioFileWithMF(path, audio_data, format);
  }

  // Otherwise, use WAV parser
  sprintf_s(debug_msg, sizeof(debug_msg), "Using WAV parser for file: %s\n", path.c_str());
  OutputDebugStringA(debug_msg);

  // Simple WAV file parser
  // Convert std::string to std::wstring for Windows file APIs
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.length(), NULL, 0);
  std::wstring wpath(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.length(), &wpath[0], size_needed);

  // Use Windows-specific wide-character file opening
  FILE* file = nullptr;
  errno_t err = _wfopen_s(&file, wpath.c_str(), L"rb");
  if (err != 0 || file == nullptr) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to open audio file: %s (errno: %d)\n", path.c_str(), err);
    OutputDebugStringA(debug_msg);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "File opened successfully\n");
  OutputDebugStringA(debug_msg);

  // Read RIFF header
  char riff[4];
  if (fread(riff, 1, 4, file) != 4) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read RIFF header\n");
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }
  sprintf_s(debug_msg, sizeof(debug_msg), "RIFF header: %.4s\n", riff);
  OutputDebugStringA(debug_msg);

  if (strncmp(riff, "RIFF", 4) != 0) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Invalid RIFF header: %.4s\n", riff);
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  // Skip file size (4 bytes)
  fseek(file, 4, SEEK_CUR);

  // Read WAVE header
  char wave[4];
  if (fread(wave, 1, 4, file) != 4) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read WAVE header\n");
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  if (strncmp(wave, "WAVE", 4) != 0) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Invalid WAVE header: %.4s\n", wave);
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "WAVE header verified\n");
  OutputDebugStringA(debug_msg);

  // Read fmt chunk
  char fmt[4];
  if (fread(fmt, 1, 4, file) != 4) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read fmt chunk ID\n");
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Chunk ID: %.4s\n", fmt);
  OutputDebugStringA(debug_msg);

  if (strncmp(fmt, "fmt ", 4) != 0) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Expected fmt chunk, got: %.4s\n", fmt);
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  // Read fmt chunk size
  uint32_t fmt_size;
  if (fread(&fmt_size, 4, 1, file) != 1) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read fmt chunk size\n");
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "fmt chunk size: %u\n", fmt_size);
  OutputDebugStringA(debug_msg);

  // Read format (basic 16 bytes)
  WAVEFORMATEX wave_format;
  if (fread(&wave_format, 16, 1, file) != 1) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read WAVEFORMATEX\n");
    OutputDebugStringA(debug_msg);
    fclose(file);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Audio format: %d Hz, %d channels, %d bits, tag: 0x%04X\n",
            wave_format.nSamplesPerSec, wave_format.nChannels,
            wave_format.wBitsPerSample, wave_format.wFormatTag);
  OutputDebugStringA(debug_msg);

  // Skip extra format bytes (if any)
  int extra_bytes_to_skip = fmt_size - 16;
  if (extra_bytes_to_skip > 0) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Skipping %d extra format bytes\n", extra_bytes_to_skip);
    OutputDebugStringA(debug_msg);
    fseek(file, extra_bytes_to_skip, SEEK_CUR);
  }

  // Allocate format structure
  *format = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
  if (*format == nullptr) {
    fclose(file);
    return E_OUTOFMEMORY;
  }

  // Copy the format we read
  **format = wave_format;
  (*format)->cbSize = 0;  // No extra bytes

  // Find data chunk (skip any other chunks)
  char data_header[4];
  bool found_data = false;

  while (fread(data_header, 1, 4, file) == 4) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Found chunk: %.4s\n", data_header);
    OutputDebugStringA(debug_msg);

    if (strncmp(data_header, "data", 4) == 0) {
      found_data = true;
      break;
    }

    // Skip this chunk
    uint32_t chunk_size;
    if (fread(&chunk_size, 4, 1, file) != 1) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read chunk size\n");
      OutputDebugStringA(debug_msg);
      CoTaskMemFree(*format);
      *format = nullptr;
      fclose(file);
      return E_FAIL;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Skipping chunk, size: %u\n", chunk_size);
    OutputDebugStringA(debug_msg);

    fseek(file, chunk_size, SEEK_CUR);
  }

  if (!found_data || feof(file) || ferror(file)) {
    sprintf_s(debug_msg, sizeof(debug_msg), "data chunk not found or read error\n");
    OutputDebugStringA(debug_msg);
    CoTaskMemFree(*format);
    *format = nullptr;
    fclose(file);
    return E_FAIL;
  }

  // Read data size
  uint32_t data_size;
  if (fread(&data_size, 4, 1, file) != 1) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read data size\n");
    OutputDebugStringA(debug_msg);
    CoTaskMemFree(*format);
    *format = nullptr;
    fclose(file);
    return E_FAIL;
  }

  sprintf_s(debug_msg, sizeof(debug_msg), "Data chunk size: %u bytes\n", data_size);
  OutputDebugStringA(debug_msg);

  // Read audio data
  audio_data.resize(data_size);
  size_t bytes_read = fread(audio_data.data(), 1, data_size, file);
  if (bytes_read != data_size) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read audio data: expected %u, got %zu\n", data_size, bytes_read);
    OutputDebugStringA(debug_msg);
    CoTaskMemFree(*format);
    *format = nullptr;
    fclose(file);
    return E_FAIL;
  }

  fclose(file);

  sprintf_s(debug_msg, sizeof(debug_msg), "Successfully read audio file, size: %zu bytes\n", audio_data.size());
  OutputDebugStringA(debug_msg);

  return S_OK;
}

HRESULT FlutterF2fSoundPlugin::ConvertAudioFormat(const WAVEFORMATEX* input_format, const std::vector<uint8_t>& input_data,
                                                 const WAVEFORMATEX* output_format, std::vector<uint8_t>& output_data) {
  char debug_msg[512];

  sprintf_s(debug_msg, sizeof(debug_msg),
    "ConvertAudioFormat: input=%dHz/%dch/%dbit(0x%04X), output=%dHz/%dch/%dbit(0x%04X)\n",
    input_format->nSamplesPerSec, input_format->nChannels, input_format->wBitsPerSample, input_format->wFormatTag,
    output_format->nSamplesPerSec, output_format->nChannels, output_format->wBitsPerSample, output_format->wFormatTag);
  OutputDebugStringA(debug_msg);

  // Check if we need 16-bit PCM to 32-bit Float conversion
  bool need_bit_depth_conversion = (input_format->wBitsPerSample != output_format->wBitsPerSample);
  bool need_channel_conversion = (input_format->nChannels != output_format->nChannels);
  bool need_rate_conversion = (input_format->nSamplesPerSec != output_format->nSamplesPerSec);

  // Calculate input sample count
  size_t input_samples = input_data.size() / (input_format->nChannels * (input_format->wBitsPerSample / 8));

  // Calculate output frame count (samples per channel)
  size_t output_frames = input_samples;
  if (need_rate_conversion) {
    // Simple resampling - adjust frame count based on sample rate ratio
    output_frames = (size_t)(input_samples * ((double)output_format->nSamplesPerSec / input_format->nSamplesPerSec));
  }

  // Calculate output data size
  size_t output_data_size = output_frames * output_format->nChannels * (output_format->wBitsPerSample / 8);
  output_data.resize(output_data_size);

  sprintf_s(debug_msg, sizeof(debug_msg),
    "Conversion: input_samples=%zu, output_frames=%zu, output_size=%zu\n",
    input_samples, output_frames, output_data_size);
  OutputDebugStringA(debug_msg);

  // Case 1: 16-bit PCM stereo to 32-bit Float stereo (same sample rate)
  if (input_format->wBitsPerSample == 16 && output_format->wBitsPerSample == 32 &&
      input_format->nChannels == output_format->nChannels &&
      input_format->nSamplesPerSec == output_format->nSamplesPerSec) {

    sprintf_s(debug_msg, sizeof(debug_msg), "Converting 16-bit PCM to 32-bit Float\n");
    OutputDebugStringA(debug_msg);

    const int16_t* input = reinterpret_cast<const int16_t*>(input_data.data());
    float* output = reinterpret_cast<float*>(output_data.data());
    size_t input_values = input_data.size() / sizeof(int16_t);

    // Convert each 16-bit sample to 32-bit float
    // Formula: float = int16 / 32768.0
    for (size_t i = 0; i < input_values; i++) {
      output[i] = static_cast<float>(input[i]) / 32768.0f;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Conversion complete: %zu samples converted\n", input_values);
    OutputDebugStringA(debug_msg);
    return S_OK;
  }

  // Case 2: 16-bit PCM mono to 32-bit Float stereo (same sample rate)
  if (input_format->wBitsPerSample == 16 && output_format->wBitsPerSample == 32 &&
      input_format->nChannels == 1 && output_format->nChannels == 2 &&
      input_format->nSamplesPerSec == output_format->nSamplesPerSec) {

    sprintf_s(debug_msg, sizeof(debug_msg), "Converting 16-bit PCM mono to 32-bit Float stereo\n");
    OutputDebugStringA(debug_msg);

    const int16_t* input = reinterpret_cast<const int16_t*>(input_data.data());
    float* output = reinterpret_cast<float*>(output_data.data());
    size_t input_samples_count = input_data.size() / sizeof(int16_t);

    // Convert mono to stereo and 16-bit to 32-bit float
    for (size_t i = 0; i < input_samples_count; i++) {
      float sample = static_cast<float>(input[i]) / 32768.0f;
      output[i * 2] = sample;      // Left channel
      output[i * 2 + 1] = sample;  // Right channel
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Conversion complete: %zu samples to stereo\n", input_samples_count);
    OutputDebugStringA(debug_msg);
    return S_OK;
  }

  // Case 3: 16-bit PCM to 32-bit Float with sample rate conversion (stereo)
  if (input_format->wBitsPerSample == 16 && output_format->wBitsPerSample == 32 &&
      input_format->nChannels == output_format->nChannels) {

    sprintf_s(debug_msg, sizeof(debug_msg), "Converting 16-bit PCM to 32-bit Float with sample rate conversion\n");
    OutputDebugStringA(debug_msg);

    const int16_t* input = reinterpret_cast<const int16_t*>(input_data.data());
    float* output = reinterpret_cast<float*>(output_data.data());

    double sample_rate_ratio = (double)output_format->nSamplesPerSec / (double)input_format->nSamplesPerSec;

    // Convert with sample rate conversion using linear interpolation
    for (size_t i = 0; i < output_frames; i++) {
      // Calculate source sample position (floating point)
      double src_pos = i / sample_rate_ratio;
      size_t src_index = static_cast<size_t>(src_pos);

      // Linear interpolation between two samples
      if (src_index + 1 < input_samples) {
        float sample1 = static_cast<float>(input[src_index * input_format->nChannels]) / 32768.0f;
        float sample2 = static_cast<float>(input[(src_index + 1) * input_format->nChannels]) / 32768.0f;
        float frac = static_cast<float>(src_pos - src_index);

        for (int ch = 0; ch < input_format->nChannels; ch++) {
          output[i * input_format->nChannels + ch] = sample1 * (1.0f - frac) + sample2 * frac;
        }
      } else {
        // Near the end, just use the last sample
        for (int ch = 0; ch < input_format->nChannels; ch++) {
          output[i * input_format->nChannels + ch] = static_cast<float>(input[src_index * input_format->nChannels + ch]) / 32768.0f;
        }
      }
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Conversion complete: %zu frames converted with resampling\n", output_frames);
    OutputDebugStringA(debug_msg);
    return S_OK;
  }

  // Case 4: Same format, just copy
  if (!need_bit_depth_conversion && !need_channel_conversion && !need_rate_conversion) {
    sprintf_s(debug_msg, sizeof(debug_msg), "No conversion needed, copying data\n");
    OutputDebugStringA(debug_msg);
    output_data = input_data;
    return S_OK;
  }

  // Case 5: Unsupported conversion - log error and copy as fallback
  sprintf_s(debug_msg, sizeof(debug_msg),
    "WARNING: Unsupported format conversion, copying as fallback. This may produce no audio!\n");
  OutputDebugStringA(debug_msg);
  output_data = input_data;

  return S_OK;
}

HRESULT FlutterF2fSoundPlugin::SetPlaybackVolume(double volume) {
  char debug_msg[256];
  sprintf_s(debug_msg, sizeof(debug_msg), "SetPlaybackVolume: volume=%.2f\n", volume);
  OutputDebugStringA(debug_msg);

  if (simple_audio_volume_) {
    HRESULT hr = simple_audio_volume_->SetMasterVolume(static_cast<float>(volume), NULL);
    if (SUCCEEDED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Volume set successfully: %.2f\n", volume);
      OutputDebugStringA(debug_msg);
    } else {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to set volume: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
    }
    return hr;
  } else {
    OutputDebugStringA("WARNING: simple_audio_volume_ is NULL, volume not set\n");
  }
  return S_OK;
}

HRESULT FlutterF2fSoundPlugin::GetPlaybackVolume(double* volume) {
  if (simple_audio_volume_) {
    float float_volume = 0.0f;
    HRESULT hr = simple_audio_volume_->GetMasterVolume(&float_volume);
    if (SUCCEEDED(hr)) {
      *volume = static_cast<double>(float_volume);
    }
    return hr;
  }
  return S_OK;
}

bool FlutterF2fSoundPlugin::IsURL(const std::string& path) {
  // Check if the path starts with http:// or https://
  return (path.find("http://") == 0 || path.find("https://") == 0);
}

HRESULT FlutterF2fSoundPlugin::DownloadAudioFile(const std::string& url, std::string& local_path) {
  char debug_msg[512];

  // Get temporary file path
  char temp_path[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_path);

  char temp_file[MAX_PATH];
  GetTempFileNameA(temp_path, "audio", 0, temp_file);

  // Extract file extension from URL
  std::string extension = ".wav";  // Default extension
  size_t query_pos = url.find('?');
  size_t url_end = (query_pos != std::string::npos) ? query_pos : url.length();
  size_t last_dot = url.find_last_of('.', url_end);

  if (last_dot != std::string::npos && last_dot < url_end) {
    extension = url.substr(last_dot, url_end - last_dot);
    // Convert extension to lowercase
    for (char& c : extension) {
      if (c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }
    }
  }

  // Remove the .tmp extension and add the correct extension from URL
  local_path = temp_file;
  local_path = local_path.substr(0, local_path.find_last_of('.'));
  local_path += extension;

  sprintf_s(debug_msg, sizeof(debug_msg), "Downloading URL: %s to: %s (extension: %s)\n",
            url.c_str(), local_path.c_str(), extension.c_str());
  OutputDebugStringA(debug_msg);

  // Use WinHTTP for downloading
  HINTERNET hSession = NULL;
  HINTERNET hConnect = NULL;
  HINTERNET hRequest = NULL;

  try {
    // Convert URL to wide string
    int url_len = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, NULL, 0);
    std::wstring w_url(url_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &w_url[0], url_len);

    // Open WinHTTP session
    hSession = WinHttpOpen(
      L"FlutterF2FSound/1.0",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0
    );

    if (!hSession) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to open WinHTTP session: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      return E_FAIL;
    }

    // Parse URL and connect to server
    URL_COMPONENTSW url_components = {0};
    url_components.dwStructSize = sizeof(URL_COMPONENTSW);

    wchar_t hostname[256];
    wchar_t url_path[1024];
    url_components.lpszHostName = hostname;
    url_components.dwHostNameLength = sizeof(hostname) / sizeof(wchar_t);
    url_components.lpszUrlPath = url_path;
    url_components.dwUrlPathLength = sizeof(url_path) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(w_url.c_str(), 0, 0, &url_components)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to parse URL: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "Connecting to: %ls:%d\n", hostname, url_components.nPort);
    OutputDebugStringA(debug_msg);

    // Connect to server
    hConnect = WinHttpConnect(
      hSession,
      hostname,
      url_components.nPort,
      0
    );

    if (!hConnect) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to connect to server: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    // Open request
    hRequest = WinHttpOpenRequest(
      hConnect,
      L"GET",
      url_path,
      NULL,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      (url_components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0
    );

    if (!hRequest) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to open request: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to send request: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to receive response: %d\n", GetLastError());
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    // Check status code
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
      hRequest,
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &status_code,
      &status_size,
      WINHTTP_NO_HEADER_INDEX
    );

    if (status_code != 200) {
      sprintf_s(debug_msg, sizeof(debug_msg), "HTTP status code: %d\n", status_code);
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    sprintf_s(debug_msg, sizeof(debug_msg), "HTTP 200 OK, starting download...\n");
    OutputDebugStringA(debug_msg);

    // Get content length
    DWORD content_length = 0;
    DWORD content_size = sizeof(content_length);
    WinHttpQueryHeaders(
      hRequest,
      WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &content_length,
      &content_size,
      WINHTTP_NO_HEADER_INDEX
    );

    sprintf_s(debug_msg, sizeof(debug_msg), "Content length: %d bytes\n", content_length);
    OutputDebugStringA(debug_msg);

    // Download data
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, local_path.c_str(), "wb");
    if (err != 0 || file == nullptr) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to create temporary file: %d\n", err);
      OutputDebugStringA(debug_msg);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return E_FAIL;
    }

    DWORD bytes_available = 0;
    DWORD total_bytes = 0;
    BYTE buffer[8192];

    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
      DWORD bytes_read = 0;
      if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytes_read)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read data: %d\n", GetLastError());
        OutputDebugStringA(debug_msg);
        break;
      }

      if (bytes_read > 0) {
        fwrite(buffer, 1, bytes_read, file);
        total_bytes += bytes_read;

        if (total_bytes % (100 * 1024) == 0) {  // Log every 100KB
          sprintf_s(debug_msg, sizeof(debug_msg), "Downloaded: %d bytes\n", total_bytes);
          OutputDebugStringA(debug_msg);
        }
      }
    }

    fclose(file);

    sprintf_s(debug_msg, sizeof(debug_msg), "Download complete: %d bytes\n", total_bytes);
    OutputDebugStringA(debug_msg);

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return S_OK;
  } catch (...) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Exception during download\n");
    OutputDebugStringA(debug_msg);

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return E_FAIL;
  }
}

// Window procedure for handling messages on the platform thread
LRESULT CALLBACK FlutterF2fSoundPlugin::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_CREATE) {
    LPCREATESTRUCT pCreate = reinterpret_cast<LPCREATESTRUCT>(lParam);
    FlutterF2fSoundPlugin* plugin = reinterpret_cast<FlutterF2fSoundPlugin*>(pCreate->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(plugin));
    return 0;
  }

  FlutterF2fSoundPlugin* plugin = reinterpret_cast<FlutterF2fSoundPlugin*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (!plugin) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  if (uMsg == WM_RECORDING_DATA) {
    // Process recording data on the platform thread
    std::vector<uint8_t>* audio_data = reinterpret_cast<std::vector<uint8_t>*>(lParam);
    if (audio_data) {
      plugin->ProcessRecordingData(*audio_data);
      delete audio_data;
    }
    return 0;
  }

  if (uMsg == WM_SYSTEM_SOUND_DATA) {
    // Process system sound data on the platform thread
    std::vector<uint8_t>* audio_data = reinterpret_cast<std::vector<uint8_t>*>(lParam);
    if (audio_data) {
      plugin->ProcessSystemSoundData(*audio_data);
      delete audio_data;
    }
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Process recording data and send to Flutter (called on platform thread)
void FlutterF2fSoundPlugin::ProcessRecordingData(const std::vector<uint8_t>& audio_data) {
  if (recording_event_sink_ && !audio_data.empty()) {
    // Create EncodableList from audio data
    flutter::EncodableList encodable_audio_data;
    for (uint8_t byte : audio_data) {
      encodable_audio_data.push_back(static_cast<int>(byte));
    }

    // Send data to Flutter (now on platform thread)
    recording_event_sink_->Success(flutter::EncodableValue(encodable_audio_data));
  }
}

// Process system sound data and send to Flutter (called on platform thread)
void FlutterF2fSoundPlugin::ProcessSystemSoundData(const std::vector<uint8_t>& audio_data) {
  static int data_packet_count = 0;
  data_packet_count++;

  if (system_sound_event_sink_ && !audio_data.empty()) {
    // Log every 50 packets to verify data is reaching Flutter
    if (data_packet_count % 50 == 0) {
      char debug_msg[512];
      sprintf_s(debug_msg, sizeof(debug_msg), "ProcessSystemSoundData: Sending packet %d to Flutter, size: %zu bytes\n",
                data_packet_count, audio_data.size());
      OutputDebugStringA(debug_msg);
    }

    // Create EncodableList from audio data
    flutter::EncodableList encodable_audio_data;
    for (uint8_t byte : audio_data) {
      encodable_audio_data.push_back(static_cast<int>(byte));
    }

    // Send data to Flutter (now on platform thread)
    system_sound_event_sink_->Success(flutter::EncodableValue(encodable_audio_data));
  } else {
    if (!system_sound_event_sink_) {
      OutputDebugStringA("ProcessSystemSoundData: No event sink!\n");
    }
  }
}

void FlutterF2fSoundPlugin::PlaybackStreamThread(const std::string& path) {
  // This implementation uses a simple file reading approach for demonstration
  // A production implementation would use Media Foundation for better format support
  
  OutputDebugStringA(("Starting playback stream thread for: " + path + "\n").c_str());
  
  // Check if it's a network URL
  if (path.find("http://") == 0 || path.find("https://") == 0) {
    // For network URLs, use WinHttp to download the content
    HINTERNET hSession = WinHttpOpen(L"FlutterF2fSound/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    
    if (!hSession) {
      OutputDebugStringA("Failed to create WinHttp session");
      return;
    }
    
    // Convert path to wide string
    int wide_length = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wide_path(wide_length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wide_path[0], wide_length);
    
    URL_COMPONENTS url_components = {0};
    url_components.dwStructSize = sizeof(URL_COMPONENTS);
    
    // Initialize URL components
    wchar_t host_name[256] = {0};
    url_components.lpszHostName = host_name;
    url_components.dwHostNameLength = sizeof(host_name) / sizeof(wchar_t);
    
    INTERNET_PORT port = 0;
    url_components.nPort = port;
    
    wchar_t path_component[1024] = {0};
    url_components.lpszUrlPath = path_component;
    url_components.dwUrlPathLength = sizeof(path_component) / sizeof(wchar_t);
    
    wchar_t extra_info[1024] = {0};
    url_components.lpszExtraInfo = extra_info;
    url_components.dwExtraInfoLength = sizeof(extra_info) / sizeof(wchar_t);
    
    if (!WinHttpCrackUrl(wide_path.c_str(), 0, 0, &url_components)) {
      OutputDebugStringA("Failed to parse URL");
      WinHttpCloseHandle(hSession);
      return;
    }
    
    // Use default HTTPS port if not specified
    INTERNET_PORT actual_port = url_components.nPort != 0 ? url_components.nPort : 443;
    bool is_https = (url_components.nScheme == INTERNET_SCHEME_HTTPS);
    
    HINTERNET hConnect = WinHttpConnect(hSession,
        host_name,
        actual_port,
        0);
    
    if (!hConnect) {
      OutputDebugStringA("Failed to connect to host");
      WinHttpCloseHandle(hSession);
      return;
    }
    
    // Build request path (include extra info)
    std::wstring request_path = path_component;
    if (url_components.lpszExtraInfo && url_components.dwExtraInfoLength > 0) {
      request_path += extra_info;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"GET",
        request_path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        is_https ? WINHTTP_FLAG_SECURE : 0);
    
    if (!hRequest) {
      OutputDebugStringA("Failed to open request");
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return;
    }
    
    // Send request
    if (!WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0)) {
      OutputDebugStringA("Failed to send request");
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return;
    }
    
    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
      OutputDebugStringA("Failed to receive response");
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return;
    }
    
    // Stream the data
    const size_t buffer_size = 4096;
    BYTE buffer[buffer_size];
    DWORD bytes_read;
    
    while (is_playback_streaming_ && WinHttpReadData(hRequest, buffer, buffer_size, &bytes_read) && bytes_read > 0) {
      // Convert to vector
      std::vector<uint8_t> audio_data(buffer, buffer + bytes_read);
      
      // Process and send the audio data
      ProcessPlaybackStreamData(audio_data);
      
      // Simulate playback speed (adjust based on actual format)
      Sleep(10); // Simple delay to control streaming rate
    }
    
    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    OutputDebugStringA("Network URL stream completed\n");
    is_playback_streaming_ = false;
    return;
  }
  
  // For local files
  HANDLE file_handle = CreateFileA(
      path.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);
  
  if (file_handle == INVALID_HANDLE_VALUE) {
    OutputDebugStringA("Failed to open audio file");
    return;
  }
  
  // Read the file header to determine format (simplified)
  BYTE header_buffer[1024];
  DWORD bytes_read;
  if (!ReadFile(file_handle, header_buffer, sizeof(header_buffer), &bytes_read, nullptr)) {
    OutputDebugStringA("Failed to read audio file header");
    CloseHandle(file_handle);
    return;
  }
  
  // Simple WAV file validation
  bool is_wav = false;
  if (bytes_read >= 4 && memcmp(header_buffer, "RIFF", 4) == 0 &&
      bytes_read >= 8 && memcmp(header_buffer + 8, "WAVE", 4) == 0) {
    is_wav = true;
  }
  
  // If it's not a WAV file, we'll still try to stream it as raw data
  if (!is_wav) {
    OutputDebugStringA("Warning: Not a recognized WAV file, streaming as raw data");
  }
  
  // Reset file pointer to beginning
  SetFilePointer(file_handle, 0, nullptr, FILE_BEGIN);
  
  // Stream the audio data
  const size_t buffer_size = 4096;
  BYTE buffer[buffer_size];
  
  while (is_playback_streaming_) {
    if (!ReadFile(file_handle, buffer, buffer_size, &bytes_read, nullptr) || bytes_read == 0) {
      // End of file or error
      break;
    }
    
    // Convert to vector
    std::vector<uint8_t> audio_data(buffer, buffer + bytes_read);
    
    // Process and send the audio data
    ProcessPlaybackStreamData(audio_data);
    
    // Simulate playback speed (adjust based on actual format)
    Sleep(10); // Simple delay to control streaming rate
  }
  
  CloseHandle(file_handle);
  OutputDebugStringA("Playback stream thread finished\n");
  is_playback_streaming_ = false;
}

bool FlutterF2fSoundPlugin::IsMP3(const std::string& path) {
  // Check if file extension is .mp3 (case insensitive)
  if (path.length() < 4) return false;

  std::string ext = path.substr(path.length() - 4);
  // Convert to lowercase manually
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';
    }
  }

  return (ext == ".mp3");
}

HRESULT FlutterF2fSoundPlugin::ReadAudioFileWithMF(const std::string& path, std::vector<uint8_t>& audio_data, WAVEFORMATEX** format) {
  char debug_msg[512];
  sprintf_s(debug_msg, sizeof(debug_msg), "Reading audio file with Media Foundation: %s\n", path.c_str());
  OutputDebugStringA(debug_msg);

  IMFSourceReader* source_reader = nullptr;
  IMFMediaType* media_type = nullptr;
  HRESULT hr = S_OK;

  // Convert path to wide string
  int path_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
  std::wstring wpath(path_len, 0);
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], path_len);

  try {
    // Create source reader from file
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), NULL, &source_reader);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to create source reader: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      return hr;
    }

    // Configure the source reader to output uncompressed PCM audio
    hr = source_reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to deselect streams: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      source_reader->Release();
      return hr;
    }

    hr = source_reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to select audio stream: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      source_reader->Release();
      return hr;
    }

    // Create media type for uncompressed audio
    hr = MFCreateMediaType(&media_type);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to create media type: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      source_reader->Release();
      return hr;
    }

    hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to set major type: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      media_type->Release();
      source_reader->Release();
      return hr;
    }

    hr = media_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to set subtype: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      media_type->Release();
      source_reader->Release();
      return hr;
    }

    hr = source_reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, media_type);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to set current media type: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      media_type->Release();
      source_reader->Release();
      return hr;
    }

    media_type->Release();
    media_type = nullptr;

    // Get the current media type to get the format
    hr = source_reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &media_type);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to get current media type: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      source_reader->Release();
      return hr;
    }

    // Create a WAVEFORMATEX structure from the media type
    UINT32 format_size = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(media_type, &wave_format_ex_, &format_size);
    if (FAILED(hr)) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to create wave format: 0x%08X\n", hr);
      OutputDebugStringA(debug_msg);
      media_type->Release();
      source_reader->Release();
      return hr;
    }

    // Copy the format for the caller
    *format = (WAVEFORMATEX*)CoTaskMemAlloc(format_size);
    if (*format == nullptr) {
      sprintf_s(debug_msg, sizeof(debug_msg), "Failed to allocate format memory\n");
      OutputDebugStringA(debug_msg);
      CoTaskMemFree(wave_format_ex_);
      media_type->Release();
      source_reader->Release();
      return E_OUTOFMEMORY;
    }

    memcpy(*format, wave_format_ex_, format_size);

    sprintf_s(debug_msg, sizeof(debug_msg), "Audio format: %d Hz, %d channels, %d bits\n",
              (*format)->nSamplesPerSec, (*format)->nChannels, (*format)->wBitsPerSample);
    OutputDebugStringA(debug_msg);

    media_type->Release();
    media_type = nullptr;

    // Read audio data
    std::vector<uint8_t> temp_buffer;
    const DWORD buffer_size = 4096;
    BYTE* buffer = new BYTE[buffer_size];

    while (true) {
      DWORD flags = 0;
      IMFSample* sample = nullptr;

      hr = source_reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample);
      if (FAILED(hr)) {
        sprintf_s(debug_msg, sizeof(debug_msg), "Failed to read sample: 0x%08X\n", hr);
        OutputDebugStringA(debug_msg);
        break;
      }

      if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        sprintf_s(debug_msg, sizeof(debug_msg), "End of audio stream reached\n");
        OutputDebugStringA(debug_msg);
        if (sample) {
          sample->Release();
        }
        break;
      }

      if (sample) {
        IMFMediaBuffer* media_buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&media_buffer);
        if (SUCCEEDED(hr)) {
          BYTE* audio_data_ptr = nullptr;
          DWORD audio_data_length = 0;

          hr = media_buffer->Lock(&audio_data_ptr, NULL, &audio_data_length);
          if (SUCCEEDED(hr)) {
            temp_buffer.insert(temp_buffer.end(), audio_data_ptr, audio_data_ptr + audio_data_length);
            media_buffer->Unlock();
          }

          media_buffer->Release();
        }

        sample->Release();
      }
    }

    delete[] buffer;

    sprintf_s(debug_msg, sizeof(debug_msg), "Read %zu bytes of audio data\n", temp_buffer.size());
    OutputDebugStringA(debug_msg);

    // Calculate expected duration based on data size and format
    UINT32 bytes_per_sample = (*format)->wBitsPerSample / 8;
    UINT32 bytes_per_frame_calc = (*format)->nChannels * bytes_per_sample;
    double bytes_per_second_calc = (*format)->nSamplesPerSec * bytes_per_frame_calc;
    double expected_duration = temp_buffer.size() / bytes_per_second_calc;

    sprintf_s(debug_msg, sizeof(debug_msg),
      "Audio stats: size=%zu bytes, bytes_per_frame=%u, bytes_per_sec=%.0f, expected_duration=%.2f sec\n",
      temp_buffer.size(), bytes_per_frame_calc, bytes_per_second_calc, expected_duration);
    OutputDebugStringA(debug_msg);

    audio_data = std::move(temp_buffer);

    source_reader->Release();
    CoTaskMemFree(wave_format_ex_);

    return S_OK;
  } catch (...) {
    sprintf_s(debug_msg, sizeof(debug_msg), "Exception during Media Foundation decoding\n");
    OutputDebugStringA(debug_msg);

    if (source_reader) source_reader->Release();
    if (media_type) media_type->Release();

    return E_FAIL;
  }
}

}  // namespace flutter_f2f_sound
