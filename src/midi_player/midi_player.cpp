#include "midi_player.h"

#include <algorithm>
#include <vector>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../../lib/TinySoundFont/tsf.h"
#include "../../lib/TinySoundFont/tml.h"

namespace godot {

static constexpr int k_block_frames = 64;

MidiPlayer::MidiPlayer() {
	set_process(true);
}

MidiPlayer::~MidiPlayer() {
	stop();
	if (midi) {
		tml_free(midi);
		midi = nullptr;
	}
	if (sf) {
		tsf_close(sf);
		sf = nullptr;
	}
	if (notes_sf) {
		tsf_close(notes_sf);
		notes_sf = nullptr;
	}
}

void MidiPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_soundfont", "resource"), &MidiPlayer::set_soundfont);
	ClassDB::bind_method(D_METHOD("get_soundfont"), &MidiPlayer::get_soundfont);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::OBJECT, "soundfont", PROPERTY_HINT_RESOURCE_TYPE, "SoundFontResource"), "set_soundfont", "get_soundfont");

	ClassDB::bind_method(D_METHOD("set_midi", "resource"), &MidiPlayer::set_midi);
	ClassDB::bind_method(D_METHOD("get_midi"), &MidiPlayer::get_midi);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::OBJECT, "midi", PROPERTY_HINT_RESOURCE_TYPE, "MidiFileResource"), "set_midi", "get_midi");

	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &MidiPlayer::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &MidiPlayer::get_loop);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");

	ClassDB::bind_method(D_METHOD("set_looping", "looping"), &MidiPlayer::set_looping);
	ClassDB::bind_method(D_METHOD("is_looping"), &MidiPlayer::is_looping);

	ClassDB::bind_method(D_METHOD("set_midi_speed", "speed"), &MidiPlayer::set_midi_speed);
	ClassDB::bind_method(D_METHOD("get_midi_speed"), &MidiPlayer::get_midi_speed);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::FLOAT, "midi_speed", PROPERTY_HINT_RANGE, "0.1,4.0,0.01"), "set_midi_speed", "get_midi_speed");

	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &MidiPlayer::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &MidiPlayer::get_volume);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_volume", "get_volume");

	ClassDB::bind_method(D_METHOD("set_generator_buffer_length", "seconds"), &MidiPlayer::set_generator_buffer_length);
	ClassDB::bind_method(D_METHOD("get_generator_buffer_length"), &MidiPlayer::get_generator_buffer_length);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::FLOAT, "generator_buffer_length", PROPERTY_HINT_RANGE, "0.05,2.0,0.01"), "set_generator_buffer_length", "get_generator_buffer_length");

	ClassDB::bind_method(D_METHOD("set_audio_bus", "bus"), &MidiPlayer::set_audio_bus);
	ClassDB::bind_method(D_METHOD("get_audio_bus"), &MidiPlayer::get_audio_bus);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::STRING_NAME, "audio_bus"), "set_audio_bus", "get_audio_bus");

	ClassDB::bind_method(D_METHOD("set_use_separate_notes_bus", "enable"), &MidiPlayer::set_use_separate_notes_bus);
	ClassDB::bind_method(D_METHOD("get_use_separate_notes_bus"), &MidiPlayer::get_use_separate_notes_bus);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::BOOL, "use_separate_notes_bus"), "set_use_separate_notes_bus", "get_use_separate_notes_bus");

	ClassDB::bind_method(D_METHOD("set_notes_audio_bus", "bus"), &MidiPlayer::set_notes_audio_bus);
	ClassDB::bind_method(D_METHOD("get_notes_audio_bus"), &MidiPlayer::get_notes_audio_bus);
	ClassDB::add_property("MidiPlayer", PropertyInfo(Variant::STRING_NAME, "notes_audio_bus"), "set_notes_audio_bus", "get_notes_audio_bus");

	ClassDB::bind_method(D_METHOD("load_soundfont", "path"), &MidiPlayer::load_soundfont);
	ClassDB::bind_method(D_METHOD("load_midi", "path"), &MidiPlayer::load_midi);

	ClassDB::bind_method(D_METHOD("play"), &MidiPlayer::play);
	ClassDB::bind_method(D_METHOD("stop"), &MidiPlayer::stop);
	ClassDB::bind_method(D_METHOD("pause"), &MidiPlayer::pause);
	ClassDB::bind_method(D_METHOD("resume"), &MidiPlayer::resume);
	ClassDB::bind_method(D_METHOD("is_playing"), &MidiPlayer::is_playing);

	ClassDB::bind_method(D_METHOD("note_on", "preset_index", "key", "velocity"), &MidiPlayer::note_on);
	ClassDB::bind_method(D_METHOD("note_off", "preset_index", "key"), &MidiPlayer::note_off);
	ClassDB::bind_method(D_METHOD("note_off_all"), &MidiPlayer::note_off_all);

	ClassDB::bind_method(D_METHOD("get_length_seconds"), &MidiPlayer::get_length_seconds);
	ClassDB::bind_method(D_METHOD("get_playback_position_seconds"), &MidiPlayer::get_playback_position_seconds);
}

