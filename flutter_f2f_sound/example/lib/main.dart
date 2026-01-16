import 'package:flutter/material.dart';
import 'dart:async';

import 'package:flutter/services.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _platformVersion = 'Unknown';
  final _flutterF2fSoundPlugin = FlutterF2fSound();

  // Audio test variables
  bool _isPlaying = false;
  double _volume = 1.0;
  double _currentPosition = 0.0;
  double _duration = 0.0;
  String _audioPath = '';
  Timer? _positionTimer;

  // Recording variables
  bool _isRecording = false;
  double _recordingDuration = 0.0;
  Timer? _recordingTimer;
  StreamSubscription<List<int>>? _recordingStreamSubscription;
  int _audioDataLength = 0;

  @override
  void initState() {
    super.initState();
    initPlatformState();
    // Set a default audio path for testing (replace with your own audio file path)
    _audioPath =
        'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3';
  }

  @override
  void dispose() {
    _positionTimer?.cancel();
    _recordingTimer?.cancel();
    _recordingStreamSubscription?.cancel();
    super.dispose();
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    String platformVersion;
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    try {
      platformVersion =
          await _flutterF2fSoundPlugin.getPlatformVersion() ??
          'Unknown platform version';
    } on PlatformException {
      platformVersion = 'Failed to get platform version.';
    }

    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
    if (!mounted) return;

    setState(() {
      _platformVersion = platformVersion;
    });
  }

  // Start a timer to update the current position
  void _startPositionTimer() {
    _positionTimer?.cancel();
    _positionTimer = Timer.periodic(const Duration(milliseconds: 500), (
      timer,
    ) async {
      if (_isPlaying) {
        final position = await _flutterF2fSoundPlugin.getCurrentPosition();
        setState(() {
          _currentPosition = position;
        });
      } else {
        timer.cancel();
      }
    });
  }

  // Format time in seconds to MM:SS format
  String _formatTime(double seconds) {
    final minutes = (seconds ~/ 60).toString().padLeft(2, '0');
    final secs = (seconds % 60).toString().padLeft(2, '0');
    return '$minutes:$secs';
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('F2F Sound Plugin Example')),
        body: Padding(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Platform version info
              Text('Running on: $_platformVersion\n'),

              // Audio path input
              TextField(
                decoration: const InputDecoration(
                  labelText: 'Audio Path',
                  hintText: 'Enter audio file path or URL',
                  border: OutlineInputBorder(),
                ),
                controller: TextEditingController(text: _audioPath),
                onChanged: (value) {
                  _audioPath = value;
                },
              ),
              const SizedBox(height: 16),

              // Playback controls
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () async {
                        if (_audioPath.isEmpty) return;

                        await _flutterF2fSoundPlugin.play(
                          path: _audioPath,
                          volume: _volume,
                          loop: false,
                        );

                        // Get duration
                        final duration = await _flutterF2fSoundPlugin
                            .getDuration(_audioPath);

                        setState(() {
                          _isPlaying = true;
                          _duration = duration;
                          _currentPosition = 0.0;
                        });

                        _startPositionTimer();
                      },
                      icon: const Icon(Icons.play_arrow),
                      label: const Text('Play'),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () async {
                        await _flutterF2fSoundPlugin.pause();
                        setState(() {
                          _isPlaying = false;
                        });
                      },
                      icon: const Icon(Icons.pause),
                      label: const Text('Pause'),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () async {
                        await _flutterF2fSoundPlugin.stop();
                        setState(() {
                          _isPlaying = false;
                          _currentPosition = 0.0;
                        });
                      },
                      icon: const Icon(Icons.stop),
                      label: const Text('Stop'),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () async {
                        await _flutterF2fSoundPlugin.resume();
                        setState(() {
                          _isPlaying = true;
                        });
                        _startPositionTimer();
                      },
                      icon: const Icon(Icons.play_arrow),
                      label: const Text('Resume'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 16),

              // Volume control
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text('Volume:'),
                  Row(
                    children: [
                      Expanded(
                        child: Slider(
                          value: _volume,
                          min: 0.0,
                          max: 1.0,
                          divisions: 100,
                          onChanged: (value) async {
                            await _flutterF2fSoundPlugin.setVolume(value);
                            setState(() {
                              _volume = value;
                            });
                          },
                        ),
                      ),
                      SizedBox(
                        width: 50,
                        child: Text('${(_volume * 100).toInt()}%'),
                      ),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 16),

              // Playback position
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(_formatTime(_currentPosition)),
                      Text(_formatTime(_duration)),
                    ],
                  ),
                  Slider(
                    value: _currentPosition,
                    min: 0.0,
                    max: _duration > 0 ? _duration : 1.0,
                    onChanged: (value) {
                      // Note: Position seeking is not implemented in this example
                    },
                    activeColor: _isPlaying ? Colors.blue : Colors.grey,
                    inactiveColor: Colors.grey[300],
                  ),
                ],
              ),
              const SizedBox(height: 16),

              // Status info
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(12.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Playback Status: ${_isPlaying ? 'Playing' : 'Stopped/Paused'}',
                      ),
                      Text('Volume: ${(_volume * 100).toInt()}%'),
                      Text(
                        'Current Position: ${_formatTime(_currentPosition)}',
                      ),
                      Text('Duration: ${_formatTime(_duration)}'),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // Recording section
              const Text(
                'Recording Controls:',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 16),

              // Recording buttons
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  ElevatedButton.icon(
                    onPressed: () async {
                      if (_isRecording) return;

                      // Request microphone permission
                      final status = await Permission.microphone.request();
                      if (status != PermissionStatus.granted) {
                        print('Microphone permission denied');
                        // Show permission denied dialog
                        showDialog(
                          context: context,
                          builder: (context) => AlertDialog(
                            title: const Text('Permission Denied'),
                            content: const Text(
                              'Microphone permission is required to record audio.',
                            ),
                            actions: [
                              TextButton(
                                onPressed: () {
                                  Navigator.pop(context);
                                },
                                child: const Text('OK'),
                              ),
                            ],
                          ),
                        );
                        return;
                      }

                      setState(() {
                        _isRecording = true;
                        _recordingDuration = 0.0;
                        _audioDataLength = 0;
                      });

                      // Start recording timer
                      _recordingTimer?.cancel();
                      _recordingTimer = Timer.periodic(
                        const Duration(milliseconds: 500),
                        (timer) {
                          if (_isRecording) {
                            setState(() {
                              _recordingDuration += 0.5;
                            });
                          } else {
                            timer.cancel();
                          }
                        },
                      );

                      try {
                        // Get recording stream
                        final recordingStream = _flutterF2fSoundPlugin
                            .startRecording();

                        // Listen to recording stream
                        _recordingStreamSubscription = recordingStream.listen(
                          (audioData) {
                            setState(() {
                              _audioDataLength += audioData.length;
                            });
                          },
                          onError: (error) {
                            print('Recording error: $error');
                            setState(() {
                              _isRecording = false;
                            });
                          },
                        );
                      } catch (e) {
                        print('Failed to start recording: $e');
                        setState(() {
                          _isRecording = false;
                        });
                      }
                    },
                    icon: const Icon(Icons.mic),
                    label: const Text('Record'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                    ),
                  ),
                  ElevatedButton.icon(
                    onPressed: () async {
                      if (!_isRecording) return;

                      setState(() {
                        _isRecording = false;
                      });

                      // Stop recording
                      await _flutterF2fSoundPlugin.stopRecording();
                      _recordingStreamSubscription?.cancel();
                      _recordingTimer?.cancel();
                    },
                    icon: const Icon(Icons.stop),
                    label: const Text('Stop'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 16),

              // Recording status
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(12.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Recording Status: ${_isRecording ? 'Recording' : 'Stopped'}',
                      ),
                      Text(
                        'Recording Duration: ${_formatTime(_recordingDuration)}',
                      ),
                      Text(
                        'Audio Data Captured: ${(_audioDataLength / 1024).toStringAsFixed(2)} KB',
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
