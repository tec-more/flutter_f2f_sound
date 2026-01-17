package com.tecmore.flutter_f2f_sound

import android.content.Context
import android.media.AudioFormat
import android.media.AudioManager as AndroidAudioManager
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaPlayer
import android.media.MediaRecorder
import android.net.Uri
import android.os.Handler
import android.os.Looper
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.EventChannel.EventSink
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

/** FlutterF2fSoundPlugin */
class FlutterF2fSoundPlugin :
    FlutterPlugin,
    MethodCallHandler {
    // The MethodChannel that will the communication between Flutter and native Android
    //
    // This local reference serves to register the plugin with the Flutter Engine and unregister it
    // when the Flutter Engine is detached from the Activity
    private lateinit var channel: MethodChannel
    private lateinit var audioManager: F2fAudioManager
    private lateinit var eventChannel: EventChannel
    private lateinit var playbackEventChannel: EventChannel
    private lateinit var systemSoundEventChannel: EventChannel
    
    // Audio recording variables
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var recordingThread: Thread? = null
    private var eventSink: EventSink? = null
    private var playbackEventSink: EventSink? = null
    private var systemSoundEventSink: EventSink? = null
    
    // Main thread handler for communication with Flutter
    private val mainThreadHandler = Handler(Looper.getMainLooper())

    override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, "com.tecmore.flutter_f2f_sound")
        channel.setMethodCallHandler(this)
        audioManager = F2fAudioManager(flutterPluginBinding.applicationContext)
        
        // Initialize event channel for audio recording streams
        eventChannel = EventChannel(flutterPluginBinding.binaryMessenger, "com.tecmore.flutter_f2f_sound/recording_stream")
        eventChannel.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventSink?) {
                eventSink = events
            }
            override fun onCancel(arguments: Any?) {
                eventSink = null
                stopRecording(null)
            }
        })
        
        // Initialize event channel for audio playback streams
        playbackEventChannel = EventChannel(flutterPluginBinding.binaryMessenger, "com.tecmore.flutter_f2f_sound/playback_stream")
        playbackEventChannel.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventSink?) {
                playbackEventSink = events
            }
            override fun onCancel(arguments: Any?) {
                playbackEventSink = null
            }
        })
        
        // Initialize event channel for system sound capture streams
        systemSoundEventChannel = EventChannel(flutterPluginBinding.binaryMessenger, "com.tecmore.flutter_f2f_sound/system_sound_stream")
        systemSoundEventChannel.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventSink?) {
                systemSoundEventSink = events
            }
            override fun onCancel(arguments: Any?) {
                systemSoundEventSink = null
            }
        })
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

    // Audio recording implementation
    private fun startRecording(result: Result?) {
        val sampleRate = 44100
        val channelConfig = AudioFormat.CHANNEL_IN_MONO
        val audioFormat = AudioFormat.ENCODING_PCM_16BIT
        val bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC, 
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
                        val intList = ArrayList<Int>(bytesRead)
                        for (i in 0 until bytesRead) {
                            intList.add(buffer[i].toInt())
                        }
                        // Send audio data to Flutter on main thread
                        mainThreadHandler.post {
                            eventSink?.success(intList)
                        }
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
                AndroidAudioManager.STREAM_MUSIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize,
                AudioTrack.MODE_STREAM
            )

            val buffer = ByteArray(bufferSize)
            var inputStream: InputStream? = null

            try {
                if (path.startsWith("http")) {
                    // For network URLs, use HttpURLConnection
                    val url = URL(path)
                    val connection = url.openConnection() as HttpURLConnection
                    connection.requestMethod = "GET"
                    connection.connectTimeout = 5000
                    connection.readTimeout = 5000
                    connection.connect()
                    
                    if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                        result?.error("NETWORK_ERROR", "Failed to connect to URL: ${connection.responseMessage}", null)
                        connection.disconnect()
                        return@Thread
                    }
                    
                    inputStream = connection.inputStream
                } else {
                    // For local files
                    val uri = Uri.parse("file://$path")
                    inputStream = FileInputStream(uri.path)
                }

                audioTrack.play()

                var bytesRead: Int
                while (inputStream.read(buffer).also { bytesRead = it } != -1 && playbackEventSink != null) {
                audioTrack.write(buffer, 0, bytesRead)
                // Send audio data to Flutter on main thread
                val intList = ArrayList<Int>(bytesRead)
                for (i in 0 until bytesRead) {
                    intList.add(buffer[i].toInt())
                }
                mainThreadHandler.post {
                    playbackEventSink?.success(intList)
                }
                }

                audioTrack.stop()
                result?.success(null)
            } catch (e: IOException) {
                result?.error("PLAYBACK_STREAM_ERROR", e.message, null)
            } finally {
                inputStream?.close()
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

/** F2fAudioManager handles all audio playback operations */
class F2fAudioManager(private val context: Context) {
    private var mediaPlayer: MediaPlayer? = null

    /** Play audio from the given path */
    fun play(path: String, volume: Double, loop: Boolean, onPrepared: (() -> Unit)? = null) {
        stop() // Stop any currently playing audio

        try {
            mediaPlayer = MediaPlayer().apply {
                if (path.startsWith("http")) {
                    // For network URLs, use setDataSource(String) directly
                    setDataSource(path)
                } else {
                    // For local files, use Uri with context
                    val uri = Uri.parse("file://$path")
                    setDataSource(context, uri)
                }
                setVolume(volume.toFloat(), volume.toFloat())
                isLooping = loop

                // Use prepareAsync() for network streams to avoid blocking UI
                // Use setOnPreparedListener to start playback when ready
                setOnPreparedListener { mp ->
                    mp.start()
                    onPrepared?.invoke()
                }

                prepareAsync()
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
        mediaPlayer?.setVolume(volume.toFloat(), volume.toFloat())
    }

    /** Check if audio is currently playing */
    fun isPlaying(): Boolean {
        return mediaPlayer?.isPlaying ?: false
    }

    /** Get the current playback position in seconds */
    fun getCurrentPosition(): Double {
        return mediaPlayer?.currentPosition?.toDouble()?.div(1000) ?: 0.0
    }

    /** Get the duration of the currently playing audio */
    fun getPlayingDuration(): Double {
        return mediaPlayer?.duration?.toDouble()?.div(1000) ?: 0.0
    }

    /** Get the duration of the audio file in seconds */
    fun getDuration(path: String): Double {
        return try {
            val tempPlayer = MediaPlayer()
            if (path.startsWith("http")) {
                // For network URLs, use setDataSource(String) directly
                tempPlayer.setDataSource(path)
            } else {
                // For local files, use Uri with context
                val uri = Uri.parse("file://$path")
                tempPlayer.setDataSource(context, uri)
            }

            // For network files, duration may not be immediately available
            // Return 0.0 for network URLs to avoid blocking
            val duration = if (path.startsWith("http")) {
                tempPlayer.release()
                0.0  // Duration will be updated when MediaPlayer is prepared
            } else {
                tempPlayer.prepare()

                // Log audio properties for debugging
                val dur = tempPlayer.duration.toDouble() / 1000
                println("Audio file loaded - Duration: $dur seconds")
                println("Note: MediaPlayer automatically handles sample rate conversion")
                println("Device sample rate will be used for playback")

                tempPlayer.release()
                dur
            }
            duration
        } catch (e: Exception) {
            e.printStackTrace()
            0.0
        }
    }

    /** Get device audio properties */
    fun getAudioProperties(): Map<String, Any> {
        val audioManager = context.getSystemService(android.content.Context.AUDIO_SERVICE) as AndroidAudioManager
        val sampleRate = audioManager.getProperty(android.media.AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE) ?: "48000"
        val framesPerBuffer = audioManager.getProperty(android.media.AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER) ?: "256"

        return mapOf(
            "sampleRate" to sampleRate.toInt(),
            "framesPerBuffer" to framesPerBuffer.toInt(),
            "supportsSampleRateConversion" to true,
            "note" to "MediaPlayer automatically converts audio to device sample rate"
        )
    }
}