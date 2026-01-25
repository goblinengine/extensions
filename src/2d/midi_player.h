#pragma once

#include <cstdint>

#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/vector.hpp>

#include "resources/midi_resources.h"

// TinySoundFont / TinyMidiLoader forward declarations.
struct tsf;
struct tml_message;

namespace godot {

class MidiPlayer : public Node {
	GDCLASS(MidiPlayer, Node)

public:
	enum GeneralMidiInstrument {
		GM_ACOUSTIC_GRAND_PIANO = 0,
		GM_BRIGHT_PIANO = 1,
		GM_ELECTRIC_GRAND_PIANO = 2,
		GM_HONKY_TONK_PIANO = 3,
		GM_ELECTRIC_PIANO_1 = 4,
		GM_ELECTRIC_PIANO_2 = 5,
		GM_HARPSICHORD = 6,
		GM_CLAV = 7,
		GM_CELESTA = 8,
		GM_GLOCKENSPIEL = 9,
		GM_MUSIC_BOX = 10,
		GM_VIBRAPHONE = 11,
		GM_MARIMBA = 12,
		GM_XYLOPHONE = 13,
		GM_TUBULAR_BELLS = 14,
		GM_DULCIMER = 15,
		GM_DRAWBAR_ORGAN = 16,
		GM_PERCUSSIVE_ORGAN = 17,
		GM_ROCK_ORGAN = 18,
		GM_CHURCH_ORGAN = 19,
		GM_REED_ORGAN = 20,
		GM_ACCORDION = 21,
		GM_HARMONICA = 22,
		GM_TANGO_ACCORDION = 23,
		GM_NYLON_STRING_GUITAR = 24,
		GM_STEEL_STRING_GUITAR = 25,
		GM_JAZZ_GUITAR = 26,
		GM_CLEAN_ELECTRIC_GUITAR = 27,
		GM_MUTED_ELECTRIC_GUITAR = 28,
		GM_OVERDRIVE_GUITAR = 29,
		GM_DISTORTION_GUITAR = 30,
		GM_GUITAR_HARMONICS = 31,
		GM_ACOUSTIC_BASS = 32,
		GM_FINGERED_BASS = 33,
		GM_PICKED_BASS = 34,
		GM_FRETLESS_BASS = 35,
		GM_SLAP_BASS_1 = 36,
		GM_SLAP_BASS_2 = 37,
		GM_SYNTH_BASS_1 = 38,
		GM_SYNTH_BASS_2 = 39,
		GM_VIOLIN = 40,
		GM_VIOLA = 41,
		GM_CELLO = 42,
		GM_CONTRABASS = 43,
		GM_TREMOLO_STRINGS = 44,
		GM_PIZZICATO_STRINGS = 45,
		GM_ORCHESTRAL_HARP = 46,
		GM_TIMPANI = 47,
		GM_STRING_ENSEMBLE_1 = 48,
		GM_STRING_ENSEMBLE_2 = 49,
		GM_SYNTH_STRINGS_1 = 50,
		GM_SYNTH_STRINGS_2 = 51,
		GM_CHOIR_AAHS = 52,
		GM_CHOIR_OOHS = 53,
		GM_SYNTH_VOICE = 54,
		GM_ORCHESTRAL_HIT = 55,
		GM_TRUMPET = 56,
		GM_TROMBONE = 57,
		GM_TUBA = 58,
		GM_MUTED_TRUMPET = 59,
		GM_FRENCH_HORN = 60,
		GM_BRASS_SECTION = 61,
		GM_SYNTH_BRASS_1 = 62,
		GM_SYNTH_BRASS_2 = 63,
		GM_SOPRANO_SAX = 64,
		GM_ALTO_SAX = 65,
		GM_TENOR_SAX = 66,
		GM_BARITONE_SAX = 67,
		GM_OBOE = 68,
		GM_ENGLISH_HORN = 69,
		GM_BASSOON = 70,
		GM_CLARINET = 71,
		GM_PICCOLO = 72,
		GM_FLUTE = 73,
		GM_RECORDER = 74,
		GM_PAN_FLUTE = 75,
		GM_BLOWN_BOTTLE = 76,
		GM_SHAKUHACHI = 77,
		GM_WHISTLE = 78,
		GM_OCARINA = 79,
		GM_SQUARE_WAVE = 80,
		GM_SAWTOOTH_WAVE = 81,
		GM_CALLIOPE = 82,
		GM_CHIFF = 83,
		GM_CHARANG = 84,
		GM_VOICE = 85,
		GM_FIFTHS = 86,
		GM_BASS_AND_LEAD = 87,
		GM_NEW_AGE = 88,
		GM_WARM = 89,
		GM_POLYSYNTH = 90,
		GM_CHOIR = 91,
		GM_BOWED = 92,
		GM_METALLIC = 93,
		GM_HALO = 94,
		GM_SWEEP = 95,
		GM_FX_RAIN = 96,
		GM_FX_SOUNDTRACK = 97,
		GM_FX_CRYSTAL = 98,
		GM_FX_ATMOSPHERE = 99,
		GM_FX_BRIGHTNESS = 100,
		GM_FX_GOBLINS = 101,
		GM_FX_ECHO_DROPS = 102,
		GM_FX_STAR_THEME = 103,
		GM_SITAR = 104,
		GM_BANJO = 105,
		GM_SHAMISEN = 106,
		GM_KOTO = 107,
		GM_KALIMBA = 108,
		GM_BAGPIPE = 109,
		GM_FIDDLE = 110,
		GM_SHANAI = 111,
		GM_TINKLE_BELL = 112,
		GM_AGOGO = 113,
		GM_STEEL_DRUMS = 114,
		GM_WOODBLOCK = 115,
		GM_TAIKO_DRUM = 116,
		GM_MELODIC_TOM = 117,
		GM_SYNTH_DRUM = 118,
		GM_REVERSE_CYMBAL = 119,
		GM_GUITAR_FRET_NOISE = 120,
		GM_BREATH_NOISE = 121,
		GM_SEASHORE = 122,
		GM_BIRD_TWEET = 123,
		GM_TELEPHONE_RING = 124,
		GM_HELICOPTER = 125,
		GM_APPLAUSE = 126,
		GM_GUNSHOT = 127,
	};

