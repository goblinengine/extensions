# Extensions GDExtension for Godot

A consolidated GDExtension plugin containing multiple components for Godot 4.5+.

## Components

### 🎵 MidiPlayer
Full-featured MIDI player with SoundFont 2 support for Godot. Play MIDI files with custom instrument samples.

**Features:**
- Load and play MIDI files (.mid, .midi)
- SoundFont 2 (.sf2) support
- Real-time note triggering
- Playback controls (play, stop, pause, resume)
- Speed control and looping
- Volume control per player
- Separate audio bus for manual notes
- Editor import plugins for seamless asset integration

**Classes:**
- `MidiPlayer` - Main player node
- `MidiFileResource` - MIDI file resource type
- `SoundFontResource` - SoundFont resource type

## Installation

1. Copy the entire `extensions` folder to your Godot project's `addons/` directory
2. Open your project in Godot 4.5 or higher
3. The plugin will be automatically loaded (no manual enabling required)

Your project structure should look like:
```
your_project/
├── addons/
│   └── extensions/
│       ├── extensions.gdextension
│       └── bin/
│           └── [compiled binaries]
└── project.godot
```

## Usage

### MidiPlayer Example

```gdscript
extends Node

var midi_player: MidiPlayer

func _ready():
    # Create MidiPlayer node
    midi_player = MidiPlayer.new()
    add_child(midi_player)
    
    # Load SoundFont and MIDI file
    midi_player.load_soundfont("res://assets/soundfont.sf2")
    midi_player.load_midi("res://assets/song.mid")
    
    # Configure playback
    midi_player.loop = true
    midi_player.volume = 0.8
    midi_player.midi_speed = 1.0
    
    # Start playback
    midi_player.play()

func _process(delta):
    if midi_player.is_playing():
        var position = midi_player.get_playback_position_seconds()
        var length = midi_player.get_length_seconds()
        print("Playing: %.1f / %.1f seconds" % [position, length])

# Manual note triggering
func play_note():
    # Play a piano note (preset 0, middle C, full velocity)
    midi_player.note_on(0, 60, 1.0)
    
func stop_note():
    midi_player.note_off(0, 60)
```

### Importing Assets

The plugin includes importers that automatically convert MIDI and SoundFont files:

1. **MIDI Files**: Drop `.mid` or `.midi` files into your project
   - They'll be imported as `MidiFileResource`
   - Assign to `MidiPlayer.midi` property

2. **SoundFont Files**: Drop `.sf2` files into your project
   - They'll be imported as `SoundFontResource`
   - Assign to `MidiPlayer.soundfont` property

## Properties

### MidiPlayer

| Property | Type | Description |
|----------|------|-------------|
| `soundfont` | `SoundFontResource` | SoundFont to use for synthesis |
| `midi` | `MidiFileResource` | MIDI file to play |
| `loop` | `bool` | Enable/disable looping |
| `volume` | `float` | Volume level (0.0 - 2.0) |
| `midi_speed` | `float` | Playback speed multiplier (0.1 - 4.0) |
| `generator_buffer_length` | `float` | Audio buffer size in seconds |
| `audio_bus` | `StringName` | Audio bus for playback |
| `use_separate_notes_bus` | `bool` | Use separate bus for manual notes |
| `notes_audio_bus` | `StringName` | Audio bus for manual notes |

## Methods

### MidiPlayer

| Method | Description |
|--------|-------------|
| `load_soundfont(path: String) -> bool` | Load SoundFont from file path |
| `load_midi(path: String) -> bool` | Load MIDI from file path |
| `play()` | Start/restart playback |
| `stop()` | Stop playback and reset position |
| `pause()` | Pause playback |
| `resume()` | Resume paused playback |
| `is_playing() -> bool` | Check if currently playing |
| `note_on(preset: int, key: int, velocity: float)` | Trigger a note |
| `note_off(preset: int, key: int)` | Stop a note |
| `note_off_all()` | Stop all notes |
| `get_length_seconds() -> float` | Get MIDI duration |
| `get_playback_position_seconds() -> float` | Get current position |

## Platform Support

Pre-built binaries are provided for:
- **Windows** (x86_64) - Debug & Release
- **Linux** (x86_64) - Debug & Release  
- **macOS** (Universal) - Debug & Release

## Requirements

- Godot 4.5 or higher
- The plugin is compatible with all Godot export targets

## Building from Source

See the [main repository README](https://github.com/yourusername/godot_extensions) for build instructions.

## License

This extension uses [TinySoundFont](https://github.com/schellingb/TinySoundFont) (MIT License) for MIDI synthesis.

## Support

For issues, feature requests, or contributions, visit the [GitHub repository](https://github.com/yourusername/godot_extensions).
