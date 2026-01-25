#include "main/custom_tree.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/global_constants.hpp>

namespace godot {

bool CustomTree::settings_registered = false;

void CustomTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_custom_processes_from_project_settings"), &CustomTree::reload_custom_processes_from_project_settings);
}

double CustomTree::_u64_usec_to_sec(uint64_t p_usec) {
	return double(p_usec) / 1000000.0;
}

void CustomTree::_connect_frame_signals() {
	// Safe to call multiple times; Godot will reject duplicate connects.
	Callable process_cb = callable_mp(this, &CustomTree::_on_process_frame);
	Callable physics_cb = callable_mp(this, &CustomTree::_on_physics_frame);
	Callable node_added_cb = callable_mp(this, &CustomTree::_on_node_added);
	Callable node_removed_cb = callable_mp(this, &CustomTree::_on_node_removed);

	Error err = connect(StringName("process_frame"), process_cb);
	if (err != OK && err != ERR_INVALID_PARAMETER) {
		UtilityFunctions::printerr(String("CustomTree: failed to connect process_frame (err=") + itos(err) + ")");
	}
	err = connect(StringName("physics_frame"), physics_cb);
	if (err != OK && err != ERR_INVALID_PARAMETER) {
		UtilityFunctions::printerr(String("CustomTree: failed to connect physics_frame (err=") + itos(err) + ")");
	}
	err = connect(StringName("node_added"), node_added_cb);
	if (err != OK && err != ERR_INVALID_PARAMETER) {
		UtilityFunctions::printerr(String("CustomTree: failed to connect node_added (err=") + itos(err) + ")");
	}
	err = connect(StringName("node_removed"), node_removed_cb);
	if (err != OK && err != ERR_INVALID_PARAMETER) {
		UtilityFunctions::printerr(String("CustomTree: failed to connect node_removed (err=") + itos(err) + ")");
	}
}

void CustomTree::_ensure_project_settings() {
	if (settings_registered) {
		return;
	}
	settings_registered = true;

	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps) {
		return;
	}

	// Avoid re-registering property info (can happen on extension reloads).
	const StringName meta_key("custom_tree_property_info_registered");
	if (ps->has_meta(meta_key) && bool(ps->get_meta(meta_key, false))) {
		return;
	}
	ps->set_meta(meta_key, true);

	// Flat-ish settings, no Array/Dictionary config.
	// Section will appear as: [custom_tree] tick/..., anim/..., effect/..., low/..., decay/...
	struct SettingDef {
		const char *name;
		Variant::Type type;
		Variant def;
	};

	const SettingDef defs[] = {
		{ "custom_tree/tick/enabled", Variant::BOOL, true },
		{ "custom_tree/tick/method", Variant::STRING, String("_tick_process") },
		{ "custom_tree/tick/interval_sec", Variant::FLOAT, 0.1 },
		{ "custom_tree/tick/run_in_physics", Variant::BOOL, false },
		{ "custom_tree/tick/ignore_time_scale", Variant::BOOL, false },

		{ "custom_tree/anim/enabled", Variant::BOOL, true },
		{ "custom_tree/anim/method", Variant::STRING, String("_anim_process") },
		{ "custom_tree/anim/interval_sec", Variant::FLOAT, 0.5 },
		{ "custom_tree/anim/run_in_physics", Variant::BOOL, false },
		{ "custom_tree/anim/ignore_time_scale", Variant::BOOL, true },

		{ "custom_tree/effect/enabled", Variant::BOOL, true },
		{ "custom_tree/effect/method", Variant::STRING, String("_effect_process") },
		{ "custom_tree/effect/interval_sec", Variant::FLOAT, 1.0 },
		{ "custom_tree/effect/run_in_physics", Variant::BOOL, false },
		{ "custom_tree/effect/ignore_time_scale", Variant::BOOL, false },

		{ "custom_tree/low/enabled", Variant::BOOL, true },
		{ "custom_tree/low/method", Variant::STRING, String("_low_process") },
		{ "custom_tree/low/interval_sec", Variant::FLOAT, 2.5 },
		{ "custom_tree/low/run_in_physics", Variant::BOOL, false },
		{ "custom_tree/low/ignore_time_scale", Variant::BOOL, false },

		{ "custom_tree/decay/enabled", Variant::BOOL, true },
		{ "custom_tree/decay/method", Variant::STRING, String("_decay_process") },
		{ "custom_tree/decay/interval_sec", Variant::FLOAT, 5.0 },
		{ "custom_tree/decay/run_in_physics", Variant::BOOL, false },
		{ "custom_tree/decay/ignore_time_scale", Variant::BOOL, false },
	};

	for (const SettingDef &d : defs) {
		Dictionary info;
		info["name"] = d.name;
		info["type"] = int(d.type);
		info["hint"] = int(PROPERTY_HINT_NONE);
		info["hint_string"] = String();
		ps->add_property_info(info);
		if (!ps->has_setting(d.name)) {
			ps->set_setting(d.name, d.def);
		}
	}
}

