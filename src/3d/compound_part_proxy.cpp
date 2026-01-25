#include "3d/compound_part_proxy.h"

#include "3d/compound_mesh_instance_3d.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CompoundPartProxy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_parent"), &CompoundPartProxy::get_parent);
	ClassDB::bind_method(D_METHOD("get_children"), &CompoundPartProxy::get_children);
	ClassDB::bind_method(D_METHOD("get_part_path"), &CompoundPartProxy::get_part_path);
	ClassDB::bind_method(D_METHOD("get_part_index"), &CompoundPartProxy::get_part_index);
}

void CompoundPartProxy::setup(ObjectID p_owner_id, int32_t p_index) {
	owner_id = p_owner_id;
	part_index = p_index;
}

static CompoundMeshInstance3D *_get_owner(ObjectID owner_id) {
	Object *o = ObjectDB::get_instance(owner_id);
	return Object::cast_to<CompoundMeshInstance3D>(o);
}

StringName CompoundPartProxy::get_part_path() const {
	CompoundMeshInstance3D *owner = _get_owner(owner_id);
	if (!owner) {
		return StringName();
	}
	return owner->get_part_path_by_index(part_index);
}

bool CompoundPartProxy::_set(const StringName &p_name, const Variant &p_value) {
	CompoundMeshInstance3D *owner = _get_owner(owner_id);
	if (!owner) {
		return false;
	}
	return owner->proxy_set(part_index, p_name, p_value);
}

bool CompoundPartProxy::_get(const StringName &p_name, Variant &r_ret) const {
	CompoundMeshInstance3D *owner = _get_owner(owner_id);
	if (!owner) {
		return false;
	}
	return owner->proxy_get(part_index, p_name, r_ret);
}

void CompoundPartProxy::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING_NAME, "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY));
	p_list->push_back(PropertyInfo(Variant::VECTOR3, "position"));
	p_list->push_back(PropertyInfo(Variant::VECTOR3, "rotation"));
	p_list->push_back(PropertyInfo(Variant::VECTOR3, "rotation_degrees"));
	p_list->push_back(PropertyInfo(Variant::VECTOR3, "scale"));
	p_list->push_back(PropertyInfo(Variant::TRANSFORM3D, "transform"));
	p_list->push_back(PropertyInfo(Variant::TRANSFORM3D, "global_transform"));
	p_list->push_back(PropertyInfo(Variant::VECTOR3, "global_position"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "visible"));
}

Variant CompoundPartProxy::get_parent() const {
	CompoundMeshInstance3D *owner = _get_owner(owner_id);
	if (!owner) {
		return Variant();
	}
	return owner->proxy_get_parent(part_index);
}

Array CompoundPartProxy::get_children() const {
	CompoundMeshInstance3D *owner = _get_owner(owner_id);
	if (!owner) {
		return Array();
	}
	return owner->proxy_get_children(part_index);
}

} // namespace godot