	enum MidiNote {
		NOTE_C0 = 0,
		NOTE_CS0 = 1,
		NOTE_D0 = 2,
		NOTE_DS0 = 3,
		NOTE_E0 = 4,
		NOTE_F0 = 5,
		NOTE_FS0 = 6,
		NOTE_G0 = 7,
		NOTE_GS0 = 8,
		NOTE_A0 = 9,
		NOTE_AS0 = 10,
		NOTE_B0 = 11,
		NOTE_C1 = 12,
		NOTE_CS1 = 13,
		NOTE_D1 = 14,
		NOTE_DS1 = 15,
		NOTE_E1 = 16,
		NOTE_F1 = 17,
		NOTE_FS1 = 18,
		NOTE_G1 = 19,
		NOTE_GS1 = 20,
		NOTE_A1 = 21,
		NOTE_AS1 = 22,
		NOTE_B1 = 23,
		NOTE_C2 = 24,
		NOTE_CS2 = 25,
		NOTE_D2 = 26,
		NOTE_DS2 = 27,
		NOTE_E2 = 28,
		NOTE_F2 = 29,
		NOTE_FS2 = 30,
		NOTE_G2 = 31,
		NOTE_GS2 = 32,
		NOTE_A2 = 33,
		NOTE_AS2 = 34,
		NOTE_B2 = 35,
		NOTE_C3 = 36,
		NOTE_CS3 = 37,
		NOTE_D3 = 38,
		NOTE_DS3 = 39,
		NOTE_E3 = 40,
		NOTE_F3 = 41,
		NOTE_FS3 = 42,
		NOTE_G3 = 43,
		NOTE_GS3 = 44,
		NOTE_A3 = 45,
		NOTE_AS3 = 46,
		NOTE_B3 = 47,
		NOTE_C4 = 48,
		NOTE_CS4 = 49,
		NOTE_D4 = 50,
		NOTE_DS4 = 51,
		NOTE_E4 = 52,
		NOTE_F4 = 53,
		NOTE_FS4 = 54,
		NOTE_G4 = 55,
		NOTE_GS4 = 56,
		NOTE_A4 = 57,
		NOTE_AS4 = 58,
		NOTE_B4 = 59,
		NOTE_C5 = 60,
		NOTE_CS5 = 61,
		NOTE_D5 = 62,
		NOTE_DS5 = 63,
		NOTE_E5 = 64,
		NOTE_F5 = 65,
		NOTE_FS5 = 66,
		NOTE_G5 = 67,
		NOTE_GS5 = 68,
		NOTE_A5 = 69,
		NOTE_AS5 = 70,
		NOTE_B5 = 71,
		NOTE_C6 = 72,
		NOTE_CS6 = 73,
		NOTE_D6 = 74,
		NOTE_DS6 = 75,
		NOTE_E6 = 76,
		NOTE_F6 = 77,
		NOTE_FS6 = 78,
		NOTE_G6 = 79,
		NOTE_GS6 = 80,
		NOTE_A6 = 81,
		NOTE_AS6 = 82,
		NOTE_B6 = 83,
		NOTE_C7 = 84,
		NOTE_CS7 = 85,
		NOTE_D7 = 86,
		NOTE_DS7 = 87,
		NOTE_E7 = 88,
		NOTE_F7 = 89,
		NOTE_FS7 = 90,
		NOTE_G7 = 91,
		NOTE_GS7 = 92,
		NOTE_A7 = 93,
		NOTE_AS7 = 94,
		NOTE_B7 = 95,
		NOTE_C8 = 96,
		NOTE_CS8 = 97,
		NOTE_D8 = 98,
		NOTE_DS8 = 99,
		NOTE_E8 = 100,
		NOTE_F8 = 101,
		NOTE_FS8 = 102,
		NOTE_G8 = 103,
		NOTE_GS8 = 104,
		NOTE_A8 = 105,
		NOTE_AS8 = 106,
		NOTE_B8 = 107,
		NOTE_C9 = 108,
		NOTE_CS9 = 109,
		NOTE_D9 = 110,
		NOTE_DS9 = 111,
		NOTE_E9 = 112,
		NOTE_F9 = 113,
		NOTE_FS9 = 114,
		NOTE_G9 = 115,
		NOTE_GS9 = 116,
		NOTE_A9 = 117,
		NOTE_AS9 = 118,
		NOTE_B9 = 119,
		NOTE_C10 = 120,
		NOTE_CS10 = 121,
		NOTE_D10 = 122,
		NOTE_DS10 = 123,
		NOTE_E10 = 124,
		NOTE_F10 = 125,
		NOTE_FS10 = 126,
		NOTE_G10 = 127,
	};

