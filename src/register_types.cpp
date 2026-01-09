#include "register_types.h"

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>

#include "midi_player/midi_player.h"
#include "midi_player/midi_resources.h"
#include "midi_player/midi_importers.h"
#include "midi_player/midi_editor_plugin.h"

#include "lightmap_baker/lightmap_baker.h"

namespace godot {

void initialize_extensions_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<MidiFileResource>();
		ClassDB::register_class<SoundFontResource>();
		ClassDB::register_class<MidiPlayer>();
		ClassDB::register_class<LightmapBaker>();
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<MidiImporter>();
		ClassDB::register_class<SoundFontImporter>();
		ClassDB::register_class<MidiEditorPlugin>();
		EditorPlugins::add_by_type<MidiEditorPlugin>();
	}
}

void uninitialize_extensions_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<MidiEditorPlugin>();
	}
}

} // namespace godot

// C linkage entry point - signature matches GDExtension requirements
extern "C" {
uint32_t GDE_EXPORT extensions_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *p_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, p_initialization);
	init_obj.register_initializer(godot::initialize_extensions_module);
	init_obj.register_terminator(godot::uninitialize_extensions_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
