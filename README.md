# Godot Extensions Workspace

A unified workspace for multiple Godot 4.x GDExtensions sharing a single `godot-cpp` dependency and unified `addons/` directory.

## Quick Start

```bash
# Setup (one-time)
git submodule update --init --recursive
pip install scons

# Build MIDI Player (default)
scons platform=windows target=template_debug arch=x86_64

# Build other extensions
scons extension=dascript platform=windows target=template_debug arch=x86_64

# Or use convenience script
.\build.bat
.\build.bat dascript
```

## Directory Structure

```
godot_extensions/
├── godot-cpp/                    # Shared dependency (git submodule)
├── addons/                       # Shared addon output
│   ├── midi_player/
│   │   ├── midi_player.gdextension
│   │   └── bin/                  # Build output
│   └── dascript/
├── extensions/                   # Extension sources
│   ├── midi_player/
│   │   ├── SConstruct
│   │   ├── src/
│   │   └── thirdparty/
│   └── dascript/
├── .gitmodules                   # Root submodules (godot-cpp only)
├── SConstruct                    # Root build system
├── build.bat                     # Windows convenience script
└── README.md                     # This file
```

## Building

### Build by Extension Name

```bash
# Default (midi_player)
scons platform=windows target=template_debug arch=x86_64

# Specific extension
scons extension=midi_player platform=windows target=template_debug arch=x86_64
scons extension=dascript platform=windows target=template_debug arch=x86_64

# Or from extension directory
cd extensions/midi_player
scons platform=windows target=template_debug arch=x86_64
```

### Build Options

- `platform`: `windows`, `linux`, `macos`
- `target`: `template_debug`, `template_release`
- `arch`: `x86_64`, `x86_32`, `arm64`, etc.
- `extension`: `midi_player` (default), `dascript`, etc.

### Platform-Specific

**Windows - MinGW (recommended)**
```bash
# Install tools
pip install scons
# Then build
scons platform=windows target=template_debug arch=x86_64
```

**Windows - MSVC**
```bash
# Setup Visual Studio environment first
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
scons platform=windows target=template_debug arch=x86_64
```

**Linux**
```bash
sudo apt-get install build-essential scons pkg-config
scons platform=linux target=template_debug arch=x86_64
```

**macOS**
```bash
brew install scons
scons platform=macos target=template_debug arch=arm64
```

## Build Output

Built libraries appear in:
```
addons/midi_player/bin/
├── midi_player.windows.template_debug.x86_64.dll
├── midi_player.windows.template_release.x86_64.dll
├── libmidi_player.linux.template_debug.x86_64.so
├── libmidi_player.linux.template_release.x86_64.so
├── libmidi_player.macos.template_debug.universal.dylib
└── libmidi_player.macos.template_release.universal.dylib
```

Copy the entire `addons/` directory to your Godot project.

## Adding a New Extension

### 1. Create Directory Structure

```bash
mkdir extensions/myextension
mkdir extensions/myextension/src
mkdir -p addons/myextension/bin
```

### 2. Create `extensions/myextension/SConstruct`

Template:
```python
#!/usr/bin/env python

import os

if not os.path.isdir("../../godot-cpp"):
    print("ERROR: Missing ../../godot-cpp")
    Exit(1)

godot_env = SConscript("../../godot-cpp/SConstruct")
env = godot_env.Clone()

sources = [
    "src/myextension.cpp",
    "src/register_types.cpp",
]

env.AppendUnique(CPPPATH=["src"])

suffix = env.get("suffix", "")
out_dir = "../../addons/myextension/bin"

if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

if env["platform"] == "windows":
    env["SHLIBPREFIX"] = ""

lib_basename = "myextension" + suffix
target_path = os.path.join(out_dir, lib_basename + env["SHLIBSUFFIX"])

lib = env.SharedLibrary(target=env.File(target_path), source=sources)
Default(lib)
```

**Key points:**
- Reference `../../godot-cpp/` (two levels up)
- Output to `../../addons/myextension/bin/`
- Use same path structure as `extensions/midi_player/SConstruct`

### 3. Create `addons/myextension/myextension.gdextension`

```ini
[configuration]
entry_symbol = "myextension_library_init"
compatibility_minimum = "4.5"
reloadable = true

[libraries]
windows.release.x86_64 = "res://addons/myextension/bin/myextension.windows.template_release.x86_64.dll"
windows.debug.x86_64 = "res://addons/myextension/bin/myextension.windows.template_debug.x86_64.dll"
linux.debug.x86_64 = "res://addons/myextension/bin/libmyextension.linux.template_debug.x86_64.so"
linux.release.x86_64 = "res://addons/myextension/bin/libmyextension.linux.template_release.x86_64.so"
macos.debug.universal = "res://addons/myextension/bin/libmyextension.macos.template_debug.universal.dylib"
macos.release.universal = "res://addons/myextension/bin/libmyextension.macos.template_release.universal.dylib"
```

### 4. Add to Root SConstruct (if needed)

Update `EXTENSIONS` dict in root `SConstruct`:
```python
EXTENSIONS = {
    "midi_player": "Build MIDI Player GDExtension",
    "dascript": "Build DaScript GDExtension",
    "myextension": "Build My Extension",  # Add this
}
```

### 5. Build

```bash
scons extension=myextension platform=windows target=template_debug arch=x86_64
```

## Requirements

- Python 3.8+
- SCons: `pip install scons`
- C++ compiler
  - Windows: MinGW-w64 or MSVC
  - Linux: GCC
  - macOS: Clang

## Troubleshooting

**"ERROR: Missing ../../godot-cpp"**
```bash
git submodule update --init --recursive
```

**Build fails with path errors**
- Ensure building from repository root or extension directory
- Check that `SConstruct` paths are correct (`../../` from `extensions/`)

**"scons: command not found"**
```bash
pip install scons
```

**MSVC build errors**
```bash
# Set up environment first
"C:\Program Files\...\vcvarsall.bat" x64
```

**Clean build artifacts**
```bash
rm -rf addons/*/bin
```

## Key Features

✅ **Single Shared godot-cpp** - One copy for all extensions
✅ **Unified Addon Structure** - All addons in one directory
✅ **Scalable** - Add unlimited extensions with same pattern
✅ **Flexible Building** - Build from root or extension directory
✅ **Reduced Disk Space** - No duplication of godot-cpp

## File Structure Details

### Extensions Location
- **Before**: `midi_player/` at root
- **Now**: `extensions/midi_player/`
- **Benefit**: Clear separation of extension sources and shared resources

### Addon Output
- **Before**: `midi_player/addons/`
- **Now**: `addons/midi_player/`
- **Benefit**: Single addon directory for Godot projects

### Dependency Sharing
- **godot-cpp**: Shared at root, referenced via `../../godot-cpp/` from extension SConstruct
- **Extension-specific deps**: In `extensions/[name]/thirdparty/` (e.g., TinySoundFont)

## Resources

- [Godot C++ GDExtension Docs](https://docs.godotengine.org/en/stable/contributing/core_and_modules/custom_modules_in_cpp.html)
- [godot-cpp Repository](https://github.com/godotengine/godot-cpp)
- [SCons Documentation](https://scons.org/)

