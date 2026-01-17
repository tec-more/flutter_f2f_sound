import Flutter
import UIKit
import AVFoundation

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

public class FlutterF2fSoundPlugin: NSObject, FlutterPlugin {
    private let audioManager = AudioManager()
    
    public static func register(with registrar: FlutterPluginRegistrar) {
        let channel = FlutterMethodChannel(name: "com.tecmore.flutter_f2f_sound", binaryMessenger: registrar.messenger())
        let instance = FlutterF2fSoundPlugin()
        registrar.addMethodCallDelegate(instance, channel: channel)
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
        default:
            result(FlutterMethodNotImplemented)
        }
    }
}
