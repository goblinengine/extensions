#include "3d/compound_mesh_instance_3d.h"

#include "3d/compound_part_proxy.h"
#include "3d/compound_part_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

static RenderingServer *RS() {
	return RenderingServer::get_singleton();
}

CompoundMeshInstance3D::CompoundMeshInstance3D() {
	set_notify_transform(true);
}

CompoundMeshInstance3D::~CompoundMeshInstance3D() {
	clear_all_instances();
	clear_part_nodes();
}

void CompoundMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_auto_flush", "enabled"), &CompoundMeshInstance3D::set_auto_flush);
	ClassDB::bind_method(D_METHOD("is_auto_flush"), &CompoundMeshInstance3D::is_auto_flush);
	ClassDB::bind_method(D_METHOD("set_create_part_nodes", "enabled"), &CompoundMeshInstance3D::set_create_part_nodes);
	ClassDB::bind_method(D_METHOD("is_create_part_nodes"), &CompoundMeshInstance3D::is_create_part_nodes);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_flush"), "set_auto_flush", "is_auto_flush");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "create_part_nodes"), "set_create_part_nodes", "is_create_part_nodes");

	ClassDB::bind_method(D_METHOD("clear_parts"), &CompoundMeshInstance3D::clear_parts);
	ClassDB::bind_method(D_METHOD("begin_batch"), &CompoundMeshInstance3D::begin_batch);
	ClassDB::bind_method(D_METHOD("end_batch", "flush_now"), &CompoundMeshInstance3D::end_batch, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("add_part", "path", "mesh", "local_transform", "material_override", "part_visible", "layer_mask", "cast_shadows"),
			&CompoundMeshInstance3D::add_part,
			DEFVAL(Transform3D()),
			DEFVAL(Ref<Material>()),
			DEFVAL(true),
			DEFVAL(1),
			DEFVAL((int32_t)GeometryInstance3D::SHADOW_CASTING_SETTING_ON));

	ClassDB::bind_method(D_METHOD("has_part", "path"), &CompoundMeshInstance3D::has_part);
	ClassDB::bind_method(D_METHOD("remove_part", "path"), &CompoundMeshInstance3D::remove_part);
	ClassDB::bind_method(D_METHOD("set_part_local_transform", "path", "transform"), &CompoundMeshInstance3D::set_part_local_transform);
	ClassDB::bind_method(D_METHOD("get_part_local_transform", "path"), &CompoundMeshInstance3D::get_part_local_transform);
	ClassDB::bind_method(D_METHOD("get_part_global_transform", "path"), &CompoundMeshInstance3D::get_part_global_transform);
	ClassDB::bind_method(D_METHOD("get_all_part_paths"), &CompoundMeshInstance3D::get_all_part_paths);
	ClassDB::bind_method(D_METHOD("set_all_layers", "layer_mask"), &CompoundMeshInstance3D::set_all_layers);
	ClassDB::bind_method(D_METHOD("set_part_cast_shadows", "path", "cast_shadows"), &CompoundMeshInstance3D::set_part_cast_shadows);
	ClassDB::bind_method(D_METHOD("set_part_shadow_only", "path", "enabled"), &CompoundMeshInstance3D::set_part_shadow_only, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_part_material_override", "path", "material"), &CompoundMeshInstance3D::set_part_material_override);
	ClassDB::bind_method(D_METHOD("set_part_visible", "path", "visible"), &CompoundMeshInstance3D::set_part_visible);
	ClassDB::bind_method(D_METHOD("set_part_mesh", "path", "mesh"), &CompoundMeshInstance3D::set_part_mesh);

	ClassDB::bind_method(D_METHOD("get_part_or_null", "path"), &CompoundMeshInstance3D::get_part_or_null);
	ClassDB::bind_method(D_METHOD("get_part", "path"), &CompoundMeshInstance3D::get_part);

	ClassDB::bind_method(D_METHOD("flush"), &CompoundMeshInstance3D::flush);
}

