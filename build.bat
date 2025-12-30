@echo off
REM Build script for Godot Extensions
REM Each extension builds independently using its own build script

if "%~1"=="help" goto :show_help
if "%~1"=="" goto :build_default

set EXTENSION=%~1
set TARGET=%~2

if not exist "extensions\%EXTENSION%" (
    echo ERROR: Extension not found: extensions\%EXTENSION%
    exit /b 1
)

echo Building %EXTENSION%...
cd extensions\%EXTENSION%
if "%TARGET%"=="" (
    call build.bat
) else (
    call build.bat %TARGET%
)
set BUILD_ERROR=%errorlevel%
cd ..\..
exit /b %BUILD_ERROR%

:build_default
echo Building midi_player (default)...
cd extensions\midi_player
call build.bat
cd ..\..
exit /b %errorlevel%

:show_help
echo.
echo Godot Extensions Build
echo.
echo Usage: build.bat [extension] [target]
echo.
echo Examples:
echo   build.bat                          # Build midi_player debug
echo   build.bat midi_player              # Build midi_player debug
echo   build.bat midi_player template_release  # Release build
echo   build.bat dascript                 # Build dascript
echo.
echo Available extensions:
echo   - midi_player
echo   - dascript
exit /b 0
