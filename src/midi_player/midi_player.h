#pragma once

#include <cstdint>

#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/vector.hpp>

#include "midi_resources.h"

// TinySoundFont / TinyMidiLoader forward declarations.
struct tsf;
struct tml_message;

namespace godot {

class MidiPlayer : public Node {
	GDCLASS(MidiPlayer, Node)

public:
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