void CompoundMeshInstance3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		sync_all_scenarios();
		mark_all_dirty();
		flush();
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		clear_all_instances();
	} else if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		mark_all_dirty();
		if (auto_flush) {
			flush();
		}
	}
}

void CompoundMeshInstance3D::set_auto_flush(bool p_enabled) {
	auto_flush = p_enabled;
}

void CompoundMeshInstance3D::set_create_part_nodes(bool p_enabled) {
	if (create_part_nodes == p_enabled) {
		return;
	}
	create_part_nodes = p_enabled;
	if (create_part_nodes) {
		// Build Node3D hierarchy for existing parts.
		part_node_ids.resize(index_to_path.size());
		for (int32_t i = 0; i < (int32_t)index_to_path.size(); i++) {
			if (part_node_ids[i].is_valid()) {
				continue;
			}
			create_part_node(index_to_path[i], i, parent[i]);
			sync_part_node_from_local(i);
		}
	} else {
		clear_part_nodes();
	}
}

void CompoundMeshInstance3D::clear_parts() {
	proxy_cache.clear();
	path_to_index.clear();
	index_to_path.clear();

	parent.clear();
	first_child.clear();
	next_sibling.clear();

	local_xf.clear();
	world_xf.clear();

	meshes.clear();
	materials.clear();
	visible.clear();
	layer_mask.clear();
	cast_shadows.clear();

	dirty.clear();
	dirty_queue.clear();

	clear_part_nodes();
	clear_all_instances();
}

void CompoundMeshInstance3D::begin_batch() {
	batch_depth++;
}

void CompoundMeshInstance3D::end_batch(bool p_flush_now) {
	batch_depth = MAX(batch_depth - 1, 0);
	if (p_flush_now && batch_depth == 0 && auto_flush) {
		flush();
	}
}

bool CompoundMeshInstance3D::has_part(const StringName &p_path) const {
	return path_to_index.has(p_path);
}

void CompoundMeshInstance3D::add_part(const StringName &p_path, const Ref<Mesh> &p_mesh, const Transform3D &p_local_transform,
		const Ref<Material> &p_material_override, bool p_visible, int32_t p_layer_mask, int32_t p_cast_shadows) {
	if (p_path == StringName() || p_mesh.is_null()) {
		return;
	}

	String s = String(p_path);
	if (s.begins_with("/")) {
		// Part paths are relative-only (no leading '/').
		return;
	}
	StringName path = StringName(s);

	int32_t parent_idx = ensure_parent_chain(path);
	int32_t idx = -1;
	if (path_to_index.has(path)) {
		idx = path_to_index[path];
	} else {
		idx = create_part(path, parent_idx);
	}

	meshes.write[idx] = p_mesh;
	materials.write[idx] = p_material_override;
	visible.write[idx] = p_visible ? 1 : 0;
	layer_mask.write[idx] = p_layer_mask;
	cast_shadows.write[idx] = p_cast_shadows;
	local_xf.write[idx] = p_local_transform;
	if (create_part_nodes) {
		sync_part_node_from_local(idx);
	}

	recreate_instance(idx);
	mark_dirty_subtree(idx);
	maybe_auto_flush();
}

void CompoundMeshInstance3D::remove_part(const StringName &p_path) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	int32_t idx = path_to_index[p_path];
	if (idx < 0 || idx >= (int32_t)index_to_path.size()) {
		return;
	}

	free_instance(idx);
	meshes.write[idx] = Ref<Mesh>();
	materials.write[idx] = Ref<Material>();
	visible.write[idx] = 0;

	Object *n = get_part_node_object(idx);
	Node *nn = Object::cast_to<Node>(n);
	if (nn) {
		nn->queue_free();
		part_node_ids.write[idx] = ObjectID();
	}

	mark_dirty_subtree(idx);
	maybe_auto_flush();
}

