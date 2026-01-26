#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "resources/midi_stream.h"

struct tsf;
struct tml_message;

namespace godot {

class MidiStreamPlayback : public AudioStreamPlaybackResampled {
	GDCLASS(MidiStreamPlayback, AudioStreamPlaybackResampled)

public:
	MidiStreamPlayback();
	~MidiStreamPlayback();

	void set_stream(const Ref<MidiStream> &p_stream);
	Ref<MidiStream> get_stream() const;

	// Manual note preview.
	void note_on(int32_t p_preset_index, int32_t p_key, float p_velocity);
	void note_off(int32_t p_preset_index, int32_t p_key);
	void note_off_all();

	// AudioStreamPlayback overrides.
	void _start(double p_from_pos) override;
	void _stop() override;
	bool _is_playing() const override;
	int32_t _get_loop_count() const override;
	double _get_playback_position() const override;
	void _seek(double p_position) override;
	int32_t _mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) override;
	float _get_stream_sampling_rate() const override;

protected:
	static void _bind_methods();

private:
	void _ensure_loaded();
	void _reset_synth();
	void _apply_event(const tml_message *p_msg);
	void _process_events_until_ms(uint32_t p_time_ms);
	void _seek_internal(double p_position_sec);

	Ref<MidiStream> stream;

	int sample_rate = 44100;
	bool playing = false;
	int32_t loop_count = 0;
	double position_sec = 0.0;

	tsf *sf = nullptr;
	tml_message *midi = nullptr;
	tml_message *event_cursor = nullptr;
	uint32_t midi_length_ms = 0;

	std::vector<float> interleaved;
};

} // namespace godot
