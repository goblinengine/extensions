## MidiPlayer addon

This folder contains the Godot addon manifest for the native GDExtension.

## Files

- `midi_player.gdextension` – loads the platform-specific compiled library
- `icon.svg` – node icon for the editor
- `bin/` – build output folder (DLL/SO/DYLIB)

## Usage

1. **Rebuild the GDExtension** using SCons (see BUILD.md in project root).
2. **Import your audio files**: Place `.sf2` (SoundFont) and `.mid` (MIDI) files in your project. Godot will automatically import them as `SoundFontResource` and `MidiFileResource` respectively (the importers are built-in to the GDExtension).
3. **Add a `MidiPlayer` node** to your scene.
4. **Assign resources** in the Inspector:
   - Set `soundfont` property to a `SoundFontResource`
   - Set `midi` property to a `MidiFileResource`
5. **Configure audio routing** (optional):
   - Set `audio_bus` for MIDI playback routing
   - Enable `use_separate_notes_bus` if you want manual `note_on()`/`note_off()` calls to use a different audio bus
   - Set `notes_audio_bus` for the separate notes output
6. **Call `play()`** to start playback.

### Manual Note Playback

Use `note_on(preset_index, key, velocity)` and `note_off(preset_index, key)` to trigger notes programmatically.

Properties:
- `loop` (bool) – loop playback
- `volume` (float, linear)
- `midi_speed` (float) – playback speed multiplier
- `generator_buffer_length` (float, seconds)
- `audio_bus` (String) – main output bus
- `use_separate_notes_bus` (bool) – route manual notes to a different bus
- `notes_audio_bus` (String) – bus for manual note output


Properties:
- `loop` (bool)
- `volume` (float, linear)
- `generator_buffer_length` (seconds)