void MidiPlayer::note_on(int p_preset_index, int p_key, float p_velocity) {
	// Clamp velocity to 0.0-1.0 range
	float vel = std::max(0.0f, std::min(1.0f, p_velocity));

	if (use_separate_notes_bus) {
		_ensure_notes_audio_setup();
		if (!notes_sf) {
			// Ensure we have a soundfont loaded (also fills soundfont_bytes_cache).
			if (!sf) {
				if (soundfont_resource.is_valid() && !soundfont_resource->get_data().is_empty()) {
					_load_soundfont_bytes(soundfont_resource->get_data());
				}
			}
			if (!soundfont_bytes_cache.is_empty()) {
				_load_notes_soundfont_bytes(soundfont_bytes_cache);
			}
		}
		if (!notes_sf) {
			UtilityFunctions::push_warning("MidiPlayer: note_on called but no soundfont loaded.");
			return;
		}
		tsf_note_on(notes_sf, p_preset_index, p_key, vel);
		return;
	}

	_ensure_audio_setup();
	if (!sf) {
		if (soundfont_resource.is_valid() && !soundfont_resource->get_data().is_empty()) {
			_load_soundfont_bytes(soundfont_resource->get_data());
		}
		if (!sf) {
			UtilityFunctions::push_warning("MidiPlayer: note_on called but no soundfont loaded.");
			return;
		}
	}
	tsf_note_on(sf, p_preset_index, p_key, vel);
}

void MidiPlayer::note_off(int p_preset_index, int p_key) {
	if (use_separate_notes_bus) {
		if (!notes_sf) {
			return;
		}
		tsf_note_off(notes_sf, p_preset_index, p_key);
		return;
	}
	if (!sf) {
		return;
	}
	tsf_note_off(sf, p_preset_index, p_key);
}

void MidiPlayer::note_off_all() {
	if (use_separate_notes_bus) {
		if (!notes_sf) {
			return;
		}
		tsf_note_off_all(notes_sf);
		return;
	}
	if (!sf) {
		return;
	}
	tsf_note_off_all(sf);
}

void MidiPlayer::_ready() {
	_ensure_audio_setup();
}

void MidiPlayer::_exit_tree() {
	stop();
}

void MidiPlayer::set_soundfont(const Ref<SoundFontResource> &p_resource) {
	soundfont_resource = p_resource;
	if (soundfont_resource.is_valid()) {
		const PackedByteArray bytes = soundfont_resource->get_data();
		if (!bytes.is_empty()) {
			_load_soundfont_bytes(bytes);
			if (use_separate_notes_bus && notes_player) {
				_ensure_notes_audio_setup();
				_load_notes_soundfont_bytes(soundfont_bytes_cache);
			}
		}
	}
}

Ref<SoundFontResource> MidiPlayer::get_soundfont() const {
	return soundfont_resource;
}

void MidiPlayer::set_midi(const Ref<MidiFileResource> &p_resource) {
	midi_resource = p_resource;
	if (midi_resource.is_valid()) {
		const PackedByteArray bytes = midi_resource->get_data();
		if (!bytes.is_empty()) {
			_load_midi_bytes(bytes);
		}
	}
}

Ref<MidiFileResource> MidiPlayer::get_midi() const {
	return midi_resource;
}

void MidiPlayer::set_loop(bool p_loop) {
	loop = p_loop;
}

bool MidiPlayer::get_loop() const {
	return loop;
}

void MidiPlayer::set_looping(bool p_looping) {
	loop = p_looping;
}

bool MidiPlayer::is_looping() const {
	return loop;
}

void MidiPlayer::set_midi_speed(float p_speed) {
	if (p_speed <= 0.0f) {
		p_speed = 1.0f;
	}
	midi_speed = p_speed;
}

