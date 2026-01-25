#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include "resources/midi_importers.h"

namespace godot {

class MidiEditorPlugin : public EditorPlugin {
	GDCLASS(MidiEditorPlugin, EditorPlugin)

public:
	void _enter_tree() override;
	void _exit_tree() override;

protected:
	static void _bind_methods();

private:
	MidiImporter *midi_importer = nullptr;
	SoundFontImporter *soundfont_importer = nullptr;
};

} // namespace godot
