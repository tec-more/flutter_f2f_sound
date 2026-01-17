@echo off
REM flutter_f2f_sound Publishing Tool for Windows
REM Usage: publish.bat

REM Set UTF-8 encoding for proper display
chcp 65001 >nul

setlocal enabledelayedexpansion

echo ========================================
echo flutter_f2f_sound Publishing Tool
echo ========================================
echo.

REM Check if proxy is needed for Google services
echo [INFO] If you need proxy to access Google services:
echo   1. Make sure your VPN/proxy is running
echo   2. Set proxy: set HTTP_PROXY=http://127.0.0.1:7897
echo   3. Set proxy: set HTTPS_PROXY=http://127.0.0.1:7897
echo   4. Or modify the proxy addresses below
echo.

REM Optional: Set proxy if needed (uncomment and modify)
REM set HTTP_PROXY=http://127.0.0.1:7897
REM set HTTPS_PROXY=http://127.0.0.1:7897

REM Check current directory
if not exist "pubspec.yaml" (
    echo [ERROR] Please run this script in flutter_f2f_sound root directory
    exit /b 1
)

echo [Step 1/5] Running code analysis...
flutter analyze
if errorlevel 1 (
    echo [ERROR] Code analysis failed, please fix errors
    exit /b 1
)
echo [SUCCESS] Code analysis passed
echo.

echo [Step 2/5] Checking code format...
dart format --output=none --set-exit-if-changed .
if errorlevel 1 (
    echo [WARNING] Code format not standard, auto-formatting...
    dart format .
    echo [TIP] Please review formatted code and run again
    exit /b 1
)
echo [SUCCESS] Code format check passed
echo.

echo [Step 3/5] Dry run publish (checking for issues)...
dart pub publish --dry-run
if errorlevel 1 (
    echo [ERROR] Dry run failed, please fix errors
    exit /b 1
)
echo [SUCCESS] Dry run successful
echo.

echo [Step 4/5] Checking required files...
set "files_ok=1"
if not exist "README.md" (
    echo [ERROR] Missing required file: README.md
    set "files_ok=0"
)
if not exist "CHANGELOG.md" (
    echo [ERROR] Missing required file: CHANGELOG.md
    set "files_ok=0"
)
if not exist "LICENSE" (
    echo [ERROR] Missing required file: LICENSE
    set "files_ok=0"
)
if "!files_ok!"=="0" exit /b 1
echo [SUCCESS] All required files exist
echo.

echo [Step 5/5] Publishing to pub.dev...
echo.
echo ========================================
echo IMPORTANT NOTICE
echo ========================================
echo You are about to publish to pub.dev, ensure you have:
echo   1. Updated version number
echo   2. Updated CHANGELOG.md
echo   3. Reviewed all changes
echo.
set /p confirm="Confirm publish? (yes/no): "

if not "%confirm%"=="yes" (
    echo [CANCELLED] Publishing cancelled
    exit /b 0
)

echo.
echo [PUBLISHING] Setting PUB_HOSTED_URL to pub.dev...
set PUB_HOSTED_URL=https://pub.dev
echo [PUBLISHING] Starting...
dart pub publish

if errorlevel 1 (
    echo.
    echo [ERROR] Publishing failed
    exit /b 1
) else (
    echo.
    echo ========================================
    echo Publishing Successful!
    echo ========================================
    echo Visit https://pub.dev/packages/flutter_f2f_sound to view your package
)

endlocal
