#pragma once

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <vector>

namespace godot {

// Drop-in compatible SceneTree replacement for projects that want a distinct MainLoop type.
//
// Important: We do NOT override MainLoop virtuals here.
// That ensures Godot's built-in SceneTree behavior remains identical.
class CustomTree : public SceneTree {
	GDCLASS(CustomTree, SceneTree);

	struct Channel {
		StringName name;
		StringName method;
		double interval_sec = 0.0;
		double accumulator_sec = 0.0;
		bool run_in_physics = false;
		bool ignore_time_scale = false;
		bool enabled = false;
		std::vector<Callable> targets;
	};

	std::vector<Channel> channels;
	bool registry_built = false;
	uint64_t last_process_usec = 0;
	uint64_t last_physics_usec = 0;
	static bool settings_registered;

	void _on_process_frame();
	void _on_physics_frame();
	void _on_node_added(Node *p_node);
	void _on_node_removed(Node *p_node);
	void _tick_channels(bool p_physics_frame);
	void _ensure_project_settings();
	void _reload_from_project_settings();
	void _rebuild_registry();
	void _register_node(Node *p_node);
	void _unregister_node(Node *p_node);
	void _connect_frame_signals();
	static double _u64_usec_to_sec(uint64_t p_usec);

protected:
	static void _bind_methods();

public:
	CustomTree();
	~CustomTree();

	// Reload channel configuration and rebuild the callable registry.
	void reload_custom_processes_from_project_settings();
};

} // namespace godot
