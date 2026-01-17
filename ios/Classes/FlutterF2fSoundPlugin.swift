import Flutter
import UIKit
import AVFoundation
import AudioToolbox

// AudioManager handles all audio playback operations
class AudioManager {
    private var audioPlayer: AVAudioPlayer?
    private var audioPlayerItem: AVPlayerItem?
    private var audioPlayerAV: AVPlayer?
    private var useAVPlayer = false  // Flag to track if we're using AVPlayer for network streams

    // Play audio from the given path
    func play(path: String, volume: Double, loop: Bool) {
        stop() // Stop any currently playing audio

        // Handle different path types
        let url: URL
        if path.starts(with: "http") || path.starts(with: "https") {
            // Network URL - use AVPlayer for better async support
            url = URL(string: path)!
            useAVPlayer = true

            // Create AVPlayer for network streams (non-blocking)
            let playerItem = AVPlayerItem(url: url)
            audioPlayerAV = AVPlayer(playerItem: playerItem)
            audioPlayerItem = playerItem

            // Setup volume
            audioPlayerAV?.volume = Float(volume)

            // Setup looping
            if loop {
                NotificationCenter.default.addObserver(
                    self,
                    selector: #selector(playerItemDidReachEnd),
                    name: .AVPlayerItemDidPlayToEndTime,
                    object: playerItem
                )
            }

            // Start playing (non-blocking for network streams)
            audioPlayerAV?.play()
        } else {
            // Local file - use AVAudioPlayer
            useAVPlayer = false

            if path.starts(with: "file://") {
                url = URL(string: path)!
            } else {
                url = URL(fileURLWithPath: path)
            }

            do {
                audioPlayer = try AVAudioPlayer(contentsOf: url)
                audioPlayer?.volume = Float(volume)
                audioPlayer?.numberOfLoops = loop ? -1 : 0
                audioPlayer?.prepareToPlay()  // Fast for local files
                audioPlayer?.play()
            } catch let error {
                print("Error playing audio: \(error.localizedDescription)")
            }
        }
    }

    @objc func playerItemDidReachEnd(notification: NSNotification) {
        if let playerItem = notification.object as? AVPlayerItem {
            playerItem.seek(to: .zero)
            audioPlayerAV?.play()
        }
    }

    // Pause the currently playing audio
    func pause() {
        if useAVPlayer {
            audioPlayerAV?.pause()
        } else {
            if audioPlayer?.isPlaying ?? false {
                audioPlayer?.pause()
            }
        }
    }

    // Stop the currently playing audio
    func stop() {
        if useAVPlayer {
            audioPlayerAV?.pause()
            audioPlayerAV?.seek(to: .zero)
            NotificationCenter.default.removeObserver(self)
            audioPlayerAV = nil
            audioPlayerItem = nil
        } else {
            audioPlayer?.stop()
            audioPlayer = nil
        }
    }

    // Resume playback of paused audio
    func resume() {
        if useAVPlayer {
            audioPlayerAV?.play()
        } else {
            if audioPlayer != nil && !(audioPlayer?.isPlaying ?? true) {
                audioPlayer?.play()
            }
        }
    }

    // Set the volume of the currently playing audio (0.0 to 1.0)
    func setVolume(volume: Double) {
        if useAVPlayer {
            audioPlayerAV?.volume = Float(volume)
        } else {
            audioPlayer?.volume = Float(volume)
        }
    }

    // Check if audio is currently playing
    func isPlaying() -> Bool {
        if useAVPlayer {
            // Check if AVPlayer is playing
            guard let player = audioPlayerAV else { return false }
            return player.rate != 0
        } else {
            return audioPlayer?.isPlaying ?? false
        }
    }

    // Get the current playback position in seconds
    func getCurrentPosition() -> Double {
        if useAVPlayer {
            return audioPlayerAV?.currentTime().seconds ?? 0.0
        } else {
            return audioPlayer?.currentTime ?? 0.0
        }
    }