void CompoundMeshInstance3D::set_part_local_transform(const StringName &p_path, const Transform3D &p_transform) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	int32_t idx = path_to_index[p_path];
	local_xf.write[idx] = p_transform;
	if (create_part_nodes) {
		sync_part_node_from_local(idx);
	}
	mark_dirty_subtree(idx);
	maybe_auto_flush();
}

Transform3D CompoundMeshInstance3D::get_part_local_transform(const StringName &p_path) const {
	if (!path_to_index.has(p_path)) {
		return Transform3D();
	}
	return local_xf[path_to_index[p_path]];
}

Transform3D CompoundMeshInstance3D::get_part_global_transform(const StringName &p_path) {
	if (!path_to_index.has(p_path)) {
		return is_inside_tree() ? get_global_transform() : Transform3D();
	}
	int32_t idx = path_to_index[p_path];
	if (idx >= 0 && idx < (int32_t)dirty.size() && dirty[idx]) {
		flush();
	}
	return world_xf[idx];
}

PackedStringArray CompoundMeshInstance3D::get_all_part_paths() const {
	PackedStringArray out;
	out.resize(index_to_path.size());
	for (int32_t i = 0; i < (int32_t)index_to_path.size(); i++) {
		out[i] = String(index_to_path[i]);
	}
	return out;
}

void CompoundMeshInstance3D::set_all_layers(int32_t p_layer_mask) {
	for (int32_t i = 0; i < (int32_t)layer_mask.size(); i++) {
		layer_mask.write[i] = p_layer_mask;
		if (instance_rid[i].is_valid()) {
			RS()->instance_set_layer_mask(instance_rid[i], p_layer_mask);
		}
	}
}

void CompoundMeshInstance3D::set_part_cast_shadows(const StringName &p_path, int32_t p_cast_shadows) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	set_part_cast_shadows_index(path_to_index[p_path], p_cast_shadows);
}

Variant CompoundMeshInstance3D::get_part_or_null(const NodePath &p_path) {
	String s = String(p_path);
	if (s.is_empty() || s == ".") {
		return this;
	}
	if (s.begins_with("/")) {
		// Part paths are relative-only (no leading '/').
		return Variant();
	}
	StringName key(s);
	if (!path_to_index.has(key)) {
		return Variant();
	}
	int32_t idx = path_to_index[key];
	if (create_part_nodes) {
		Object *n = get_part_node_object(idx);
		if (n) {
			return n;
		}
	}
	return get_or_create_proxy(idx);
}

Variant CompoundMeshInstance3D::get_part(const NodePath &p_path) {
	Variant v = get_part_or_null(p_path);
	if (v.get_type() == Variant::NIL) {
		UtilityFunctions::push_error(vformat("CompoundMeshInstance3D: Part not found: %s", String(p_path)));
	}
	return v;
}

void CompoundMeshInstance3D::flush() {
	if (!is_inside_tree()) {
		return;
	}
	if (dirty_queue.is_empty()) {
		return;
	}

	Vector<int32_t> roots;
	for (int32_t i = 0; i < (int32_t)dirty_queue.size(); i++) {
		int32_t idx = dirty_queue[i];
		if (idx < 0 || idx >= (int32_t)dirty.size()) {
			continue;
		}
		if (!dirty[idx]) {
			continue;
		}
		int32_t p = parent[idx];
		if (p == -1 || (p >= 0 && p < (int32_t)dirty.size() && !dirty[p])) {
			roots.push_back(idx);
		}
	}

	dirty_queue.clear();

	for (int32_t r = 0; r < (int32_t)roots.size(); r++) {
		int32_t root_idx = roots[r];
		Transform3D parent_world;
		int32_t p = parent[root_idx];
		if (p == -1) {
			parent_world = get_global_transform();
		} else {
			parent_world = world_xf[p];
		}
		update_subtree_world_and_rid(root_idx, parent_world);
	}
}