float MidiPlayer::get_midi_speed() const {
	return midi_speed;
}

void MidiPlayer::set_volume(float p_volume) {
	volume = std::max(0.0f, p_volume);
	if (sf) {
		tsf_set_volume(sf, volume);
	}
	if (notes_sf) {
		tsf_set_volume(notes_sf, volume);
	}
}

float MidiPlayer::get_volume() const {
	return volume;
}

void MidiPlayer::set_generator_buffer_length(float p_seconds) {
	generator_buffer_length = std::max(0.05f, p_seconds);
	if (generator.is_valid()) {
		generator->set_buffer_length(generator_buffer_length);
	}
	if (notes_generator.is_valid()) {
		notes_generator->set_buffer_length(generator_buffer_length);
	}
}

float MidiPlayer::get_generator_buffer_length() const {
	return generator_buffer_length;
}

void MidiPlayer::set_audio_bus(const StringName &p_bus) {
	audio_bus = p_bus;
	if (player) {
		player->set_bus(audio_bus);
	}
	if (!use_separate_notes_bus && notes_player) {
		notes_player->set_bus(audio_bus);
	}
}

StringName MidiPlayer::get_audio_bus() const {
	return audio_bus;
}

void MidiPlayer::set_use_separate_notes_bus(bool p_enable) {
	use_separate_notes_bus = p_enable;
	if (!use_separate_notes_bus) {
		if (notes_sf) {
			tsf_note_off_all(notes_sf);
			tsf_reset(notes_sf);
		}
		if (notes_player) {
			notes_player->stop();
			notes_player->set_bus(audio_bus);
		}
		notes_playback_base.unref();
		notes_playback = nullptr;
	}
}

bool MidiPlayer::get_use_separate_notes_bus() const {
	return use_separate_notes_bus;
}

void MidiPlayer::set_notes_audio_bus(const StringName &p_bus) {
	notes_audio_bus = p_bus;
	if (notes_player) {
		notes_player->set_bus(use_separate_notes_bus ? notes_audio_bus : audio_bus);
	}
}

StringName MidiPlayer::get_notes_audio_bus() const {
	return notes_audio_bus;
}

PackedByteArray MidiPlayer::_read_all_bytes(const String &p_path) {
	PackedByteArray out;
	if (p_path.is_empty()) {
		return out;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::push_error(String("MidiPlayer: Failed to open file: ") + p_path);
		return out;
	}

	int64_t len = f->get_length();
	if (len <= 0) {
		return out;
	}

	out.resize(len);
	f->get_buffer(out.ptrw(), len);
	return out;
}

bool MidiPlayer::_load_soundfont_bytes(const PackedByteArray &p_bytes) {
	if (p_bytes.is_empty()) {
		UtilityFunctions::push_error("MidiPlayer: SoundFont bytes are empty.");
		return false;
	}

	soundfont_bytes_cache = p_bytes;

	if (sf) {
		tsf_close(sf);
		sf = nullptr;
	}
	if (notes_sf) {
		tsf_close(notes_sf);
		notes_sf = nullptr;
	}

	sf = tsf_load_memory(soundfont_bytes_cache.ptr(), (int)soundfont_bytes_cache.size());
	if (!sf) {
		UtilityFunctions::push_error("MidiPlayer: tsf_load_memory() failed.");
		return false;
	}

	sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	tsf_set_output(sf, TSF_STEREO_INTERLEAVED, sample_rate, 0.0f);
	tsf_set_max_voices(sf, 256);
	tsf_set_volume(sf, volume);

	// Initialize channels so channel allocation won't happen during playback.
	for (int ch = 0; ch < 16; ch++) {
		// Default program 0, drums on channel 9.
		tsf_channel_set_presetnumber(sf, ch, 0, ch == 9);
		// Set center pan + full volume in TSF's MIDI controller space.
		tsf_channel_midi_control(sf, ch, (int)TML_PAN_MSB, 64);
		tsf_channel_midi_control(sf, ch, (int)TML_VOLUME_MSB, 127);
	}

	return true;
}

