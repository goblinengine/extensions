#!/usr/bin/env python

import os

# Root SConstruct for building all Godot extensions
# This enables building multiple extensions with a single shared godot-cpp dependency

# Available extensions to build
EXTENSIONS = {
    "midi_player": "Build MIDI Player GDExtension",
    "dascript": "Build DaScript GDExtension (if enabled)",
}

# Get build target from command line (e.g., scons extension=midi_player)
extension_target = ARGUMENTS.get("extension", "midi_player")

if extension_target not in EXTENSIONS:
    print(f"ERROR: Unknown extension '{extension_target}'")
    print(f"Available extensions: {', '.join(EXTENSIONS.keys())}")
    print(f"\nUsage: scons extension=<name> platform=<platform> target=<target> arch=<arch>")
    for ext, desc in EXTENSIONS.items():
        print(f"  - {ext}: {desc}")
    Exit(1)

print(f"Building extension: {extension_target}")

# Verify godot-cpp exists
if not os.path.isdir("godot-cpp"):
    print("ERROR: Missing ./godot-cpp. Ensure godot-cpp is set up as a git submodule:")
    print("  git submodule update --init --recursive")
    Exit(1)

# Build the selected extension
extension_path = f"extensions/{extension_target}"
if os.path.isdir(extension_path):
    print(f"Building {extension_target} from {extension_path}/")
    SConscript(f"{extension_path}/SConstruct")
else:
    print(f"ERROR: Extension directory '{extension_path}' not found")
    Exit(1)