void CompoundMeshInstance3D::on_part_node_transform_changed(int32_t p_index) {
	if (!create_part_nodes) {
		return;
	}
	if (suppress_part_node_sync > 0) {
		return;
	}
	if (p_index < 0 || p_index >= (int32_t)local_xf.size()) {
		return;
	}
	Object *n = get_part_node_object(p_index);
	Node3D *node = Object::cast_to<Node3D>(n);
	if (!node) {
		return;
	}
	local_xf.write[p_index] = node->get_transform();
	mark_dirty_subtree(p_index);
	maybe_auto_flush();
}

StringName CompoundMeshInstance3D::get_part_path_by_index(int32_t p_index) const {
	if (p_index < 0 || p_index >= (int32_t)index_to_path.size()) {
		return StringName();
	}
	return index_to_path[p_index];
}

bool CompoundMeshInstance3D::proxy_set(int32_t p_index, const StringName &p_property, const Variant &p_value) {
	if (p_index < 0 || p_index >= (int32_t)local_xf.size()) {
		return false;
	}

	Transform3D t = local_xf[p_index];

	if (p_property == StringName("position")) {
		t.origin = (Vector3)p_value;
		local_xf.write[p_index] = t;
		mark_dirty_subtree(p_index);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("rotation")) {
		Vector3 rot = (Vector3)p_value;
		Vector3 sc = t.basis.get_scale();
		t.basis = Basis::from_euler(rot).scaled(sc);
		local_xf.write[p_index] = t;
		mark_dirty_subtree(p_index);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("rotation_degrees")) {
		Vector3 rot_deg = (Vector3)p_value;
		Vector3 sc2 = t.basis.get_scale();
		t.basis = Basis::from_euler(rot_deg * Math_PI / 180.0).scaled(sc2);
		local_xf.write[p_index] = t;
		mark_dirty_subtree(p_index);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("scale")) {
		Vector3 scv = (Vector3)p_value;
		Basis rot = t.basis.orthonormalized();
		t.basis = rot.scaled(scv);
		local_xf.write[p_index] = t;
		mark_dirty_subtree(p_index);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("transform")) {
		local_xf.write[p_index] = (Transform3D)p_value;
		mark_dirty_subtree(p_index);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("global_transform")) {
		set_part_global_transform_by_index(p_index, (Transform3D)p_value);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("global_position")) {
		Transform3D gt = get_part_global_transform(index_to_path[p_index]);
		gt.origin = (Vector3)p_value;
		set_part_global_transform_by_index(p_index, gt);
		maybe_auto_flush();
		return true;
	}
	if (p_property == StringName("visible")) {
		visible.write[p_index] = ((bool)p_value) ? 1 : 0;
		if (instance_rid[p_index].is_valid()) {
			RS()->instance_set_visible(instance_rid[p_index], visible[p_index] == 1);
		}
		return true;
	}

	return false;
}

bool CompoundMeshInstance3D::proxy_get(int32_t p_index, const StringName &p_property, Variant &r_ret) const {
	if (p_index < 0 || p_index >= (int32_t)local_xf.size()) {
		return false;
	}

	if (p_property == StringName("name")) {
		String s = String(index_to_path[p_index]);
		r_ret = StringName(s.get_file());
		return true;
	}
	if (p_property == StringName("position")) {
		r_ret = local_xf[p_index].origin;
		return true;
	}
	if (p_property == StringName("rotation")) {
		r_ret = local_xf[p_index].basis.get_euler();
		return true;
	}
	if (p_property == StringName("rotation_degrees")) {
		r_ret = local_xf[p_index].basis.get_euler() * (180.0 / Math_PI);
		return true;
	}
	if (p_property == StringName("scale")) {
		r_ret = local_xf[p_index].basis.get_scale();
		return true;
	}
	if (p_property == StringName("transform")) {
		r_ret = local_xf[p_index];
		return true;
	}
	if (p_property == StringName("global_transform")) {
		// Not safe to force flush from const; return cached.
		r_ret = world_xf[p_index];
		return true;
	}
	if (p_property == StringName("global_position")) {
		r_ret = world_xf[p_index].origin;
		return true;
	}
	if (p_property == StringName("visible")) {
		r_ret = visible[p_index] == 1;
		return true;
	}

	return false;
}