bool MidiPlayer::_load_notes_soundfont_bytes(const PackedByteArray &p_bytes) {
	if (p_bytes.is_empty()) {
		return false;
	}

	if (notes_sf) {
		tsf_close(notes_sf);
		notes_sf = nullptr;
	}

	notes_sf = tsf_load_memory(p_bytes.ptr(), (int)p_bytes.size());
	if (!notes_sf) {
		UtilityFunctions::push_error("MidiPlayer: notes tsf_load_memory() failed.");
		return false;
	}

	sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	tsf_set_output(notes_sf, TSF_STEREO_INTERLEAVED, sample_rate, 0.0f);
	tsf_set_max_voices(notes_sf, 256);
	tsf_set_volume(notes_sf, volume);

	for (int ch = 0; ch < 16; ch++) {
		tsf_channel_set_presetnumber(notes_sf, ch, 0, ch == 9);
		tsf_channel_midi_control(notes_sf, ch, (int)TML_PAN_MSB, 64);
		tsf_channel_midi_control(notes_sf, ch, (int)TML_VOLUME_MSB, 127);
	}

	return true;
}

bool MidiPlayer::_load_midi_bytes(const PackedByteArray &p_bytes) {
	if (p_bytes.is_empty()) {
		UtilityFunctions::push_error("MidiPlayer: MIDI bytes are empty.");
		return false;
	}

	if (midi) {
		tml_free(midi);
		midi = nullptr;
	}

	midi = tml_load_memory(p_bytes.ptr(), (int)p_bytes.size());
	if (!midi) {
		UtilityFunctions::push_error("MidiPlayer: tml_load_memory() failed.");
		return false;
	}

	unsigned int first_note_ms = 0;
	unsigned int length_ms = 0;
	tml_get_info(midi, nullptr, nullptr, nullptr, &first_note_ms, &length_ms);
	midi_length_ms = (uint32_t)length_ms;

	return true;
}

bool MidiPlayer::load_soundfont(const String &p_path) {
	PackedByteArray bytes = _read_all_bytes(p_path);
	return _load_soundfont_bytes(bytes);
}

bool MidiPlayer::load_midi(const String &p_path) {
	PackedByteArray bytes = _read_all_bytes(p_path);
	return _load_midi_bytes(bytes);
}

void MidiPlayer::_ensure_audio_setup() {
	if (!player) {
		player = memnew(AudioStreamPlayer);
		player->set_name("_MidiPlayerAudio");
		add_child(player);
		// Set bus after adding to tree to ensure it takes effect
		player->set_bus(audio_bus);
	}

	sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	if (!generator.is_valid()) {
		generator.instantiate();
		generator->set_mix_rate(sample_rate);
		generator->set_buffer_length(generator_buffer_length);
		player->set_stream(generator);
	}

	if (!player->is_playing()) {
		player->play();
	}

	playback_base = player->get_stream_playback();
	playback = Object::cast_to<AudioStreamGeneratorPlayback>(playback_base.ptr());
	if (!playback) {
		// If this ever happens, we can't output audio.
		UtilityFunctions::push_warning("MidiPlayer: AudioStreamGeneratorPlayback not available yet.");
	}
}

void MidiPlayer::_ensure_notes_audio_setup() {
	if (!notes_player) {
		notes_player = memnew(AudioStreamPlayer);
		notes_player->set_name("_MidiPlayerNotesAudio");
		add_child(notes_player);
		// Set bus after adding to tree
		notes_player->set_bus(use_separate_notes_bus ? notes_audio_bus : audio_bus);
	}

	sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	if (!notes_generator.is_valid()) {
		notes_generator.instantiate();
		notes_generator->set_mix_rate(sample_rate);
		notes_generator->set_buffer_length(generator_buffer_length);
		notes_player->set_stream(notes_generator);
	}

	if (!notes_player->is_playing()) {
		notes_player->play();
	}

	notes_playback_base = notes_player->get_stream_playback();
	notes_playback = Object::cast_to<AudioStreamGeneratorPlayback>(notes_playback_base.ptr());
	if (!notes_playback) {
		UtilityFunctions::push_warning("MidiPlayer: Notes AudioStreamGeneratorPlayback not available yet.");
	}
}

void MidiPlayer::_clear_audio_buffer() {
	if (!playback) {
		return;
	}
	// There is no explicit clear API; pushing nothing lets it drain.
	// We force a stop/play cycle to effectively reset the buffer.
	if (player) {
		player->stop();
		player->play();
		playback_base = player->get_stream_playback();
		playback = Object::cast_to<AudioStreamGeneratorPlayback>(playback_base.ptr());
	}
}

