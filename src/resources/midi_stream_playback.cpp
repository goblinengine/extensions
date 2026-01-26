#include "resources/midi_stream_playback.h"

// TinySoundFont + TinyMidiLoader implementation is compiled into this TU.
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#define TML_IMPLEMENTATION
#define TML_NO_STDIO

#include "tsf.h"
#include "tml.h"

#include <algorithm>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

MidiStreamPlayback::MidiStreamPlayback() {
}

MidiStreamPlayback::~MidiStreamPlayback() {
	_stop();
	if (midi) {
		tml_free(midi);
		midi = nullptr;
	}
	if (sf) {
		tsf_close(sf);
		sf = nullptr;
	}
}

void MidiStreamPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &MidiStreamPlayback::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &MidiStreamPlayback::get_stream);

	ClassDB::bind_method(D_METHOD("note_on", "preset_index", "key", "velocity"), &MidiStreamPlayback::note_on);
	ClassDB::bind_method(D_METHOD("note_off", "preset_index", "key"), &MidiStreamPlayback::note_off);
	ClassDB::bind_method(D_METHOD("note_off_all"), &MidiStreamPlayback::note_off_all);
}

void MidiStreamPlayback::set_stream(const Ref<MidiStream> &p_stream) {
	stream = p_stream;
}

Ref<MidiStream> MidiStreamPlayback::get_stream() const {
	return stream;
}

void MidiStreamPlayback::_ensure_loaded() {
	if (!stream.is_valid()) {
		return;
	}

	sample_rate = (int)AudioServer::get_singleton()->get_mix_rate();
	if (sample_rate <= 0) {
		sample_rate = 44100;
	}

	if (!sf) {
		Ref<SoundFontResource> sf_res = stream->get_soundfont();
		if (sf_res.is_valid() && !sf_res->get_data().is_empty()) {
			const PackedByteArray bytes = sf_res->get_data();
			sf = tsf_load_memory(bytes.ptr(), (int)bytes.size());
			if (!sf) {
				UtilityFunctions::push_error("MidiStreamPlayback: tsf_load_memory() failed.");
			}
		}
		if (sf) {
			tsf_set_output(sf, TSF_STEREO_INTERLEAVED, sample_rate, 0.0f);
			tsf_set_max_voices(sf, 256);
			tsf_set_volume(sf, 1.0f);
			_reset_synth();
		}
	}

	if (!midi) {
		Ref<MidiFileResource> midi_res = stream->get_midi();
		if (midi_res.is_valid() && !midi_res->get_data().is_empty()) {
			const PackedByteArray bytes = midi_res->get_data();
			midi = tml_load_memory(bytes.ptr(), (int)bytes.size());
			if (!midi) {
				UtilityFunctions::push_error("MidiStreamPlayback: tml_load_memory() failed.");
			} else {
				unsigned int first_note_ms = 0;
				unsigned int length_ms = 0;
				tml_get_info(midi, nullptr, nullptr, nullptr, &first_note_ms, &length_ms);
				midi_length_ms = (uint32_t)length_ms;
			}
		}
	}
}

void MidiStreamPlayback::_reset_synth() {
	if (!sf) {
		return;
	}

	tsf_reset(sf);
	// Channel defaults.
	for (int ch = 0; ch < 16; ch++) {
		tsf_channel_set_presetnumber(sf, ch, 0, ch == 9);
		tsf_channel_midi_control(sf, ch, (int)TML_PAN_MSB, 64);
		tsf_channel_midi_control(sf, ch, (int)TML_VOLUME_MSB, 127);
	}
}

void MidiStreamPlayback::_apply_event(const tml_message *p_msg) {
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
			break;
		default:
			break;
	}
}

void MidiStreamPlayback::_process_events_until_ms(uint32_t p_time_ms) {
	while (event_cursor && event_cursor->time <= p_time_ms) {
		_apply_event(event_cursor);
		event_cursor = event_cursor->next;
		if (!event_cursor) {
			break;
		}
	}
}

void MidiStreamPlayback::_seek_internal(double p_position_sec) {
	_ensure_loaded();
	if (!sf || !midi) {
		position_sec = 0.0;
		event_cursor = midi;
		return;
	}

	_reset_synth();
	event_cursor = midi;
	position_sec = std::max(0.0, p_position_sec);

	const float midi_speed = stream.is_valid() ? stream->get_midi_speed() : 1.0f;
	const uint32_t target_ms = (uint32_t)(position_sec * 1000.0 * midi_speed);
	_process_events_until_ms(target_ms);
}

void MidiStreamPlayback::_start(double p_from_pos) {
	begin_resample();
	playing = true;
	_seek_internal(p_from_pos);
}

void MidiStreamPlayback::_stop() {
	playing = false;
	position_sec = 0.0;
	loop_count = 0;
	if (sf) {
		tsf_note_off_all(sf);
		_reset_synth();
	}
	event_cursor = midi;
}

bool MidiStreamPlayback::_is_playing() const {
	return playing;
}

int32_t MidiStreamPlayback::_get_loop_count() const {
	return loop_count;
}

double MidiStreamPlayback::_get_playback_position() const {
	return position_sec;
}

void MidiStreamPlayback::_seek(double p_position) {
	_seek_internal(p_position);
}

int32_t MidiStreamPlayback::_mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) {
	if (!playing) {
		return 0;
	}
	_ensure_loaded();
	if (!sf) {
		return 0;
	}

	if ((int)interleaved.size() < p_frame_count * 2) {
		interleaved.resize((size_t)p_frame_count * 2);
	}

	// Apply MIDI events up to the end of this block.
	if (stream.is_valid() && midi) {
		const float midi_speed = stream->get_midi_speed();
		const double block_end_sec = position_sec + (double)p_frame_count / (double)sample_rate;
		const uint32_t block_end_ms = (uint32_t)(block_end_sec * 1000.0 * midi_speed);
		_process_events_until_ms(block_end_ms);
	}

	tsf_render_float(sf, interleaved.data(), p_frame_count, 0);
	for (int i = 0; i < p_frame_count; i++) {
		p_dst_buffer[i].left = interleaved[i * 2 + 0];
		p_dst_buffer[i].right = interleaved[i * 2 + 1];
	}

	position_sec += (double)p_frame_count / (double)sample_rate;

	// Stop/loop when done.
	if (stream.is_valid() && midi && !event_cursor) {
		if (tsf_active_voice_count(sf) == 0) {
			if (stream->get_loop()) {
				loop_count++;
				_seek_internal(0.0);
			} else {
				playing = false;
			}
		}
	}

	return p_frame_count;
}

float MidiStreamPlayback::_get_stream_sampling_rate() const {
	return (float)sample_rate;
}

void MidiStreamPlayback::note_on(int32_t p_preset_index, int32_t p_key, float p_velocity) {
	_ensure_loaded();
	if (!sf) {
		UtilityFunctions::push_warning("MidiStreamPlayback: note_on called but no soundfont loaded.");
		return;
	}
	float vel = std::max(0.0f, std::min(1.0f, p_velocity));
	tsf_note_on(sf, p_preset_index, p_key, vel);
}

void MidiStreamPlayback::note_off(int32_t p_preset_index, int32_t p_key) {
	if (!sf) {
		return;
	}
	tsf_note_off(sf, p_preset_index, p_key);
}

void MidiStreamPlayback::note_off_all() {
	if (!sf) {
		return;
	}
	tsf_note_off_all(sf);
}

} // namespace godot