void CustomTree::_reload_from_project_settings() {
	_ensure_project_settings();
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps) {
		return;
	}

	channels.clear();
	channels.reserve(5);

	auto load_channel = [&](const StringName &p_name) {
		Channel c;
		c.name = p_name;
		const String prefix = String("custom_tree/") + String(p_name) + String("/");
		c.enabled = bool(ps->get_setting(prefix + String("enabled"), false));
		c.method = StringName(ps->get_setting(prefix + String("method"), String()));
		c.interval_sec = double(ps->get_setting(prefix + String("interval_sec"), 0.0));
		c.run_in_physics = bool(ps->get_setting(prefix + String("run_in_physics"), false));
		c.ignore_time_scale = bool(ps->get_setting(prefix + String("ignore_time_scale"), false));
		if (c.interval_sec < 0.0) {
			c.interval_sec = 0.0;
		}
		if (!c.enabled || c.method.is_empty()) {
			return;
		}
		channels.push_back(c);
	};

	load_channel(StringName("tick"));
	load_channel(StringName("anim"));
	load_channel(StringName("effect"));
	load_channel(StringName("low"));
	load_channel(StringName("decay"));
}

void CustomTree::reload_custom_processes_from_project_settings() {
	_reload_from_project_settings();
	_rebuild_registry();
}

void CustomTree::_on_process_frame() {
	const uint64_t now = Time::get_singleton() ? Time::get_singleton()->get_ticks_usec() : 0;
	if (last_process_usec == 0) {
		last_process_usec = now;
		if (!registry_built) {
			_rebuild_registry();
		}
		return;
	}

	_tick_channels(false);
	last_process_usec = now;
}

void CustomTree::_on_physics_frame() {
	const uint64_t now = Time::get_singleton() ? Time::get_singleton()->get_ticks_usec() : 0;
	if (last_physics_usec == 0) {
		last_physics_usec = now;
		if (!registry_built) {
			_rebuild_registry();
		}
		return;
	}

	_tick_channels(true);
	last_physics_usec = now;
}

void CustomTree::_tick_channels(bool p_physics_frame) {
	Time *time = Time::get_singleton();
	const uint64_t now = time ? time->get_ticks_usec() : 0;
	const uint64_t last = p_physics_frame ? last_physics_usec : last_process_usec;
	const double dt_real = (now > last) ? _u64_usec_to_sec(now - last) : 0.0;
	const double time_scale = Engine::get_singleton() ? (double)Engine::get_singleton()->get_time_scale() : 1.0;

	if (dt_real <= 0.0) {
		return;
	}

	const Array no_args;

	for (Channel &c : channels) {
		if (!c.enabled) {
			continue;
		}
		if (c.run_in_physics != p_physics_frame) {
			continue;
		}
		const double dt = c.ignore_time_scale ? dt_real : (dt_real * time_scale);
		if (dt <= 0.0) {
			continue;
		}

		auto run_targets = [&]() {
			for (size_t i = 0; i < c.targets.size(); /*increment inside*/) {
				Callable &cb = c.targets[i];
				if (!cb.is_valid()) {
					c.targets[i] = c.targets.back();
					c.targets.pop_back();
					continue;
				}
				cb.callv(no_args);
				i++;
			}
		};

		if (c.interval_sec <= 0.0) {
			run_targets();
			continue;
		}

		c.accumulator_sec += dt;
		while (c.accumulator_sec >= c.interval_sec) {
			c.accumulator_sec -= c.interval_sec;
			run_targets();
		}
	}
}

void CustomTree::_register_node(Node *p_node) {
	if (!p_node) {
		return;
	}
	const int64_t node_id = p_node->get_instance_id();
	for (Channel &c : channels) {
		if (!c.enabled || c.method.is_empty()) {
			continue;
		}
		if (!p_node->has_method(c.method)) {
			continue;
		}
		// Avoid duplicates.
		bool already_registered = false;
		for (const Callable &existing : c.targets) {
			if (existing.get_object_id() == node_id && existing.get_method() == c.method) {
				already_registered = true;
				break;
			}
		}
		if (!already_registered) {
			c.targets.emplace_back(Callable(p_node, c.method));
		}
	}
}

void CustomTree::_unregister_node(Node *p_node) {
	if (!p_node) {
		return;
	}
	const int64_t node_id = p_node->get_instance_id();
	for (Channel &c : channels) {
		for (size_t i = 0; i < c.targets.size(); /*increment inside*/) {
			if (c.targets[i].get_object_id() == node_id) {
				c.targets[i] = c.targets.back();
				c.targets.pop_back();
				continue;
			}
			i++;
		}
	}
}

void CustomTree::_rebuild_registry() {
	for (Channel &c : channels) {
		c.targets.clear();
		c.accumulator_sec = 0.0;
	}

	Node *root = get_root();
	if (!root) {
		registry_built = true;
		return;
	}

	std::vector<Node *> stack;
	stack.reserve(256);
	stack.push_back(root);

	while (!stack.empty()) {
		Node *node = stack.back();
		stack.pop_back();
		if (!node) {
			continue;
		}
		_register_node(node);

		const int child_count = node->get_child_count(false);
		for (int i = 0; i < child_count; i++) {
			Node *child = node->get_child(i, false);
			if (child) {
				stack.push_back(child);
			}
		}
	}

	registry_built = true;
}

void CustomTree::_on_node_added(Node *p_node) {
	_register_node(p_node);
}

void CustomTree::_on_node_removed(Node *p_node) {
	_unregister_node(p_node);
}

CustomTree::CustomTree() {
	_connect_frame_signals();
	_reload_from_project_settings();
}

CustomTree::~CustomTree() = default;

} // namespace godot