void MidiPlayer::_clear_notes_audio_buffer() {
	if (!notes_playback) {
		return;
	}
	if (notes_player) {
		notes_player->stop();
		notes_player->play();
		notes_playback_base = notes_player->get_stream_playback();
		notes_playback = Object::cast_to<AudioStreamGeneratorPlayback>(notes_playback_base.ptr());
	}
}

void MidiPlayer::_reset_synth() {
	if (!sf) {
		return;
	}
	tsf_reset(sf);
	// Re-apply volume since reset may clear channels.
	tsf_set_output(sf, TSF_STEREO_INTERLEAVED, sample_rate, 0.0f);
	tsf_set_max_voices(sf, 256);
	tsf_set_volume(sf, volume);
	for (int ch = 0; ch < 16; ch++) {
		tsf_channel_set_presetnumber(sf, ch, 0, ch == 9);
		tsf_channel_midi_control(sf, ch, (int)TML_PAN_MSB, 64);
		tsf_channel_midi_control(sf, ch, (int)TML_VOLUME_MSB, 127);
	}
}

void MidiPlayer::_reset_notes_synth() {
	if (!notes_sf) {
		return;
	}
	tsf_reset(notes_sf);
	tsf_set_output(notes_sf, TSF_STEREO_INTERLEAVED, sample_rate, 0.0f);
	tsf_set_max_voices(notes_sf, 256);
	tsf_set_volume(notes_sf, volume);
	for (int ch = 0; ch < 16; ch++) {
		tsf_channel_set_presetnumber(notes_sf, ch, 0, ch == 9);
		tsf_channel_midi_control(notes_sf, ch, (int)TML_PAN_MSB, 64);
		tsf_channel_midi_control(notes_sf, ch, (int)TML_VOLUME_MSB, 127);
	}
}

void MidiPlayer::play() {
	_ensure_audio_setup();

	if (!sf) {
		if (soundfont_resource.is_valid() && !soundfont_resource->get_data().is_empty()) {
			_load_soundfont_bytes(soundfont_resource->get_data());
		}
	}
	if (!midi) {
		if (midi_resource.is_valid() && !midi_resource->get_data().is_empty()) {
			_load_midi_bytes(midi_resource->get_data());
		}
	}

	if (!sf || !midi) {
		UtilityFunctions::push_error("MidiPlayer: Cannot play (missing soundfont or midi). Call load_soundfont() and load_midi() first.");
		return;
	}

	_reset_synth();
	_clear_audio_buffer();

	event_cursor = midi;
	synth_time_sec = 0.0;
	playing = true;
	paused = false;

	if (player && !player->is_playing()) {
		player->play();
	}
}

void MidiPlayer::stop() {
	playing = false;
	paused = false;
	synth_time_sec = 0.0;
	notes_time_sec = 0.0;
	event_cursor = midi;

	if (sf) {
		tsf_note_off_all(sf);
		tsf_reset(sf);
	}
	if (notes_sf) {
		tsf_note_off_all(notes_sf);
		tsf_reset(notes_sf);
	}
	if (player) {
		player->stop();
	}
	if (notes_player) {
		notes_player->stop();
	}
	playback_base.unref();
	playback = nullptr;
	notes_playback_base.unref();
	notes_playback = nullptr;
}

void MidiPlayer::pause() {
	if (!playing) {
		return;
	}
	paused = true;
	if (player) {
		player->stop();
	}
}

void MidiPlayer::resume() {
	if (!playing) {
		return;
	}
	paused = false;
	_ensure_audio_setup();
	if (player && !player->is_playing()) {
		player->play();
	}
}

bool MidiPlayer::is_playing() const {
	return playing && !paused;
}

float MidiPlayer::get_length_seconds() const {
	return (float)midi_length_ms / 1000.0f;
}

float MidiPlayer::get_playback_position_seconds() const {
	return (float)synth_time_sec;
}

void MidiPlayer::_apply_event(const tml_message *p_msg) {
	if (!sf || !p_msg) {
		return;
	}

	switch (p_msg->type) {
		case TML_NOTE_ON: {
			const float vel = (float)(uint8_t)p_msg->velocity / 127.0f;
			tsf_channel_note_on(sf, p_msg->channel, p_msg->key, vel);
		} break;
		case TML_NOTE_OFF: {
			tsf_channel_note_off(sf, p_msg->channel, p_msg->key);
		} break;
		case TML_CONTROL_CHANGE: {
			tsf_channel_midi_control(sf, p_msg->channel, (int)(uint8_t)p_msg->control, (int)(uint8_t)p_msg->control_value);
		} break;
		case TML_PROGRAM_CHANGE: {
			tsf_channel_set_presetnumber(sf, p_msg->channel, (int)(uint8_t)p_msg->program, p_msg->channel == 9);
		} break;
		case TML_PITCH_BEND: {
			tsf_channel_set_pitchwheel(sf, p_msg->channel, (int)p_msg->pitch_bend);
		} break;
		case TML_CHANNEL_PRESSURE:
		case TML_KEY_PRESSURE:
			// Not directly supported by TSF channel API.
			break;
		default:
			// Includes tempo meta messages and EOT. Times are already baked into Msg->time.
			break;
	}
}

