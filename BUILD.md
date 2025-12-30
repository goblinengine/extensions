# Building Godot Extensions

This repository contains multiple Godot extensions as git submodules. Each extension can be built independently.

## Architecture

```
godot_extensions/                    # Main repo
├── godot-cpp/                       # Shared godot-cpp (submodule)
├── extensions/
│   ├── midi_player/                 # Independent repo (submodule)
│   │   ├── build.bat               # Extension-specific build script
│   │   ├── SConstruct              # References ../../godot-cpp
│   │   └── src/
│   └── dascript/                   # Another extension (submodule)
└── addons/                         # Build outputs land here
    └── midi_player/
        └── bin/
```

## Build Options

### Option 1: Build Individual Extensions (Recommended)

Navigate to the extension directory and build:

```batch
cd extensions\midi_player
.\build.bat                           # Debug build
.\build.bat template_release         # Release build
```

### Option 2: Build from Main Repo

Use the wrapper script from the main repo:

```batch
.\build.bat                           # Build midi_player (default)
.\build.bat midi_player              # Build midi_player debug
.\build.bat midi_player template_release  # Build midi_player release
.\build.bat dascript                # Build dascript
```

## Setup Requirements

1. **Clone with submodules:**
   ```bash
   git clone --recurse-submodules <your-repo-url>
   ```
   
   Or if already cloned:
   ```bash
   git submodule update --init --recursive
   ```

2. **Visual Studio 2022** (or compatible MSVC compiler)
   - The build scripts automatically detect and configure MSVC

3. **SCons** build tool:
   ```bash
   pip install scons
   ```

## Extension Development

Each extension is an independent git repository that:
- Has its own build system (`build.bat`, `SConstruct`)
- References the shared `../../godot-cpp` submodule
- Builds outputs to `../../addons/<extension_name>/bin/`
- Can be developed, tested, and released independently

To add a new extension:
1. Create the extension repo with proper build scripts
2. Add as submodule: `git submodule add <repo-url> extensions/<name>`
3. Ensure it references `../../godot-cpp` in its SConstruct
4. Update this README

## Troubleshooting

**Issue:** SCons can't find MSVC compiler

**Solution:** The extension's `build.bat` automatically sets up MSVC. Make sure you're running from the extension directory or using the main `build.bat` wrapper.

**Issue:** godot-cpp submodule is empty

**Solution:**
```bash
git submodule update --init --recursive
```
