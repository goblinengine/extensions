#include "midi_resources.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void MidiFileResource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &MidiFileResource::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &MidiFileResource::get_data);
	ClassDB::add_property("MidiFileResource", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), "set_data", "get_data");
}

void MidiFileResource::set_data(const PackedByteArray &p_data) {
	data = p_data;
}

PackedByteArray MidiFileResource::get_data() const {
	return data;
}

void SoundFontResource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &SoundFontResource::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &SoundFontResource::get_data);
	ClassDB::add_property("SoundFontResource", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), "set_data", "get_data");
}

void SoundFontResource::set_data(const PackedByteArray &p_data) {
	data = p_data;
}

PackedByteArray SoundFontResource::get_data() const {
	return data;
}

} // namespace godot