Variant CompoundMeshInstance3D::proxy_get_parent(int32_t p_index) const {
	if (p_index < 0 || p_index >= (int32_t)parent.size()) {
		return Variant();
	}
	int32_t p = parent[p_index];
	if (p == -1) {
		return const_cast<CompoundMeshInstance3D *>(this);
	}
	if (create_part_nodes) {
		Object *n = get_part_node_object(p);
		if (n) {
			return n;
		}
	}
	return get_or_create_proxy(p);
}

Array CompoundMeshInstance3D::proxy_get_children(int32_t p_index) const {
	Array out;
	if (p_index < 0 || p_index >= (int32_t)first_child.size()) {
		return out;
	}
	int32_t c = first_child[p_index];
	while (c != -1) {
		if (create_part_nodes) {
			Object *n = get_part_node_object(c);
			if (n) {
				out.append(n);
			} else {
				out.append(get_or_create_proxy(c));
			}
		} else {
			out.append(get_or_create_proxy(c));
		}
		c = next_sibling[c];
	}
	return out;
}

int32_t CompoundMeshInstance3D::ensure_parent_chain(const StringName &p_path) {
	String s = String(p_path);
	int slash = s.rfind("/");
	if (slash == -1) {
		return -1;
	}
	String parent_str = s.substr(0, slash);
	StringName parent_path(parent_str);
	if (path_to_index.has(parent_path)) {
		return path_to_index[parent_path];
	}
	int32_t gpidx = ensure_parent_chain(parent_path);
	int32_t pidx = create_part(parent_path, gpidx);
	return pidx;
}

int32_t CompoundMeshInstance3D::create_part(const StringName &p_path, int32_t p_parent_index) {
	int32_t idx = (int32_t)index_to_path.size();
	path_to_index.insert(p_path, idx);
	index_to_path.push_back(p_path);

	parent.push_back(p_parent_index);
	first_child.push_back(-1);
	next_sibling.push_back(-1);

	local_xf.push_back(Transform3D());
	world_xf.push_back(Transform3D());

	instance_rid.push_back(RID());
	meshes.push_back(Ref<Mesh>());
	materials.push_back(Ref<Material>());
	visible.push_back(1);
	layer_mask.push_back(1);
	cast_shadows.push_back((int32_t)GeometryInstance3D::SHADOW_CASTING_SETTING_ON);

	dirty.push_back(1);
	dirty_queue.push_back(idx);

	if (p_parent_index != -1) {
		next_sibling.write[idx] = first_child[p_parent_index];
		first_child.write[p_parent_index] = idx;
	}

	if (create_part_nodes) {
		create_part_node(p_path, idx, p_parent_index);
	}

	return idx;
}

void CompoundMeshInstance3D::recreate_instance(int32_t p_index) {
	free_instance(p_index);
	Ref<Mesh> mesh = meshes[p_index];
	if (mesh.is_null()) {
		return;
	}
	RID inst = RS()->instance_create();
	RS()->instance_set_base(inst, mesh->get_rid());
	RS()->instance_set_visible(inst, visible[p_index] == 1);
	RS()->instance_set_layer_mask(inst, layer_mask[p_index]);
	RS()->instance_geometry_set_cast_shadows_setting(inst, (RenderingServer::ShadowCastingSetting)cast_shadows[p_index]);
	Ref<Material> mat = materials[p_index];
	if (mat.is_valid()) {
		RS()->instance_geometry_set_material_override(inst, mat->get_rid());
	}
	instance_rid.write[p_index] = inst;

	if (is_inside_tree()) {
		Ref<World3D> w = get_world_3d();
		if (w.is_valid()) {
			RS()->instance_set_scenario(inst, w->get_scenario());
		}
	}
}

void CompoundMeshInstance3D::free_instance(int32_t p_index) {
	RID inst = instance_rid[p_index];
	if (inst.is_valid()) {
		RS()->free_rid(inst);
	}
	instance_rid.write[p_index] = RID();
}

