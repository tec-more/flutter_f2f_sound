import Flutter
import UIKit
import AVFoundation

// AudioManager handles all audio playback operations
class AudioManager {
    private var audioPlayer: AVAudioPlayer?
    
    // Play audio from the given path
    func play(path: String, volume: Double, loop: Bool) {
        stop() // Stop any currently playing audio
        
        do {
            // Handle different path types
            let url: URL
            if path.starts(with: "http") || path.starts(with: "https") {
                // Network URL
                url = URL(string: path)! 
            } else if path.starts(with: "file://") {
                // File URL
                url = URL(string: path)! 
            } else {
                // Local file path
                url = URL(fileURLWithPath: path)
            }
            
            audioPlayer = try AVAudioPlayer(contentsOf: url)
            audioPlayer?.volume = Float(volume)
            audioPlayer?.numberOfLoops = loop ? -1 : 0
            audioPlayer?.prepareToPlay()
            audioPlayer?.play()
        } catch let error {
            print("Error playing audio: \(error.localizedDescription)")
        }
    }
    
    // Pause the currently playing audio
    func pause() {
        if audioPlayer?.isPlaying ?? false {
            audioPlayer?.pause()
        }
    }
    
    // Stop the currently playing audio
    func stop() {
        audioPlayer?.stop()
        audioPlayer = nil
    }
    
    // Resume playback of paused audio
    func resume() {
        if audioPlayer != nil && !(audioPlayer?.isPlaying ?? true) {
            audioPlayer?.play()
        }
    }
    
    // Set the volume of the currently playing audio (0.0 to 1.0)
    func setVolume(volume: Double) {
        audioPlayer?.volume = Float(volume)
    }
    
    // Check if audio is currently playing
    func isPlaying() -> Bool {
        return audioPlayer?.isPlaying ?? false
    }
    
    // Get the current playback position in seconds
    func getCurrentPosition() -> Double {
        return audioPlayer?.currentTime ?? 0.0
    }
    
    // Get the duration of the audio file in seconds
    func getDuration(path: String) -> Double {
        do {
            // Handle different path types
            let url: URL
            if path.starts(with: "http") || path.starts(with: "https") {
                // Network URL
                url = URL(string: path)! 
            } else if path.starts(with: "file://") {
                // File URL
                url = URL(string: path)! 
            } else {
                // Local file path
                url = URL(fileURLWithPath: path)
            }
            
            let tempPlayer = try AVAudioPlayer(contentsOf: url)
            return tempPlayer.duration
        } catch let error {
            print("Error getting duration: \(error.localizedDescription)")
            return 0.0
        }
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
