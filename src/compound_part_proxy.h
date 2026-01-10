#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

class CompoundMeshInstance3D;

class CompoundPartProxy : public RefCounted {
	GDCLASS(CompoundPartProxy, RefCounted);

	ObjectID owner_id;
	int32_t part_index = -1;

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void setup(ObjectID p_owner_id, int32_t p_index);

	Variant get_parent() const;
	Array get_children() const;

	StringName get_part_path() const;
	int32_t get_part_index() const { return part_index; }
};

} // namespace godot