void CompoundMeshInstance3D::clear_all_instances() {
	for (int32_t i = 0; i < (int32_t)instance_rid.size(); i++) {
		free_instance(i);
	}
}

void CompoundMeshInstance3D::sync_all_scenarios() {
	if (!is_inside_tree()) {
		return;
	}
	Ref<World3D> w = get_world_3d();
	if (!w.is_valid()) {
		return;
	}
	RID scenario = w->get_scenario();
	for (int32_t i = 0; i < (int32_t)instance_rid.size(); i++) {
		if (instance_rid[i].is_valid()) {
			RS()->instance_set_scenario(instance_rid[i], scenario);
		}
	}
}

void CompoundMeshInstance3D::mark_all_dirty() {
	for (int32_t i = 0; i < (int32_t)dirty.size(); i++) {
		if (!dirty[i]) {
			dirty.write[i] = 1;
			dirty_queue.push_back(i);
		}
	}
}

void CompoundMeshInstance3D::mark_dirty_subtree(int32_t p_index) {
	Vector<int32_t> stack;
	stack.push_back(p_index);
	while (!stack.is_empty()) {
		int32_t n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!dirty[n]) {
			dirty.write[n] = 1;
			dirty_queue.push_back(n);
		}
		int32_t c = first_child[n];
		while (c != -1) {
			stack.push_back(c);
			c = next_sibling[c];
		}
	}
}

void CompoundMeshInstance3D::update_subtree_world_and_rid(int32_t p_root_index, const Transform3D &p_parent_world) {
	Vector<int32_t> stack_nodes;
	Vector<Transform3D> stack_parents;
	stack_nodes.push_back(p_root_index);
	stack_parents.push_back(p_parent_world);

	while (!stack_nodes.is_empty()) {
		int32_t idx = stack_nodes[stack_nodes.size() - 1];
		stack_nodes.resize(stack_nodes.size() - 1);
		Transform3D pw = stack_parents[stack_parents.size() - 1];
		stack_parents.resize(stack_parents.size() - 1);

		Transform3D wxf = pw * local_xf[idx];
		world_xf.write[idx] = wxf;
		dirty.write[idx] = 0;

		RID inst = instance_rid[idx];
		if (inst.is_valid()) {
			RS()->instance_set_transform(inst, wxf);
		}

		int32_t c = first_child[idx];
		while (c != -1) {
			stack_nodes.push_back(c);
			stack_parents.push_back(wxf);
			c = next_sibling[c];
		}
	}
}

Object *CompoundMeshInstance3D::get_part_node_object(int32_t p_index) const {
	if (p_index < 0 || p_index >= (int32_t)part_node_ids.size()) {
		return nullptr;
	}
	if (!part_node_ids[p_index].is_valid()) {
		return nullptr;
	}
	return ObjectDB::get_instance(part_node_ids[p_index]);
}

void CompoundMeshInstance3D::create_part_node(const StringName &p_path, int32_t p_index, int32_t p_parent_index) {
	if (!create_part_nodes) {
		return;
	}
	if (part_node_ids.size() < (int32_t)index_to_path.size()) {
		part_node_ids.resize(index_to_path.size());
	}

	CompoundPartNode3D *n = memnew(CompoundPartNode3D);
	String s = String(p_path);
	int slash = s.rfind("/");
	String leaf = (slash == -1) ? s : s.substr(slash + 1);
	n->set_name(leaf);
	n->setup(ObjectID(get_instance_id()), p_index);

	Node *parent_node = this;
	if (p_parent_index != -1) {
		Object *pn = get_part_node_object(p_parent_index);
		Node *pnode = Object::cast_to<Node>(pn);
		if (pnode) {
			parent_node = pnode;
		}
	}
	parent_node->add_child(n);
	part_node_ids.write[p_index] = n->get_instance_id();
}

