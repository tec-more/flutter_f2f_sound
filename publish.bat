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

REM Check current directory
if not exist "pubspec.yaml" (
    echo [ERROR] Please run this script in flutter_f2f_sound root directory
    exit /b 1
)

echo [Step 1/6] Running code analysis...
flutter analyze
if errorlevel 1 (
    echo [ERROR] Code analysis failed, please fix errors
    exit /b 1
)
echo [SUCCESS] Code analysis passed
echo.

echo [Step 2/6] Running tests...
flutter test
if errorlevel 1 (
    echo [ERROR] Tests failed, please fix errors
    exit /b 1
)
echo [SUCCESS] Tests passed
echo.

echo [Step 3/6] Checking code format...
dart format --output=none --set-exit-if-changed .
if errorlevel 1 (
    echo [WARNING] Code format not standard, auto-formatting...
    dart format .
    echo [TIP] Please review formatted code and run again
    exit /b 1
)
echo [SUCCESS] Code format check passed
echo.

echo [Step 4/6] Dry run publish (checking for issues)...
dart pub publish --dry-run
if errorlevel 1 (
    echo [ERROR] Dry run failed, please fix errors
    exit /b 1
)
echo [SUCCESS] Dry run successful
echo.

echo [Step 5/6] Checking required files...
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

echo [Step 6/6] Publishing to pub.dev...
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