	enum MidiDrumNote {
		DRUM_ACOUSTIC_BASS_DRUM = 35,
		DRUM_BASS_DRUM_1 = 36,
		DRUM_SIDE_STICK = 37,
		DRUM_ACOUSTIC_SNARE = 38,
		DRUM_HAND_CLAP = 39,
		DRUM_ELECTRIC_SNARE = 40,
		DRUM_LOW_FLOOR_TOM = 41,
		DRUM_CLOSED_HI_HAT = 42,
		DRUM_HIGH_FLOOR_TOM = 43,
		DRUM_PEDAL_HI_HAT = 44,
		DRUM_LOW_TOM = 45,
		DRUM_OPEN_HI_HAT = 46,
		DRUM_LOW_MID_TOM = 47,
		DRUM_HI_MID_TOM = 48,
		DRUM_CRASH_CYMBAL_1 = 49,
		DRUM_HIGH_TOM = 50,
		DRUM_RIDE_CYMBAL_1 = 51,
		DRUM_CHINESE_CYMBAL = 52,
		DRUM_RIDE_BELL = 53,
		DRUM_TAMBOURINE = 54,
		DRUM_SPLASH_CYMBAL = 55,
		DRUM_COWBELL = 56,
		DRUM_CRASH_CYMBAL_2 = 57,
		DRUM_VIBRASLAP = 58,
		DRUM_RIDE_CYMBAL_2 = 59,
		DRUM_HI_BONGO = 60,
		DRUM_LOW_BONGO = 61,
		DRUM_MUTE_HI_CONGA = 62,
		DRUM_OPEN_HI_CONGA = 63,
		DRUM_LOW_CONGA = 64,
		DRUM_HIGH_TIMBALE = 65,
		DRUM_LOW_TIMBALE = 66,
		DRUM_HIGH_AGOGO = 67,
		DRUM_LOW_AGOGO = 68,
		DRUM_CABASA = 69,
		DRUM_MARACAS = 70,
		DRUM_SHORT_WHISTLE = 71,
		DRUM_LONG_WHISTLE = 72,
		DRUM_SHORT_GUIRO = 73,
		DRUM_LONG_GUIRO = 74,
		DRUM_CLAVES = 75,
		DRUM_HI_WOOD_BLOCK = 76,
		DRUM_LOW_WOOD_BLOCK = 77,
		DRUM_MUTE_CUICA = 78,
		DRUM_OPEN_CUICA = 79,
		DRUM_MUTE_TRIANGLE = 80,
		DRUM_OPEN_TRIANGLE = 81,
		DRUM_SHAKER = 82,
	};