void CompoundMeshInstance3D::clear_part_nodes() {
	for (int32_t i = 0; i < (int32_t)part_node_ids.size(); i++) {
		Object *o = ObjectDB::get_instance(part_node_ids[i]);
		Node *n = Object::cast_to<Node>(o);
		if (n) {
			n->queue_free();
		}
		part_node_ids.write[i] = ObjectID();
	}
	part_node_ids.clear();
}

void CompoundMeshInstance3D::sync_part_node_from_local(int32_t p_index) {
	Object *o = get_part_node_object(p_index);
	Node3D *n = Object::cast_to<Node3D>(o);
	if (!n) {
		return;
	}
	suppress_part_node_sync++;
	n->set_transform(local_xf[p_index]);
	suppress_part_node_sync = MAX(suppress_part_node_sync - 1, 0);
}

void CompoundMeshInstance3D::maybe_auto_flush() {
	if (auto_flush && batch_depth == 0) {
		flush();
	}
}

void CompoundMeshInstance3D::set_part_global_transform_by_index(int32_t p_index, const Transform3D &p_global) {
	int32_t p = parent[p_index];
	Transform3D parent_world;
	if (p == -1) {
		parent_world = is_inside_tree() ? get_global_transform() : Transform3D();
	} else {
		if (p >= 0 && p < (int32_t)dirty.size() && dirty[p]) {
			flush();
		}
		parent_world = world_xf[p];
	}
	local_xf.write[p_index] = parent_world.affine_inverse() * p_global;
	if (create_part_nodes) {
		sync_part_node_from_local(p_index);
	}
	mark_dirty_subtree(p_index);
}

void CompoundMeshInstance3D::set_part_cast_shadows_index(int32_t p_index, int32_t p_cast_shadows) {
	if (p_index < 0 || p_index >= (int32_t)cast_shadows.size()) {
		return;
	}
	cast_shadows.write[p_index] = p_cast_shadows;
	if (instance_rid[p_index].is_valid()) {
		RS()->instance_geometry_set_cast_shadows_setting(instance_rid[p_index], (RenderingServer::ShadowCastingSetting)p_cast_shadows);
	}
}

void CompoundMeshInstance3D::set_part_shadow_only(const StringName &p_path, bool p_enabled) {
	set_part_cast_shadows(p_path, p_enabled ? (int32_t)GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY : (int32_t)GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
}

void CompoundMeshInstance3D::set_part_material_override(const StringName &p_path, const Ref<Material> &p_material) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	int32_t idx = path_to_index[p_path];
	materials.write[idx] = p_material;
	if (instance_rid[idx].is_valid()) {
		if (p_material.is_valid()) {
			RS()->instance_geometry_set_material_override(instance_rid[idx], p_material->get_rid());
		} else {
			RS()->instance_geometry_set_material_override(instance_rid[idx], RID());
		}
	}
}

void CompoundMeshInstance3D::set_part_visible(const StringName &p_path, bool p_visible) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	int32_t idx = path_to_index[p_path];
	visible.write[idx] = p_visible ? 1 : 0;
	if (instance_rid[idx].is_valid()) {
		RS()->instance_set_visible(instance_rid[idx], p_visible);
	}
}

void CompoundMeshInstance3D::set_part_mesh(const StringName &p_path, const Ref<Mesh> &p_mesh) {
	if (!path_to_index.has(p_path)) {
		return;
	}
	int32_t idx = path_to_index[p_path];
	meshes.write[idx] = p_mesh;
	recreate_instance(idx);
	maybe_auto_flush();
}

Ref<CompoundPartProxy> CompoundMeshInstance3D::get_or_create_proxy(int32_t p_index) const {
	StringName path = index_to_path[p_index];
	if (proxy_cache.has(path)) {
		return proxy_cache[path];
	}
	Ref<CompoundPartProxy> proxy;
	proxy.instantiate();
	proxy->setup(ObjectID(get_instance_id()), p_index);
	proxy_cache.insert(path, proxy);
	return proxy;
}

} // namespace godot
