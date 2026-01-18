#!/usr/bin/env python

import os
import sys

# Add godot-cpp tools folder so we can use their build tools
godot_cpp_path = "godot-cpp"
if os.path.isdir(godot_cpp_path):
    sys.path.append(os.path.abspath(os.path.join(godot_cpp_path, "tools")))

# Import godot-cpp's SConstruct to get the configured env
godot_env = SConscript("godot-cpp/SConstruct")

env = godot_env.Clone()

# Project sources - organize by component
sources = [
    # Register types (main entry point)
    "src/register_types.cpp",

    # Upscaling / render scaling
    "src/upscale_viewport.cpp",

    # Compound mesh instance (visual-only, RID-backed)
    "src/compound_mesh_instance_3d.cpp",
    "src/compound_part_proxy.cpp",
    "src/compound_part_node.cpp",
    
    # MidiPlayer component
    "src/midi_player/midi_player.cpp",
    "src/midi_player/midi_resources.cpp",
    "src/midi_player/midi_importers.cpp",
    "src/midi_player/midi_editor_plugin.cpp",
    "src/midi_player/thirdparty_tsf_tml.cpp",

    # Lightmap baker component
    "src/lightmap_baker/lightmap_baker.cpp",

    # Third-party: xatlas (runtime UV2 unwrapping)
    "lib/xatlas/source/xatlas/xatlas.cpp",
]

env.AppendUnique(CPPPATH=[
    "src",
    "src/midi_player",
    "lib/TinySoundFont",
	"lib/xatlas/source/xatlas",
])

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# Build output naming.
# godot-cpp exposes env['suffix'] like: .windows.template_debug.x86_64
suffix = env.get("suffix", "")

# Shared library naming.
lib_basename = "extensions" + suffix

# Emit into the godot_project addon bin folder so Godot can load it.
out_dir = "godot_project/addons/extensions/bin"

# Ensure output dir exists.
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

# SCons' SharedLibrary will add platform-specific prefixes/suffixes.
# For Windows we want no 'lib' prefix, for others 'lib' is fine.
if env["platform"] == "windows":
    env["SHLIBPREFIX"] = ""

# Set the target filename explicitly.
target_path = os.path.join(out_dir, lib_basename + env["SHLIBSUFFIX"])

lib = env.SharedLibrary(
    target=env.File(target_path),
    source=sources,
)

# Default build target.
Default(lib)