    // Get the duration of the audio file in seconds
    func getDuration(path: String) -> Double {
        // For network URLs, return 0.0 to avoid blocking
        if path.starts(with: "http") || path.starts(with: "https") {
            return 0.0  // Duration will be available when player is ready
        }

        // For local files, load synchronously (fast)
        do {
            let url: URL
            if path.starts(with: "file://") {
                url = URL(string: path)!
            } else {
                url = URL(fileURLWithPath: path)
            }

            let tempPlayer = try AVAudioPlayer(contentsOf: url)
            return tempPlayer.duration
        } catch let error {
            print("Error getting duration: \(error.localizedDescription)")
            return 0.0
        }
    }

    // Get the duration of currently playing audio
    func getPlayingDuration() -> Double {
        if useAVPlayer {
            // Get duration from AVPlayer
            if let playerItem = audioPlayerItem {
                return playerItem.duration.seconds
            }
            return 0.0
        } else {
            return audioPlayer?.duration ?? 0.0
        }
    }

    // Get device audio properties
    func getAudioProperties() -> [String: Any] {
        var sampleRate: Double = 44100.0
        var supportsSampleRateConversion = false

        // Get current hardware sample rate
        if let session = AVAudioSession.sharedInstance() as? AVAudioSession {
            sampleRate = session.sampleRate
            supportsSampleRateConversion = true
        }

        return [
            "sampleRate": sampleRate,
            "supportsSampleRateConversion": supportsSampleRateConversion,
            "note": "AVPlayer and AVAudioPlayer automatically handle sample rate conversion"
        ]
    }
}

public class FlutterF2fSoundPlugin: NSObject, FlutterPlugin, FlutterStreamHandler {
    private let audioManager = AudioManager()

    // Audio recording
    private var audioQueue: AudioQueueRef?
    private var recordingBuffers: [AudioQueueBufferRef] = []
    private var isRecording = false
    private var eventSink: FlutterEventSink?
    private var recordingFormat: AudioStreamBasicDescription?

    // Recording thread
    private var recordingThread: Thread?
    
    // Playback stream properties
    private var playbackStreamEngine: AVAudioEngine?
    private var playbackStreamPlayerNode: AVAudioPlayerNode?
    private var playbackStreamSink: FlutterEventSink?
    private var playbackStreamUrl: URL?
    private var isPlaybackStreaming = false

    public static func register(with registrar: FlutterPluginRegistrar) {
        let channel = FlutterMethodChannel(name: "com.tecmore.flutter_f2f_sound", binaryMessenger: registrar.messenger())
        let recordingEventChannel = FlutterEventChannel(name: "com.tecmore.flutter_f2f_sound/recording_stream", binaryMessenger: registrar.messenger())
        let playbackEventChannel = FlutterEventChannel(name: "com.tecmore.flutter_f2f_sound/playback_stream", binaryMessenger: registrar.messenger())
        let instance = FlutterF2fSoundPlugin()

        registrar.addMethodCallDelegate(instance, channel: channel)
        recordingEventChannel.setStreamHandler(instance)
        playbackEventChannel.setStreamHandler(instance)
    }

