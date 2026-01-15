package com.tecmore.flutter_f2f_sound

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result

/** FlutterF2fSoundPlugin */
class FlutterF2fSoundPlugin :
    FlutterPlugin,
    MethodCallHandler {
    // The MethodChannel that will the communication between Flutter and native Android
    //
    // This local reference serves to register the plugin with the Flutter Engine and unregister it
    // when the Flutter Engine is detached from the Activity
    private lateinit var channel: MethodChannel
    private lateinit var audioManager: AudioManager

    override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, "flutter_f2f_sound")
        channel.setMethodCallHandler(this)
        audioManager = AudioManager(flutterPluginBinding.applicationContext)
    }

    override fun onMethodCall(
        call: MethodCall,
        result: Result
    ) {
        when (call.method) {
            "getPlatformVersion" -> {
                result.success("Android ${android.os.Build.VERSION.RELEASE}")
            }
            "play" -> {
                val path = call.argument<String>("path")
                val volume = call.argument<Double>("volume") ?: 1.0
                val loop = call.argument<Boolean>("loop") ?: false
                
                if (path != null) {
                    audioManager.play(path, volume, loop)
                    result.success(null)
                } else {
                    result.error("INVALID_PATH", "Audio path is required", null)
                }
            }
            "pause" -> {
                audioManager.pause()
                result.success(null)
            }
            "stop" -> {
                audioManager.stop()
                result.success(null)
            }
            "resume" -> {
                audioManager.resume()
                result.success(null)
            }
            "setVolume" -> {
                val volume = call.argument<Double>("volume") ?: 1.0
                audioManager.setVolume(volume)
                result.success(null)
            }
            "isPlaying" -> {
                result.success(audioManager.isPlaying())
            }
            "getCurrentPosition" -> {
                result.success(audioManager.getCurrentPosition())
            }
            "getDuration" -> {
                val path = call.argument<String>("path")
                if (path != null) {
                    result.success(audioManager.getDuration(path))
                } else {
                    result.error("INVALID_PATH", "Audio path is required", null)
                }
            }
            else -> {
                result.notImplemented()
            }
        }
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        audioManager.stop()
    }
}

/** AudioManager handles all audio playback operations */
class AudioManager(private val context: Context) {
    private var mediaPlayer: MediaPlayer? = null

    /** Play audio from the given path */
    fun play(path: String, volume: Double, loop: Boolean) {
        stop() // Stop any currently playing audio
        
        try {
            val uri = if (path.startsWith("http")) {
                Uri.parse(path)
            } else {
                Uri.parse("file://$path")
            }
            
            mediaPlayer = MediaPlayer().apply {
                setDataSource(context, uri)
                this.volume = volume.toFloat()
                isLooping = loop
                prepare()
                start()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    /** Pause the currently playing audio */
    fun pause() {
        mediaPlayer?.takeIf { it.isPlaying }?.pause()
    }

    /** Stop the currently playing audio */
    fun stop() {
        mediaPlayer?.apply {
            stop()
            release()
        }
        mediaPlayer = null
    }

    /** Resume playback of paused audio */
    fun resume() {
        mediaPlayer?.takeIf { !it.isPlaying }?.start()
    }

    /** Set the volume of the currently playing audio (0.0 to 1.0) */
    fun setVolume(volume: Double) {
        mediaPlayer?.volume = volume.toFloat()
    }

    /** Check if audio is currently playing */
    fun isPlaying(): Boolean {
        return mediaPlayer?.isPlaying ?: false
    }

    /** Get the current playback position in seconds */
    fun getCurrentPosition(): Double {
        return mediaPlayer?.currentPosition?.toDouble()?.div(1000) ?: 0.0
    }

    /** Get the duration of the audio file in seconds */
    fun getDuration(path: String): Double {
        return try {
            val uri = if (path.startsWith("http")) {
                Uri.parse(path)
            } else {
                Uri.parse("file://$path")
            }
            
            MediaPlayer().use {tempPlayer ->
                tempPlayer.setDataSource(context, uri)
                tempPlayer.prepare()
                tempPlayer.duration.toDouble() / 1000
            }
        } catch (e: Exception) {
            0.0
        }
    }
}
