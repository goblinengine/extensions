#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/templates/hash_map.hpp>

namespace godot {

class CompoundPartProxy;
class CompoundPartNode3D;

class CompoundMeshInstance3D : public Node3D {
	GDCLASS(CompoundMeshInstance3D, Node3D);

	bool auto_flush = true;
	bool create_part_nodes = true;  // Default: true for animation blender compatibility

	HashMap<StringName, int32_t> path_to_index;
	Vector<StringName> index_to_path;

	Vector<int32_t> parent;
	Vector<int32_t> first_child;
	Vector<int32_t> next_sibling;

	Vector<Transform3D> local_xf;
	Vector<Transform3D> world_xf;

	Vector<RID> instance_rid;
	Vector<Ref<Mesh>> meshes;
	Vector<Ref<Material>> materials;

	Vector<uint8_t> visible;
	Vector<int32_t> layer_mask;
	Vector<int32_t> cast_shadows;

	Vector<uint8_t> dirty;
	Vector<int32_t> dirty_queue;

	int32_t batch_depth = 0;

	// Optional Node3D hierarchy for compatibility with NodePath-based animation systems.
	Vector<ObjectID> part_node_ids;
	int32_t suppress_part_node_sync = 0;

	// Proxy cache (path -> proxy)
	mutable HashMap<StringName, Ref<CompoundPartProxy>> proxy_cache;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	CompoundMeshInstance3D();
	~CompoundMeshInstance3D();

	// Properties
	void set_auto_flush(bool p_enabled);
	bool is_auto_flush() const { return auto_flush; }

	void set_create_part_nodes(bool p_enabled);
	bool is_create_part_nodes() const { return create_part_nodes; }

	// Core API
	void clear_parts();
	void begin_batch();
	void end_batch(bool p_flush_now = true);

	void add_part(const StringName &p_path, const Ref<Mesh> &p_mesh, const Transform3D &p_local_transform = Transform3D(),
			const Ref<Material> &p_material_override = Ref<Material>(), bool p_visible = true, int32_t p_layer_mask = 1,
			int32_t p_cast_shadows = 1);

	bool has_part(const StringName &p_path) const;
	void remove_part(const StringName &p_path);

	void set_part_local_transform(const StringName &p_path, const Transform3D &p_transform);
	Transform3D get_part_local_transform(const StringName &p_path) const;
	Transform3D get_part_global_transform(const StringName &p_path);

	PackedStringArray get_all_part_paths() const;

	void set_all_layers(int32_t p_layer_mask);
	void set_part_cast_shadows(const StringName &p_path, int32_t p_cast_shadows);
	void set_part_shadow_only(const StringName &p_path, bool p_enabled = true);
	void set_part_material_override(const StringName &p_path, const Ref<Material> &p_material);
	void set_part_visible(const StringName &p_path, bool p_visible);
	void set_part_mesh(const StringName &p_path, const Ref<Mesh> &p_mesh);

	Variant get_part_or_null(const NodePath &p_path);
	Variant get_part(const NodePath &p_path);

	void flush();

	// Called by CompoundPartNode3D.
	void on_part_node_transform_changed(int32_t p_index);

	// Proxy helper surface.
	StringName get_part_path_by_index(int32_t p_index) const;
	bool proxy_set(int32_t p_index, const StringName &p_property, const Variant &p_value);
	bool proxy_get(int32_t p_index, const StringName &p_property, Variant &r_ret) const;
	Variant proxy_get_parent(int32_t p_index) const;
	Array proxy_get_children(int32_t p_index) const;

private:
	int32_t ensure_parent_chain(const StringName &p_path);
	int32_t create_part(const StringName &p_path, int32_t p_parent_index);
	void recreate_instance(int32_t p_index);
	void free_instance(int32_t p_index);
	void clear_all_instances();
	void sync_all_scenarios();

	void mark_all_dirty();
	void mark_dirty_subtree(int32_t p_index);
	void update_subtree_world_and_rid(int32_t p_root_index, const Transform3D &p_parent_world);

	Object *get_part_node_object(int32_t p_index) const;
	void create_part_node(const StringName &p_path, int32_t p_index, int32_t p_parent_index);
	void clear_part_nodes();
	void sync_part_node_from_local(int32_t p_index);
	void maybe_auto_flush();
	void set_part_global_transform_by_index(int32_t p_index, const Transform3D &p_global);
	void set_part_cast_shadows_index(int32_t p_index, int32_t p_cast_shadows);

	Ref<CompoundPartProxy> get_or_create_proxy(int32_t p_index) const;
};

} // namespace godot
