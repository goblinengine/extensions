#include "midi_importers.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "midi_resources.h"

namespace godot {

void MidiImporter::_bind_methods() {
}

String MidiImporter::_get_importer_name() const {
	return "midi_stream.mid";
}

String MidiImporter::_get_visible_name() const {
	return "MIDI File (MidiStream)";
}

PackedStringArray MidiImporter::_get_recognized_extensions() const {
	PackedStringArray exts;
	exts.push_back("mid");
	exts.push_back("midi");
	return exts;
}

String MidiImporter::_get_save_extension() const {
	return "res";
}

String MidiImporter::_get_resource_type() const {
	return "MidiFileResource";
}

float MidiImporter::_get_priority() const {
	return 1.0f;
}

int MidiImporter::_get_import_order() const {
	return 0;
}

int MidiImporter::_get_preset_count() const {
	return 1;
}

String MidiImporter::_get_preset_name(int p_preset_index) const {
	return "Default";
}

TypedArray<Dictionary> MidiImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	return TypedArray<Dictionary>();
}

Error MidiImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
		const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	if (bytes.is_empty()) {
		UtilityFunctions::push_error("MidiStream importer: failed to read bytes: " + p_source_file);
		return ERR_CANT_OPEN;
	}

	Ref<MidiFileResource> res = memnew(MidiFileResource);
	res->set_data(bytes);

	String out_path = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(res, out_path);
}

void SoundFontImporter::_bind_methods() {
}

String SoundFontImporter::_get_importer_name() const {
	return "midi_stream.sf2";
}

String SoundFontImporter::_get_visible_name() const {
	return "SoundFont 2 (MidiStream)";
}

PackedStringArray SoundFontImporter::_get_recognized_extensions() const {
	PackedStringArray exts;
	exts.push_back("sf2");
	return exts;
}

String SoundFontImporter::_get_save_extension() const {
	return "res";
}

String SoundFontImporter::_get_resource_type() const {
	return "SoundFontResource";
}

float SoundFontImporter::_get_priority() const {
	return 1.0f;
}

int SoundFontImporter::_get_import_order() const {
	return 0;
}

int SoundFontImporter::_get_preset_count() const {
	return 1;
}

String SoundFontImporter::_get_preset_name(int p_preset_index) const {
	return "Default";
}

TypedArray<Dictionary> SoundFontImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	return TypedArray<Dictionary>();
}

Error SoundFontImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
		const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	if (bytes.is_empty()) {
		UtilityFunctions::push_error("MidiStream importer: failed to read bytes: " + p_source_file);
		return ERR_CANT_OPEN;
	}

	Ref<SoundFontResource> res = memnew(SoundFontResource);
	res->set_data(bytes);

	String out_path = p_save_path + String(".") + _get_save_extension();
	return ResourceSaver::get_singleton()->save(res, out_path);
}

} // namespace godot