    public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
        switch call.method {
        case "getPlatformVersion":
            result("iOS " + UIDevice.current.systemVersion)
        case "play":
            guard let arguments = call.arguments as? [String: Any],
                  let path = arguments["path"] as? String else {
                result(FlutterError(code: "INVALID_PATH", message: "Audio path is required", details: nil))
                return
            }

            let volume = arguments["volume"] as? Double ?? 1.0
            let loop = arguments["loop"] as? Bool ?? false

            audioManager.play(path: path, volume: volume, loop: loop)
            result(nil)
        case "pause":
            audioManager.pause()
            result(nil)
        case "stop":
            audioManager.stop()
            result(nil)
        case "resume":
            audioManager.resume()
            result(nil)
        case "setVolume":
            guard let arguments = call.arguments as? [String: Any],
                  let volume = arguments["volume"] as? Double else {
                result(nil)
                return
            }

            audioManager.setVolume(volume: volume)
            result(nil)
        case "isPlaying":
            result(audioManager.isPlaying())
        case "getCurrentPosition":
            result(audioManager.getCurrentPosition())
        case "getDuration":
            guard let arguments = call.arguments as? [String: Any],
                  let path = arguments["path"] as? String else {
                result(FlutterError(code: "INVALID_PATH", message: "Audio path is required", details: nil))
                return
            }

            result(audioManager.getDuration(path: path))
        case "startRecording":
            startRecording(result: result)
        case "stopRecording":
            stopRecording(result: result)
        case "startPlaybackStream":
            guard let arguments = call.arguments as? [String: Any],
                  let path = arguments["path"] as? String else {
                result(FlutterError(code: "INVALID_PATH", message: "Audio path is required", details: nil))
                return
            }
            startPlaybackStream(path: path, result: result)
        default:
            result(FlutterMethodNotImplemented)
        }
    }

    // MARK: - Audio Recording

    private func startRecording(result: @escaping FlutterResult) {
        let session = AVAudioSession.sharedInstance()

        do {
            try session.setCategory(.record, mode: .default)
            try session.setActive(true)

            // Setup audio format for recording
            var streamDescription = AudioStreamBasicDescription(
                mSampleRate: 44100.0,
                mFormatID: kAudioFormatLinearPCM,
                mFormatFlags: kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked,
                mBytesPerPacket: 2,
                mFramesPerPacket: 1,
                mBytesPerFrame: 2,
                mChannelsPerFrame: 1,
                mBitsPerChannel: 16,
                mReserved: 0
            )

            recordingFormat = streamDescription

            // Create audio queue
            var audioQueue: AudioQueueRef?
            let status = AudioQueueNewInput(
                &streamDescription,
                audioQueueInputCallback,
                Unmanaged.passUnretained(self).toOpaque(),
                .none,
                .none,
                0,
                &audioQueue
            )

            if status != noErr {
                result(FlutterError(code: "RECORD_ERROR", message: "Failed to create audio queue", details: nil))
                return
            }

            self.audioQueue = audioQueue

            // Allocate buffers
            let bufferSize: UInt32 = 4096
            let bufferCount = 3

            for _ in 0..<bufferCount {
                var buffer: AudioQueueBufferRef?
                let bufferStatus = AudioQueueAllocateBuffer(audioQueue!, bufferSize, &buffer)

                if bufferStatus == noErr, let buffer = buffer {
                    recordingBuffers.append(buffer)
                    AudioQueueEnqueueBuffer(audioQueue!, buffer, 0, nil)
                }
            }

            // Start recording
            let startStatus = AudioQueueStart(audioQueue!, nil)
            if startStatus != noErr {
                result(FlutterError(code: "RECORD_ERROR", message: "Failed to start audio queue", details: nil))
                return
            }

            isRecording = true
            result(nil)

        } catch {
            result(FlutterError(code: "RECORD_ERROR", message: error.localizedDescription, details: nil))
        }
    }

    private func stopRecording(result: @escaping FlutterResult) {
        if let queue = audioQueue {
            AudioQueueStop(queue, true)
            AudioQueueDispose(queue, true)
            audioQueue = nil
        }

        recordingBuffers.removeAll()
        isRecording = false

        let session = AVAudioSession.sharedInstance()
        try? session.setActive(false)

        result(nil)
    }
    
    // MARK: - Audio Playback Stream
    
    private func startPlaybackStream(path: String, result: @escaping FlutterResult) {
        // Stop any existing playback stream
        if isPlaybackStreaming {
            stopPlaybackStream()
        }
        
        let url: URL
        if path.starts(with: "http://") || path.starts(with: "https://") {
            guard let networkUrl = URL(string: path) else {
                result(FlutterError(code: "INVALID_URL", message: "Invalid network URL", details: nil))
                return
            }
            url = networkUrl
        } else {
            url = URL(fileURLWithPath: path) // Local file
        }
        
        playbackStreamUrl = url
        
        // Initialize playback engine and player node
        playbackStreamEngine = AVAudioEngine()
        playbackStreamPlayerNode = AVAudioPlayerNode()
        
        guard let engine = playbackStreamEngine, let playerNode = playbackStreamPlayerNode else {
            result(FlutterError(code: "INIT_ERROR", message: "Failed to initialize playback stream", details: nil))
            return
        }
        
        engine.attach(playerNode)
        
        // Read audio file asynchronously
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let audioFile = try AVAudioFile(forReading: url)
                
                DispatchQueue.main.async {
                    let mainMixer = engine.mainMixerNode
                    engine.connect(playerNode, to: mainMixer, format: audioFile.processingFormat)
                    
                    // Start playing and sending data
                    self.playbackStreamSink = self.eventSink
                    self.isPlaybackStreaming = true
                    
                    do {
                        try engine.start()
                        playerNode.play()
                        
                        // Read and stream audio data in a separate thread
                        DispatchQueue.global(qos: .userInitiated).async {
                            do {
                                let bufferSize = AVAudioFrameCount(4096)
                                var buffer = AVAudioPCMBuffer(pcmFormat: audioFile.processingFormat, frameCapacity: bufferSize)
                                
                                while audioFile.framePosition < audioFile.length && self.isPlaybackStreaming {
                                    try audioFile.read(into: buffer)
                                    if let pcmData = self.convertBufferToPCM(buffer: buffer) {
                                        DispatchQueue.main.async {
                                            self.playbackStreamSink?(pcmData)
                                        }
                                    }
                                }
                                
                                DispatchQueue.main.async {
                                    self.stopPlaybackStream()
                                    result(nil)
                                }
                            } catch {
                                DispatchQueue.main.async {
                                    self.stopPlaybackStream()
                                    result(FlutterError(code: "STREAM_ERROR", message: error.localizedDescription, details: nil))
                                }
                            }
                        }
                    } catch {
                        self.stopPlaybackStream()
                        result(FlutterError(code: "ENGINE_ERROR", message: error.localizedDescription, details: nil))
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    self.stopPlaybackStream()
                    result(FlutterError(code: "FILE_ERROR", message: "Could not open audio file", details: nil))
                }
            }
        }
    }
    
    private func stopPlaybackStream() {
        isPlaybackStreaming = false
        
        // Stop playback engine and player node
        if let playerNode = playbackStreamPlayerNode {
            playerNode.stop()
            playbackStreamPlayerNode = nil
        }
        
        if let engine = playbackStreamEngine {
            engine.stop()
            engine.reset()
            playbackStreamEngine = nil
        }
        
        playbackStreamSink = nil
        playbackStreamUrl = nil
    }
    
    private func convertBufferToPCM(buffer: AVAudioPCMBuffer) -> [Int]? {
        guard let channelData = buffer.floatChannelData, let floatData = channelData.pointee else {
            return nil
        }
        
        let frameCount = Int(buffer.frameLength)
        let channelCount = Int(buffer.format.channelCount)
        var pcmData = [Int](repeating: 0, count: frameCount * channelCount)
        
        for frame in 0..<frameCount {
            for channel in 0..<channelCount {
                let value = floatData[frame * channelCount + channel]
                // Convert float (-1.0 to 1.0) to 16-bit PCM (-32768 to 32767)
                let intValue = Int32(value * 32767.0)
                pcmData[frame * channelCount + channel] = Int(intValue)
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
        stopPlaybackStream()
        return nil
    }
}

// MARK: - Audio Queue Callback

private func audioQueueInputCallback(
    _ inUserData: UnsafeMutableRawPointer?,
    inQueue: AudioQueueRef,
    inBuffer: AudioQueueBufferRef,
    inStartTime: UnsafePointer<AudioTimeStamp>,
    inNumberPacketDescriptions: UInt32,
    inPacketDescs: UnsafePointer<AudioStreamPacketDescription>?
) {
    guard let userData = inUserData else { return }

    let plugin = Unmanaged<FlutterF2fSoundPlugin>.fromOpaque(userData).takeUnretainedValue()

    if plugin.isRecording, let eventSink = plugin.eventSink {
        let audioData = Data(bytes: inBuffer.pointee.mAudioData, count: Int(inBuffer.pointee.mAudioDataByteSize))
        let intArray = [Int](audioData)

        DispatchQueue.main.async {
            eventSink(intArray)
        }
    }

    // Re-enqueue the buffer
    AudioQueueEnqueueBuffer(inQueue, inBuffer, 0, nil)
}
