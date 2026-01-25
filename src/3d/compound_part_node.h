#pragma once

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CompoundMeshInstance3D;

class CompoundPartNode3D : public Node3D {
	GDCLASS(CompoundPartNode3D, Node3D);

	ObjectID owner_id;
	int32_t part_index = -1;

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	void setup(ObjectID p_owner_id, int32_t p_index);
	int32_t get_part_index() const { return part_index; }
};

} // namespace godot
