#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void initialize_extensions_module(godot::ModuleInitializationLevel p_level);
void uninitialize_extensions_module(godot::ModuleInitializationLevel p_level);

} // namespace godot