	MidiPlayer();
	~MidiPlayer();

	void set_soundfont(const Ref<SoundFontResource> &p_resource);
	Ref<SoundFontResource> get_soundfont() const;

	void set_midi(const Ref<MidiFileResource> &p_resource);
	Ref<MidiFileResource> get_midi() const;

	void set_loop(bool p_loop);
	bool get_loop() const;

	void set_looping(bool p_looping);
	bool is_looping() const;

	void set_midi_speed(float p_speed);
	float get_midi_speed() const;

	void set_volume(float p_volume);
	float get_volume() const;

	void set_generator_buffer_length(float p_seconds);
	float get_generator_buffer_length() const;

	void set_audio_bus(const StringName &p_bus);
	StringName get_audio_bus() const;

	void set_use_separate_notes_bus(bool p_enable);
	bool get_use_separate_notes_bus() const;

	void set_notes_audio_bus(const StringName &p_bus);
	StringName get_notes_audio_bus() const;

	bool load_soundfont(const String &p_path);
	bool load_midi(const String &p_path);

	void play();
	void stop();
	void pause();
	void resume();
	bool is_playing() const;

	void note_on(int p_preset_index, int p_key, float p_velocity);
	void note_off(int p_preset_index, int p_key);
	void note_off_all();

	float get_length_seconds() const;
	float get_playback_position_seconds() const;

	// Virtual methods (public for godot-cpp binding)
	void _ready() override;
	void _exit_tree() override;
	void _process(double p_delta) override;

protected:
	static void _bind_methods();
	void _ensure_audio_setup();
	void _ensure_notes_audio_setup();
	void _clear_audio_buffer();
	void _clear_notes_audio_buffer();
	void _reset_synth();
	void _reset_notes_synth();
	bool _load_soundfont_bytes(const PackedByteArray &p_bytes);
	bool _load_notes_soundfont_bytes(const PackedByteArray &p_bytes);
	bool _load_midi_bytes(const PackedByteArray &p_bytes);
	static PackedByteArray _read_all_bytes(const String &p_path);
	void _apply_event(const tml_message *p_msg);
	void _process_events_until_ms(uint32_t p_time_ms);
	void _pump_audio(bool p_process_events);
	void _pump_notes_audio();

	Ref<SoundFontResource> soundfont_resource;
	Ref<MidiFileResource> midi_resource;
	PackedByteArray soundfont_bytes_cache;

	bool loop = false;
	float volume = 1.0f; // linear gain
	float midi_speed = 1.0f; // playback speed multiplier
	float generator_buffer_length = 0.5f;
	StringName audio_bus = "Master";
	bool use_separate_notes_bus = false;
	StringName notes_audio_bus = "Master";

	// Godot audio output
	AudioStreamPlayer *player = nullptr;
	Ref<AudioStreamGenerator> generator;
	Ref<AudioStreamPlayback> playback_base;
	AudioStreamGeneratorPlayback *playback = nullptr; // borrowed from playback_base

	AudioStreamPlayer *notes_player = nullptr;
	Ref<AudioStreamGenerator> notes_generator;
	Ref<AudioStreamPlayback> notes_playback_base;
	AudioStreamGeneratorPlayback *notes_playback = nullptr; // borrowed from notes_playback_base
	int sample_rate = 44100;

	// Synth/midi
	tsf *sf = nullptr;
	tsf *notes_sf = nullptr;
	tml_message *midi = nullptr;
	tml_message *event_cursor = nullptr;

	uint32_t midi_length_ms = 0;
	bool playing = false;
	bool paused = false;

	// Amount of audio already generated since play() in seconds.
	double synth_time_sec = 0.0;
	double notes_time_sec = 0.0;
};

} // namespace godot
