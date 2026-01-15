import 'package:flutter/material.dart';
import 'dart:async';

import 'package:flutter/services.dart';
import 'package:flutter_f2f_sound/flutter_f2f_sound.dart';

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

  @override
  void initState() {
    super.initState();
    initPlatformState();
    // Set a default audio path for testing (replace with your own audio file path)
    _audioPath = 'https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3';
  }

  @override
  void dispose() {
    _positionTimer?.cancel();
    super.dispose();
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    String platformVersion;
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    try {
      platformVersion = await _flutterF2fSoundPlugin.getPlatformVersion() ?? 'Unknown platform version';
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
    _positionTimer = Timer.periodic(const Duration(milliseconds: 500), (timer) async {
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
        appBar: AppBar(
          title: const Text('F2F Sound Plugin Example'),
        ),
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
                  ElevatedButton.icon(
                    onPressed: () async {
                      if (_audioPath.isEmpty) return;
                      
                      await _flutterF2fSoundPlugin.play(
                        path: _audioPath,
                        volume: _volume,
                        loop: false,
                      );
                      
                      // Get duration
                      final duration = await _flutterF2fSoundPlugin.getDuration(_audioPath);
                      
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
                  ElevatedButton.icon(
                    onPressed: () async {
                      await _flutterF2fSoundPlugin.pause();
                      setState(() {
                        _isPlaying = false;
                      });
                    },
                    icon: const Icon(Icons.pause),
                    label: const Text('Pause'),
                  ),
                  ElevatedButton.icon(
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
                  ElevatedButton.icon(
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
                      Text('Playback Status: ${_isPlaying ? 'Playing' : 'Stopped/Paused'}'),
                      Text('Volume: ${(_volume * 100).toInt()}%'),
                      Text('Current Position: ${_formatTime(_currentPosition)}'),
                      Text('Duration: ${_formatTime(_duration)}'),
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
