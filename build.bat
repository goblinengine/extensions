@echo off
REM Build script for Godot Extensions
REM Builds from extensions/ directory with shared godot-cpp

if "%1"=="" (
    echo Building midi_player (default)...
    scons extension=midi_player platform=windows target=template_debug arch=x86_64
    exit /b !errorlevel!
)

if "%1"=="help" (
    echo.
    echo Godot Extensions Build
    echo.
    echo Usage: build.bat [extension] [platform] [target] [arch]
    echo.
    echo Examples:
    echo   build.bat                          # Build midi_player debug
    echo   build.bat dascript                 # Build dascript
    echo   build.bat midi_player template_release  # Release build
    exit /b 0
)

setlocal enabledelayedexpansion
set EXTENSION=%1
set PLATFORM=%2
if "!PLATFORM!"=="" set PLATFORM=windows
set TARGET=%3
if "!TARGET!"=="" set TARGET=template_debug
set ARCH=%4
if "!ARCH!"=="" set ARCH=x86_64

echo Building !EXTENSION! ^(platform: !PLATFORM!, target: !TARGET!, arch: !ARCH!^)
scons extension=!EXTENSION! platform=!PLATFORM! target=!TARGET! arch=!ARCH!
exit /b !errorlevel!
