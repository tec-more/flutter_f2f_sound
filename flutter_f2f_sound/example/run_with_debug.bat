@echo off
REM Run the Flutter app and capture Debug Output

echo Starting Flutter app with debug output monitoring...
echo.

REM Start the app
start "" "build\windows\x64\runner\Debug\flutter_f2f_sound_example.exe"

REM Wait for the app to start
timeout /t 3 /nobreak >nul

echo.
echo App started. Click the Play button in the app to trigger playback.
echo.
echo Debug output will appear below (watch for "Play called" message):
echo.

REM Monitor debug output for 30 seconds
powershell -Command "& {Add-Type -AssemblyName System.Diagnostics;$proc=Start-Process 'build\windows\x64\runner\Debug\flutter_f2f_sound_example.exe' -PassThru -NoNewWindow;Start-Sleep -Seconds 30;$proc.Kill()}" 2>&1

echo.
echo Monitoring stopped.
pause