void MidiPlayer::_process_events_until_ms(uint32_t p_time_ms) {
	while (event_cursor && event_cursor->time <= p_time_ms) {
		_apply_event(event_cursor);
		event_cursor = event_cursor->next;

		if (!event_cursor) {
			break;
		}
	}
}

void MidiPlayer::_pump_audio(bool p_process_events) {
	if (!sf || !playback) {
		return;
	}

	int frames_available = playback->get_frames_available();
	if (frames_available <= 0) {
		return;
	}

	std::vector<float> interleaved;
	interleaved.resize((size_t)k_block_frames * 2);

	while (frames_available > 0) {
		const int frames = std::min(frames_available, k_block_frames);
		const double block_end_sec = synth_time_sec + (double)frames / (double)sample_rate;
		if (p_process_events) {
			// Apply midi_speed to convert real time to MIDI time
			const uint32_t block_end_ms = (uint32_t)(block_end_sec * 1000.0 * midi_speed);
			_process_events_until_ms(block_end_ms);
		}

		if ((int)interleaved.size() < frames * 2) {
			interleaved.resize((size_t)frames * 2);
		}

		tsf_render_float(sf, interleaved.data(), frames, 0);

		PackedVector2Array buf;
		buf.resize(frames);
		for (int i = 0; i < frames; i++) {
			const float l = interleaved[i * 2 + 0];
			const float r = interleaved[i * 2 + 1];
			buf.set(i, Vector2(l, r));
		}

		playback->push_buffer(buf);

		synth_time_sec = block_end_sec;
		frames_available -= frames;

		// If we're past the MIDI length and there are no active voices, stop/loop.
		if (p_process_events && !event_cursor) {
			if (tsf_active_voice_count(sf) == 0) {
				if (loop) {
					play();
				}
				break;
			}
		}
	}
}

void MidiPlayer::_pump_notes_audio() {
	if (!notes_sf || !notes_playback) {
		return;
	}

	int frames_available = notes_playback->get_frames_available();
	if (frames_available <= 0) {
		return;
	}

	std::vector<float> interleaved;
	interleaved.resize((size_t)k_block_frames * 2);

	while (frames_available > 0) {
		const int frames = std::min(frames_available, k_block_frames);
		const double block_end_sec = notes_time_sec + (double)frames / (double)sample_rate;

		if ((int)interleaved.size() < frames * 2) {
			interleaved.resize((size_t)frames * 2);
		}

		tsf_render_float(notes_sf, interleaved.data(), frames, 0);

		PackedVector2Array buf;
		buf.resize(frames);
		for (int i = 0; i < frames; i++) {
			const float l = interleaved[i * 2 + 0];
			const float r = interleaved[i * 2 + 1];
			buf.set(i, Vector2(l, r));
		}

		notes_playback->push_buffer(buf);
		notes_time_sec = block_end_sec;
		frames_available -= frames;

		if (tsf_active_voice_count(notes_sf) == 0) {
			break;
		}
	}
}

void MidiPlayer::_process(double p_delta) {
	(void)p_delta;
	if (playing && !paused) {
		_ensure_audio_setup();
		_pump_audio(true);

		// Auto-stop when finished (non-loop).
		if (!loop && !event_cursor && sf && tsf_active_voice_count(sf) == 0) {
			stop();
		}
	} else {
		// Not playing (or paused): still render any manual notes on the main synth when not using a separate notes bus.
		if (!use_separate_notes_bus && sf && tsf_active_voice_count(sf) > 0) {
			_ensure_audio_setup();
			_pump_audio(false);
		}
	}

	// Separate notes bus output.
	if (use_separate_notes_bus && notes_sf && tsf_active_voice_count(notes_sf) > 0) {
		_ensure_notes_audio_setup();
		_pump_notes_audio();
	}
}

} // namespace godot
