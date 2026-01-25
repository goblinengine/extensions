#include "register_types.h"

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>

#include "main/custom_tree.h"

#include "2d/midi_player.h"
#include "resources/midi_resources.h"
#include "resources/midi_importers.h"
#include "gui/midi_editor_plugin.h"

#include "3d/lightmap_baker/lightmap_baker.h"

#include "3d/compound_mesh_instance_3d.h"
#include "3d/compound_part_proxy.h"
#include "3d/compound_part_node.h"

namespace godot {

void initialize_extensions_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<CustomTree>();
		ClassDB::register_class<MidiFileResource>();
		ClassDB::register_class<SoundFontResource>();
		ClassDB::register_class<MidiPlayer>();
		ClassDB::register_class<LightmapBaker>();
		ClassDB::register_class<CompoundMeshInstance3D>();
		ClassDB::register_class<CompoundPartProxy>();
		ClassDB::register_class<CompoundPartNode3D>();
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
