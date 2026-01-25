#include "midi_editor_plugin.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void MidiEditorPlugin::_bind_methods() {
}

void MidiEditorPlugin::_enter_tree() {
	midi_importer = memnew(MidiImporter);
	add_import_plugin(midi_importer);

	soundfont_importer = memnew(SoundFontImporter);
	add_import_plugin(soundfont_importer);
}

void MidiEditorPlugin::_exit_tree() {
	if (midi_importer) {
		remove_import_plugin(midi_importer);
		memdelete(midi_importer);
		midi_importer = nullptr;
	}
	if (soundfont_importer) {
		remove_import_plugin(soundfont_importer);
		memdelete(soundfont_importer);
		soundfont_importer = nullptr;
	}
}

} // namespace godot
