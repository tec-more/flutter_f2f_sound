import Cocoa
import FlutterMacOS
import AVFoundation

public class FlutterF2fSoundPlugin: NSObject, FlutterPlugin, FlutterStreamHandler {
    private var methodChannel: FlutterMethodChannel?
    private var eventChannel: FlutterEventChannel?
    private var eventSink: FlutterEventSink?

    // Audio playback
    private var audioPlayer: AVAudioPlayer?
    private var playbackTimer: Timer?

    // Audio recording
    private var audioRecorder: AVAudioRecorder?
    private var recordingTimer: Timer?

    // Audio playback stream
    private var playbackStreamEngine: AVAudioEngine?
    private var playbackStreamPlayerNode = AVAudioPlayerNode()
    private var playbackStreamSink: FlutterEventSink?

    // State tracking
    private var isPlaying = false
    private var isPaused = false
    private var currentPosition: Double = 0.0
    private var currentDuration: Double = 0.0
    private var currentVolume: Double = 1.0
    private var isLooping = false

    public static func register(with registrar: FlutterPluginRegistrar) {
        let methodChannel = FlutterMethodChannel(name: "flutter_f2f_sound", binaryMessenger: registrar.messenger)
        let eventChannel = FlutterEventChannel(name: "flutter_f2f_sound/recording_stream", binaryMessenger: registrar.messenger)
        let instance = FlutterF2fSoundPlugin()

        instance.methodChannel = methodChannel
        instance.eventChannel = eventChannel

        registrar.addMethodCallDelegate(instance, channel: methodChannel)
        eventChannel.setStreamHandler(instance)
    }

