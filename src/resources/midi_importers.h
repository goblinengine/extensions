#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class MidiImporter : public EditorImportPlugin {
	GDCLASS(MidiImporter, EditorImportPlugin)

public:
	String _get_importer_name() const override;
	String _get_visible_name() const override;
	PackedStringArray _get_recognized_extensions() const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	float _get_priority() const override;
	int _get_import_order() const override;
	int _get_preset_count() const override;
	String _get_preset_name(int p_preset_index) const override;
	TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;

protected:
	static void _bind_methods();
};

class SoundFontImporter : public EditorImportPlugin {
	GDCLASS(SoundFontImporter, EditorImportPlugin)

public:
	String _get_importer_name() const override;
	String _get_visible_name() const override;
	PackedStringArray _get_recognized_extensions() const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	float _get_priority() const override;
	int _get_import_order() const override;
	int _get_preset_count() const override;
	String _get_preset_name(int p_preset_index) const override;
	TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;

protected:
	static void _bind_methods();
};

} // namespace godot
