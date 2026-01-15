package com.tecmore.flutter_f2f_sound

import android.content.Context
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaPlayer
import android.net.Uri
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.EventChannel.EventSink
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import java.io.FileInputStream
import java.io.IOException

/** FlutterF2fSoundPlugin */
class FlutterF2fSoundPlugin :
    FlutterPlugin,
    MethodCallHandler, 
    EventChannel.StreamHandler {
    // The MethodChannel that will the communication between Flutter and native Android
    //
    // This local reference serves to register the plugin with the Flutter Engine and unregister it
    // when the Flutter Engine is detached from the Activity
    private lateinit var channel: MethodChannel
    private lateinit var audioManager: AudioManager
    private lateinit var eventChannel: EventChannel
    
    // Audio recording variables
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var recordingThread: Thread? = null
    private var eventSink: EventSink? = null

    override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, "flutter_f2f_sound")
        channel.setMethodCallHandler(this)
        audioManager = AudioManager(flutterPluginBinding.applicationContext)
        
        // Initialize event channel for audio streams
        eventChannel = EventChannel(flutterPluginBinding.binaryMessenger, "flutter_f2f_sound/recording_stream")
        eventChannel.setStreamHandler(this)
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
            "startRecording" -> {
                startRecording(result)
            }
            "stopRecording" -> {
                stopRecording(result)
            }
            "startPlaybackStream" -> {
                val path = call.argument<String>("path")
                if (path != null) {
                    startPlaybackStream(path, result)
                } else {
                    result.error("INVALID_PATH", "Audio path is required", null)
                }
            }
            else -> {
                result.notImplemented()
            }
        }
    }

    // EventChannel.StreamHandler implementation
    override fun onListen(arguments: Any?, events: EventSink?) {
        eventSink = events
    }

    override fun onCancel(arguments: Any?) {
        eventSink = null
        stopRecording(null)
    }

    // Audio recording implementation
    private fun startRecording(result: Result?) {
        val sampleRate = 44100
        val channelConfig = AudioFormat.CHANNEL_IN_MONO
        val audioFormat = AudioFormat.ENCODING_PCM_16BIT
        val bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)

        try {
            audioRecord = AudioRecord(
                AudioManager.STREAM_MUSIC, 
                sampleRate, 
                channelConfig, 
                audioFormat, 
                bufferSize
            )

            if (audioRecord!!.state != AudioRecord.STATE_INITIALIZED) {
                result?.error("RECORD_INIT_ERROR", "Failed to initialize AudioRecord", null)
                return
            }

            audioRecord!!.startRecording()
            isRecording = true

            // Start recording thread
            recordingThread = Thread {
                val buffer = ByteArray(bufferSize)
                while (isRecording && audioRecord?.state == AudioRecord.STATE_INITIALIZED) {
                    val bytesRead = audioRecord?.read(buffer, 0, buffer.size) ?: 0
                    if (bytesRead > 0 && eventSink != null) {
                        // Convert to List<Int> for Flutter
                        val intList = buffer.take(bytesRead).toList()
                        eventSink?.success(intList)
                    }
                }
            }
            recordingThread?.start()
            result?.success(null)
        } catch (e: Exception) {
            result?.error("RECORD_START_ERROR", e.message, null)
        }
    }

    private fun stopRecording(result: Result?) {
        isRecording = false
        recordingThread?.join()
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        result?.success(null)
    }

    // Audio playback stream implementation
    private fun startPlaybackStream(path: String, result: Result?) {
        Thread {
            val sampleRate = 44100
            val channelConfig = AudioFormat.CHANNEL_OUT_MONO
            val audioFormat = AudioFormat.ENCODING_PCM_16BIT
            val bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat)

            val audioTrack = AudioTrack(
                AudioManager.STREAM_MUSIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize,
                AudioTrack.MODE_STREAM
            )

            val buffer = ByteArray(bufferSize)
            var fileInputStream: FileInputStream? = null

            try {
                val uri = if (path.startsWith("http")) {
                    // For network URLs, we need to download first (simplified example)
                    result?.error("NETWORK_NOT_SUPPORTED", "Network URLs not supported for playback stream", null)
                    return@Thread
                } else {
                    Uri.parse("file://$path")
                }

                fileInputStream = FileInputStream(uri.path)
                audioTrack.play()

                var bytesRead: Int
                while (fileInputStream.read(buffer).also { bytesRead = it } != -1 && eventSink != null) {
                    audioTrack.write(buffer, 0, bytesRead)
                    // Send audio data to Flutter
                    val intList = buffer.take(bytesRead).toList()
                    eventSink?.success(intList)
                }

                audioTrack.stop()
                result?.success(null)
            } catch (e: IOException) {
                result?.error("PLAYBACK_STREAM_ERROR", e.message, null)
            } finally {
                fileInputStream?.close()
                audioTrack.release()
            }
        }.start()
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        eventChannel.setStreamHandler(null)
        audioManager.stop()
        stopRecording(null)
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