    public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
        switch call.method {
        case "getPlatformVersion":
            let version = ProcessInfo.processInfo.operatingSystemVersionString
            result("macOS \(version)")

        case "play":
            guard let args = call.arguments as? [String: Any],
                  let path = args["path"] as? String else {
                result(FlutterError(code: "INVALID_ARGS", message: "Path is required", details: nil))
                return
            }
            let volume = (args["volume"] as? Double) ?? 1.0
            let loop = (args["loop"] as? Bool) ?? false
            play(path: path, volume: volume, loop: loop, result: result)

        case "pause":
            pause(result: result)

        case "stop":
            stop(result: result)

        case "resume":
            resume(result: result)

        case "setVolume":
            guard let args = call.arguments as? [String: Any],
                  let volume = args["volume"] as? Double else {
                result(FlutterError(code: "INVALID_ARGS", message: "Volume is required", details: nil))
                return
            }
            setVolume(volume: volume, result: result)

        case "isPlaying":
            result(isPlaying && !isPaused)

        case "getCurrentPosition":
            result(currentPosition)

        case "getDuration":
            guard let args = call.arguments as? [String: Any],
                  let path = args["path"] as? String else {
                result(FlutterError(code: "INVALID_ARGS", message: "Path is required", details: nil))
                return
            }
            getDuration(path: path, result: result)

        case "startRecording":
            startRecording(result: result)

        case "stopRecording":
            stopRecording(result: result)

        case "startPlaybackStream":
            guard let args = call.arguments as? [String: Any],
                  let path = args["path"] as? String else {
                result(FlutterError(code: "INVALID_ARGS", message: "Path is required", details: nil))
                return
            }
            startPlaybackStream(path: path, result: result)

        default:
            result(FlutterMethodNotImplemented)
        }
    }

    // MARK: - Audio Playback

    private func play(path: String, volume: Double, loop: Bool, result: @escaping FlutterResult) {
        // Stop any currently playing audio
        stop { _ in }

        let url: URL
        if path.starts(with: "http://") || path.starts(with: "https://") {
            url = URL(string: path)!
        } else {
            url = URL(fileURLWithPath: path)
        }

        do {
            audioPlayer = try AVAudioPlayer(contentsOf: url)
            audioPlayer?.volume = Float(volume)
            audioPlayer?.numberOfLoops = loop ? -1 : 0

            currentDuration = Double(audioPlayer?.duration ?? 0.0)
            currentPosition = 0.0
            currentVolume = volume
            isLooping = loop
            isPaused = false

            audioPlayer?.delegate = self
            audioPlayer?.play()

            isPlaying = true
            startPositionTimer()

            result(nil)
        } catch {
            result(FlutterError(code: "PLAY_ERROR", message: error.localizedDescription, details: nil))
        }
    }

    private func pause(result: @escaping FlutterResult) {
        audioPlayer?.pause()
        isPaused = true
        playbackTimer?.invalidate()
        result(nil)
    }

    private func stop(result: @escaping FlutterResult) {
        playbackTimer?.invalidate()
        audioPlayer?.stop()
        audioPlayer = nil

        currentPosition = 0.0
        isPlaying = false
        isPaused = false

        result(nil)
    }

    private func resume(result: @escaping FlutterResult) {
        audioPlayer?.play()
        isPaused = false
        startPositionTimer()
        result(nil)
    }

    private func setVolume(volume: Double, result: @escaping FlutterResult) {
        audioPlayer?.volume = Float(volume)
        currentVolume = volume
        result(nil)
    }

    private func getDuration(path: String, result: @escaping FlutterResult) {
        let url: URL
        if path.starts(with: "http://") || path.starts(with: "https://") {
            // For network URLs, duration might not be available immediately
            result(0.0)
            return
        } else {
            url = URL(fileURLWithPath: path)
        }

        do {
            let player = try AVAudioPlayer(contentsOf: url)
            let duration = Double(player.duration)
            player.stop()
            result(duration)
        } catch {
            result(0.0)
        }
    }

    private func startPositionTimer() {
        playbackTimer?.invalidate()
        playbackTimer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            guard let self = self, self.isPlaying, !self.isPaused else { return }
            self.currentPosition = Double(self.audioPlayer?.currentTime ?? 0.0)
        }
    }

    // MARK: - Audio Recording

    private func startRecording(result: @escaping FlutterResult) {
        let session = AVAudioSession.sharedInstance()

        do {
            try session.setCategory(.record, mode: .default)
            try session.setActive(true)

            let url = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("recording.wav")

            let settings: [String: Any] = [
                AVFormatIDKey: Int(kAudioFormatLinearPCM),
                AVSampleRateKey: 44100.0,
                AVNumberOfChannelsKey: 1,
                AVLinearPCMBitDepthKey: 16,
                AVLinearPCMIsBigEndianKey: false,
                AVLinearPCMIsFloatKey: false,
                AVLinearPCMIsNonInterleaved: false
            ]

            audioRecorder = try AVAudioRecorder(url: url, settings: settings)
            audioRecorder?.delegate = self
            audioRecorder?.isMeteringEnabled = true
            audioRecorder?.record()

            // Start timer to send audio data
            startRecordingDataTimer()

            result(nil)
        } catch {
            result(FlutterError(code: "RECORD_ERROR", message: error.localizedDescription, details: nil))
        }
    }

    private func startRecordingDataTimer() {
        recordingTimer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            guard let self = self, let recorder = self.audioRecorder, recorder.isRecording else { return }

            // Read audio data from the recorder
            if let audioData = self.readRecordingData() {
                self.eventSink?(audioData)
            }
        }
    }

    private func readRecordingData() -> [Int]? {
        guard let recorder = audioRecorder else { return nil }

        // For simplicity, we'll read from the recorded file
        // In a production implementation, you might want to use AudioQueue directly
        // for real-time streaming
        let url = recorder.url
        guard let data = try? Data(contentsOf: url) else { return nil }

        // Skip the WAV header (44 bytes) and convert to PCM data
        let pcmDataOffset = 44
        guard data.count > pcmDataOffset else { return nil }

        let pcmBytes = data.subdata(in: pcmDataOffset..<data.count)
        return [Int](pcmBytes)
    }

    private func stopRecording(result: @escaping FlutterResult) {
        recordingTimer?.invalidate()
        audioRecorder?.stop()

        let session = AVAudioSession.sharedInstance()
        try? session.setActive(false)

        result(nil)
    }

    // MARK: - Audio Playback Stream

    private func startPlaybackStream(path: String, result: @escaping FlutterResult) {
        let url: URL
        if path.starts(with: "http://") || path.starts(with: "https://") {
            result(FlutterError(code: "NOT_SUPPORTED", message: "Network URLs not supported for playback stream", details: nil))
            return
        } else {
            url = URL(fileURLWithPath: path)
        }

        playbackStreamEngine = AVAudioEngine()
        let engine = playbackStreamEngine!

        engine.attach(playbackStreamPlayerNode)

        // Read audio file
        guard let audioFile = try? AVAudioFile(forReading: url) else {
            result(FlutterError(code: "FILE_ERROR", message: "Could not open audio file", details: nil))
            return
        }

        let mainMixer = engine.mainMixerNode
        engine.connect(playbackStreamPlayerNode, to: mainMixer, format: audioFile.processingFormat)

        // Start playing and sending data
        playbackStreamSink = eventSink

        do {
            try engine.start()
            playbackStreamPlayerNode.play()

            // Read and stream audio data
            let bufferSize = AVAudioFrameCount(4096)
            var buffer = AVAudioPCMBuffer(pcmFormat: audioFile.processingFormat, frameCapacity: bufferSize)

            while audioFile.framePosition < audioFile.length {
                try audioFile.read(into: buffer)
                if let pcmData = convertBufferToPCM(buffer: buffer) {
                    playbackStreamSink?(pcmData)
                }
            }

            playbackStreamPlayerNode.stop()
            engine.stop()
            playbackStreamSink = nil

            result(nil)
        } catch {
            result(FlutterError(code: "STREAM_ERROR", message: error.localizedDescription, details: nil))
        }
    }

    private func convertBufferToPCM(buffer: AVAudioPCMBuffer) -> [Int]? {
        guard let channelData = buffer.floatChannelData else { return nil }

        let frameLength = Int(buffer.frameLength)
        let pcmData = [Int](repeating: 0, count: frameLength * 2) // 16-bit = 2 bytes per sample

        if buffer.format.channelCount == 1 {
            // Mono
            for i in 0..<frameLength {
                let sample = channelData[0][i]
                let pcmSample = Int16(sample * 32767.0)
                pcmData[i * 2] = Int(pcmSample & 0xFF)
                pcmData[i * 2 + 1] = Int((pcmSample >> 8) & 0xFF)
            }
        } else {
            // Stereo - mix down to mono
            for i in 0..<frameLength {
                let sample = (channelData[0][i] + channelData[1][i]) / 2.0
                let pcmSample = Int16(sample * 32767.0)
                pcmData[i * 2] = Int(pcmSample & 0xFF)
                pcmData[i * 2 + 1] = Int((pcmSample >> 8) & 0xFF)
            }
        }

        return pcmData
    }

    // MARK: - FlutterStreamHandler

    public func onListen(withArguments arguments: Any?, eventSink: @escaping FlutterEventSink) -> FlutterError? {
        self.eventSink = eventSink
        return nil
    }

    public func onCancel(withArguments arguments: Any?) -> FlutterError? {
        eventSink = nil
        stopRecording { _ in }
        return nil
    }
}

// MARK: - AVAudioPlayerDelegate

extension FlutterF2fSoundPlugin: AVAudioPlayerDelegate {
    public func audioPlayerDidFinishPlaying(_ player: AVAudioPlayer, successfully flag: Bool) {
        isPlaying = false
        playbackTimer?.invalidate()
    }

    public func audioPlayerDecodeErrorDidOccur(_ player: AVAudioPlayer, error: Error?) {
        isPlaying = false
        playbackTimer?.invalidate()
    }
}

// MARK: - AVAudioRecorderDelegate

extension FlutterF2fSoundPlugin: AVAudioRecorderDelegate {
    public func audioRecorderDidFinishRecording(_ recorder: AVAudioRecorder, successfully flag: Bool) {
        recordingTimer?.invalidate()
    }

    public func audioRecorderEncodeErrorDidOccur(_ recorder: AVAudioRecorder, error: Error?) {
        recordingTimer?.invalidate()
    }
}
