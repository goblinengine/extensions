#include "compound_part_node.h"

#include "compound_mesh_instance_3d.h"

#include <godot_cpp/core/object.hpp>

namespace godot {

void CompoundPartNode3D::setup(ObjectID p_owner_id, int32_t p_index) {
	owner_id = p_owner_id;
	part_index = p_index;
	set_notify_transform(true);
}

void CompoundPartNode3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		Object *o = ObjectDB::get_instance(owner_id);
		CompoundMeshInstance3D *owner = Object::cast_to<CompoundMeshInstance3D>(o);
		if (owner) {
			owner->on_part_node_transform_changed(part_index);
		}
	}
}

} // namespace godot
