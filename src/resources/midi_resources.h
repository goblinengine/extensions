#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class MidiFileResource : public Resource {
	GDCLASS(MidiFileResource, Resource)

public:
	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;

protected:
	static void _bind_methods();

private:
	PackedByteArray data;
};

class SoundFontResource : public Resource {
	GDCLASS(SoundFontResource, Resource)

public:
	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;

protected:
	static void _bind_methods();

private:
	PackedByteArray data;
};

} // namespace godot
