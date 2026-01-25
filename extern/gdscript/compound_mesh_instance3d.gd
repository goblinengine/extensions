## CompoundMeshInstance3D
##
## A single Node3D that *virtualizes* a hierarchy of mesh instances without spawning
## child nodes. Parts are addressed by NodePath-like strings (e.g. "hip/torso/head").
##
## - `get_node()` / `get_node_or_null()` are overridden to return lightweight proxy
##   objects for virtual parts (duck-typed to behave like Node3D for transforms).
## - Rendering is done via `RenderingServer` instances (RIDs).
## - Transforms are hierarchical: each part has a local Transform3D, composed with
##   its parent chain and this node's `global_transform`.
##
## Notes:
## - Purely visual: no physics, no signals per-part.
## - Designed for high-frequency animation updates: supports batched updates via
##   `begin_batch()` / `end_batch()` and deferred `flush()`.

@tool
class_name CompoundMeshInstance3D_GDScript
extends Node3D

@export var auto_flush: bool = true

var _path_to_index: Dictionary = {} # StringName -> int
var _index_to_path: PackedStringArray = PackedStringArray()

var _parent: PackedInt32Array = PackedInt32Array()      # parent index, -1 for root
var _first_child: PackedInt32Array = PackedInt32Array() # first child index, -1 none
var _next_sibling: PackedInt32Array = PackedInt32Array()# next sibling index, -1 none

var _local: Array[Transform3D] = []
var _world: Array[Transform3D] = []

var _instance_rid: Array[RID] = []
var _mesh: Array[Mesh] = []
var _material_override: Array[Material] = []

var _visible: PackedByteArray = PackedByteArray()       # 0/1
var _layer_mask: PackedInt32Array = PackedInt32Array()
var _cast_shadows: PackedInt32Array = PackedInt32Array()

var _dirty: PackedByteArray = PackedByteArray()         # 0/1
var _dirty_queue: PackedInt32Array = PackedInt32Array() # indices possibly dirty

var _in_batch: int = 0

var _proxy_cache: Dictionary = {} # StringName -> RefCounted proxy

# Lightweight Node3D hierarchy for animation systems (e.g. AnimationBlender).
# These nodes have NO meshes; they only carry transforms. Their transforms are synced into
# the virtual part transforms used for RenderingServer instances.
var _part_nodes: Array[Node3D] = []
var _suppress_part_node_sync: int = 0


class _PartNode:
	extends Node3D
	var _owner: CompoundMeshInstance3D = null
	var _idx: int = -1

	func _ready() -> void:
		set_notify_transform(true)

	func _notification(what: int) -> void:
		if what == NOTIFICATION_TRANSFORM_CHANGED and _owner != null:
			_owner._on_part_node_transform_changed(_idx)

	func set_cast_shadow_mode(mode: int) -> void:
		if _owner != null:
			_owner._set_part_cast_shadows_index(_idx, mode)

	func set_shadow_only(enabled: bool = true) -> void:
		set_cast_shadow_mode(GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY if enabled else GeometryInstance3D.SHADOW_CASTING_SETTING_ON)


func _notification(what: int) -> void:
	match what:
		NOTIFICATION_ENTER_TREE:
			_sync_all_scenarios()
			_mark_all_dirty()
			flush()
		NOTIFICATION_EXIT_TREE:
			_clear_all_instances()
		NOTIFICATION_TRANSFORM_CHANGED:
			# Our global transform changed; all part world transforms changed.
			_mark_all_dirty()
			if auto_flush:
				flush()


# --- Public API --------------------------------------------------------------

func clear_parts() -> void:
	_proxy_cache.clear()
	_clear_part_nodes()
	_path_to_index.clear()
	_index_to_path = PackedStringArray()
	_parent = PackedInt32Array()
	_first_child = PackedInt32Array()
	_next_sibling = PackedInt32Array()
	_local.clear()
	_world.clear()
	_mesh.clear()
	_material_override.clear()
	_visible = PackedByteArray()
	_layer_mask = PackedInt32Array()
	_cast_shadows = PackedInt32Array()
	_dirty = PackedByteArray()
	_dirty_queue = PackedInt32Array()
	_clear_all_instances()


func begin_batch() -> void:
	_in_batch += 1


func end_batch(flush_now: bool = true) -> void:
	_in_batch = max(_in_batch - 1, 0)
	if flush_now and _in_batch == 0 and auto_flush:
		flush()


func add_part(
		path: StringName,
		mesh: Mesh,
		local_transform: Transform3D = Transform3D.IDENTITY,
		material_override: Material = null,
		part_visible: bool = true,
		layer_mask: int = 1,
		cast_shadows: int = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	) -> void:
	if path == &"" or mesh == null:
		return
	var path_str := String(path)
	if path_str.begins_with("/"):
		# Part paths are relative-only (no leading '/').
		return
	path = StringName(path_str)

	# Ensure parents exist (virtual transform-only parts are allowed).
	var parent_idx := _ensure_parent_chain(path)
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		idx = _create_part(path, parent_idx)

	_mesh[idx] = mesh
	_material_override[idx] = material_override
	_visible[idx] = 1 if part_visible else 0
	_layer_mask[idx] = layer_mask
	_cast_shadows[idx] = cast_shadows
	_local[idx] = local_transform
	_sync_part_node_from_local(idx)

	_recreate_instance(idx)
	_mark_dirty_subtree(idx)
	if auto_flush and _in_batch == 0:
		flush()


func has_part(path: StringName) -> bool:
	return _path_to_index.has(path)


func remove_part(path: StringName) -> void:
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		return
	if idx >= 0 and idx < _part_nodes.size() and _part_nodes[idx] != null:
		_part_nodes[idx].queue_free()
		_part_nodes[idx] = null
	_free_instance(idx)
	_mesh[idx] = null
	_material_override[idx] = null
	_visible[idx] = 0
	_mark_dirty_subtree(idx)
	if auto_flush and _in_batch == 0:
		flush()


func set_part_local_transform(path: StringName, t: Transform3D) -> void:
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		return
	_local[idx] = t
	_sync_part_node_from_local(idx)
	_mark_dirty_subtree(idx)
	if auto_flush and _in_batch == 0:
		flush()


func get_part_local_transform(path: StringName) -> Transform3D:
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		return Transform3D.IDENTITY
	return _local[idx]


func get_part_global_transform(path: StringName) -> Transform3D:
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		return global_transform if is_inside_tree() else Transform3D.IDENTITY
	# Ensure world cache is up-to-date.
	if _dirty[idx] == 1:
		flush()
	return _world[idx]


func set_all_layers(layer_mask: int) -> void:
	for i in range(_layer_mask.size()):
		_layer_mask[i] = layer_mask
		if _instance_rid[i].is_valid():
			RenderingServer.instance_set_layer_mask(_instance_rid[i], layer_mask)


func set_part_cast_shadows(path: StringName, cast_shadows: int) -> void:
	var idx: int = _path_to_index.get(path, -1)
	if idx == -1:
		return
	_set_part_cast_shadows_index(idx, cast_shadows)


func set_part_shadow_only(path: StringName, enabled: bool = true) -> void:
	set_part_cast_shadows(path, GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY if enabled else GeometryInstance3D.SHADOW_CASTING_SETTING_ON)


func get_all_part_paths() -> Array[StringName]:
	var paths: Array[StringName] = []
	for path in _path_to_index:
		paths.append(path)
	return paths


func flush() -> void:
	if not is_inside_tree():
		return
	if _dirty_queue.is_empty():
		return

	# Build a list of dirty roots (dirty nodes whose parents are not dirty).
	var roots := PackedInt32Array()
	for i in _dirty_queue:
		if i < 0 or i >= _dirty.size():
			continue
		if _dirty[i] == 0:
			continue
		var p := _parent[i]
		if p == -1 or _dirty[p] == 0:
			roots.append(i)

	_dirty_queue = PackedInt32Array()

	for r in roots:
		var parent_world: Transform3D
		var p := _parent[r]
		if p == -1:
			parent_world = global_transform
		else:
			parent_world = _world[p]
		_update_subtree_world_and_rid(r, parent_world)


# --- Node virtualization (custom methods, not overrides) -------------------

func get_part(path: NodePath) -> Variant:
	var n: Variant = get_part_or_null(path)
	if n == null:
		push_error("CompoundMeshInstance3D: Part not found: %s" % String(path))
		return null
	return n


func get_part_or_null(path: NodePath) -> Variant:
	var s := String(path)
	if s == "" or s == ".":
		return self
	if s.begins_with("/"):
		# Part paths are relative-only (no leading '/').
		return null
	var key := StringName(s)
	var idx: int = _path_to_index.get(key, -1)
	if idx != -1:
		if idx >= 0 and idx < _part_nodes.size() and _part_nodes[idx] != null:
			return _part_nodes[idx]
		return _get_proxy_for_index(idx)
	return null


func has_part_path(path: NodePath) -> bool:
	return get_part_or_null(path) != null


# --- Internal: part creation & instance management --------------------------

func _ensure_parent_chain(path: StringName) -> int:
	var s := String(path)
	var slash := s.rfind("/")
	if slash == -1:
		return -1
	var parent_path := StringName(s.substr(0, slash))
	var pidx: int = _path_to_index.get(parent_path, -1)
	if pidx != -1:
		return pidx
	var gpidx := _ensure_parent_chain(parent_path)
	pidx = _create_part(parent_path, gpidx)
	# Parent is transform-only by default (no mesh)
	return pidx


func _create_part(path: StringName, parent_idx: int) -> int:
	var idx := _index_to_path.size()
	_path_to_index[path] = idx
	_index_to_path.append(String(path))

	_parent.append(parent_idx)
	_first_child.append(-1)
	_next_sibling.append(-1)

	_local.append(Transform3D.IDENTITY)
	_world.append(Transform3D.IDENTITY)

	_instance_rid.append(RID())
	_mesh.append(null)
	_material_override.append(null)

	_visible.append(1)
	_layer_mask.append(1)
	_cast_shadows.append(GeometryInstance3D.SHADOW_CASTING_SETTING_ON)

	_dirty.append(1)
	_dirty_queue.append(idx)

	if parent_idx != -1:
		_next_sibling[idx] = _first_child[parent_idx]
		_first_child[parent_idx] = idx

	_create_part_node(path, idx, parent_idx)

	return idx


func _create_part_node(path: StringName, idx: int, parent_idx: int) -> void:
	while _part_nodes.size() <= idx:
		_part_nodes.append(null)
	var n := _PartNode.new()
	var s := String(path)
	var slash := s.rfind("/")
	n.name = s.substr(slash + 1) if slash != -1 else s
	n._owner = self
	n._idx = idx
	var parent_node: Node = self
	if parent_idx != -1 and parent_idx < _part_nodes.size() and _part_nodes[parent_idx] != null:
		parent_node = _part_nodes[parent_idx]
	parent_node.add_child(n)
	_part_nodes[idx] = n
	_sync_part_node_from_local(idx)


func _clear_part_nodes() -> void:
	for n in _part_nodes:
		if n != null:
			n.queue_free()
	_part_nodes.clear()


func _sync_part_node_from_local(idx: int) -> void:
	if idx < 0 or idx >= _part_nodes.size():
		return
	var n := _part_nodes[idx]
	if n == null:
		return
	_suppress_part_node_sync += 1
	n.transform = _local[idx]
	_suppress_part_node_sync = max(_suppress_part_node_sync - 1, 0)


func _on_part_node_transform_changed(idx: int) -> void:
	if _suppress_part_node_sync > 0:
		return
	if idx < 0 or idx >= _part_nodes.size():
		return
	var n := _part_nodes[idx]
	if n == null:
		return
	_local[idx] = n.transform
	_mark_dirty_subtree(idx)
	_maybe_auto_flush()


func _recreate_instance(idx: int) -> void:
	_free_instance(idx)
	var mesh := _mesh[idx]
	if mesh == null:
		return
	var inst := RenderingServer.instance_create()
	RenderingServer.instance_set_base(inst, mesh.get_rid())
	RenderingServer.instance_set_visible(inst, _visible[idx] == 1)
	RenderingServer.instance_set_layer_mask(inst, _layer_mask[idx])
	RenderingServer.instance_geometry_set_cast_shadows_setting(inst, _cast_shadows[idx])
	var mat := _material_override[idx]
	if mat != null:
		RenderingServer.instance_geometry_set_material_override(inst, mat.get_rid())
	_instance_rid[idx] = inst

	if is_inside_tree():
		var w := get_world_3d()
		if w != null:
			RenderingServer.instance_set_scenario(inst, w.scenario)


func _free_instance(idx: int) -> void:
	var inst := _instance_rid[idx]
	if inst.is_valid():
		RenderingServer.free_rid(inst)
	_instance_rid[idx] = RID()


func _clear_all_instances() -> void:
	for i in range(_instance_rid.size()):
		_free_instance(i)


func _sync_all_scenarios() -> void:
	if not is_inside_tree():
		return
	var w := get_world_3d()
	if w == null:
		return
	for inst in _instance_rid:
		if inst.is_valid():
			RenderingServer.instance_set_scenario(inst, w.scenario)


# --- Internal: transforms ---------------------------------------------------

func _mark_all_dirty() -> void:
	for i in range(_dirty.size()):
		if _dirty[i] == 0:
			_dirty[i] = 1
			_dirty_queue.append(i)


func _mark_dirty_subtree(idx: int) -> void:
	# Mark idx and all descendants dirty (iterative DFS).
	var stack := PackedInt32Array([idx])
	while not stack.is_empty():
		var n: int = stack[stack.size() - 1]
		stack.resize(stack.size() - 1)
		if _dirty[n] == 0:
			_dirty[n] = 1
			_dirty_queue.append(n)
		var c := _first_child[n]
		while c != -1:
			stack.append(c)
			c = _next_sibling[c]


func _update_subtree_world_and_rid(root_idx: int, parent_world: Transform3D) -> void:
	# DFS stack of pairs encoded as two parallel arrays for speed.
	var stack_nodes := PackedInt32Array()
	var stack_parent_world: Array[Transform3D] = []
	stack_nodes.append(root_idx)
	stack_parent_world.append(parent_world)

	while not stack_nodes.is_empty():
		var idx: int = stack_nodes[stack_nodes.size() - 1]
		stack_nodes.resize(stack_nodes.size() - 1)
		var pworld: Transform3D = stack_parent_world[stack_parent_world.size() - 1]
		stack_parent_world.resize(stack_parent_world.size() - 1)

		# Compose world transform
		var wxf: Transform3D = pworld * _local[idx]
		_world[idx] = wxf
		_dirty[idx] = 0

		var inst := _instance_rid[idx]
		if inst.is_valid():
			RenderingServer.instance_set_transform(inst, wxf)

		# Push children
		var c := _first_child[idx]
		while c != -1:
			stack_nodes.append(c)
			stack_parent_world.append(wxf)
			c = _next_sibling[c]


# --- Proxy -----------------------------------------------------------------

func _get_proxy_for_index(idx: int) -> RefCounted:
	var key := StringName(_index_to_path[idx])
	var existing: Variant = _proxy_cache.get(key)
	if existing != null:
		return existing
	var p := _CompoundPartProxy.new(self, idx)
	_proxy_cache[key] = p
	return p


class _CompoundPartProxy extends RefCounted:
	var _owner: CompoundMeshInstance3D
	var _idx: int

	func _init(owner: CompoundMeshInstance3D, idx: int) -> void:
		_owner = owner
		_idx = idx

	func _get_property_list() -> Array:
		return [
			{ name = &"name", type = TYPE_STRING_NAME, usage = PROPERTY_USAGE_READ_ONLY },
			{ name = &"position", type = TYPE_VECTOR3 },
			{ name = &"rotation", type = TYPE_VECTOR3 },
			{ name = &"rotation_degrees", type = TYPE_VECTOR3 },
			{ name = &"scale", type = TYPE_VECTOR3 },
			{ name = &"transform", type = TYPE_TRANSFORM3D },
			{ name = &"global_transform", type = TYPE_TRANSFORM3D },
			{ name = &"global_position", type = TYPE_VECTOR3 },
			{ name = &"visible", type = TYPE_BOOL },
		]

	func _get(property: StringName) -> Variant:
		if _owner == null:
			return null
		match property:
			&"name":
				return StringName(_owner._index_to_path[_idx].get_file())
			&"position":
				return _owner._local[_idx].origin
			&"rotation":
				return _owner._local[_idx].basis.get_euler()
			&"rotation_degrees":
				return _owner._local[_idx].basis.get_euler() * (180.0 / PI)
			&"scale":
				return _owner._local[_idx].basis.get_scale()
			&"transform":
				return _owner._local[_idx]
			&"global_transform":
				return _owner.get_part_global_transform(StringName(_owner._index_to_path[_idx]))
			&"global_position":
				return _owner.get_part_global_transform(StringName(_owner._index_to_path[_idx])).origin
			&"visible":
				return _owner._visible[_idx] == 1
		return null

	func _set(property: StringName, value: Variant) -> bool:
		if _owner == null:
			return false
		var t := _owner._local[_idx]
		match property:
			&"position":
				t.origin = value
				_owner._local[_idx] = t
				_owner._mark_dirty_subtree(_idx)
				_owner._maybe_auto_flush()
				return true
			&"rotation":
				var scale := t.basis.get_scale()
				t.basis = Basis.from_euler(value).scaled(scale)
				_owner._local[_idx] = t
				_owner._mark_dirty_subtree(_idx)
				_owner._maybe_auto_flush()
				return true
			&"rotation_degrees":
				var scale2 := t.basis.get_scale()
				t.basis = Basis.from_euler(value * (PI / 180.0)).scaled(scale2)
				_owner._local[_idx] = t
				_owner._mark_dirty_subtree(_idx)
				_owner._maybe_auto_flush()
				return true
			&"scale":
				var rot := t.basis.orthonormalized()
				t.basis = rot.scaled(value)
				_owner._local[_idx] = t
				_owner._mark_dirty_subtree(_idx)
				_owner._maybe_auto_flush()
				return true
			&"transform":
				_owner._local[_idx] = value
				_owner._mark_dirty_subtree(_idx)
				_owner._maybe_auto_flush()
				return true
			&"global_transform":
				_owner._set_part_global_transform(_idx, value)
				_owner._maybe_auto_flush()
				return true
			&"global_position":
				var gt := _owner.get_part_global_transform(StringName(_owner._index_to_path[_idx]))
				gt.origin = value
				_owner._set_part_global_transform(_idx, gt)
				_owner._maybe_auto_flush()
				return true
			&"visible":
				_owner._visible[_idx] = 1 if bool(value) else 0
				var inst := _owner._instance_rid[_idx]
				if inst.is_valid():
					RenderingServer.instance_set_visible(inst, _owner._visible[_idx] == 1)
				return true
		return false

	func get_children() -> Array:
		if _owner == null:
			return []
		var out: Array = []
		var c := _owner._first_child[_idx]
		while c != -1:
			out.append(_owner._get_proxy_for_index(c))
			c = _owner._next_sibling[c]
		return out

	func get_parent() -> Variant:
		if _owner == null:
			return null
		var p := _owner._parent[_idx]
		if p == -1:
			return _owner
		return _owner._get_proxy_for_index(p)


func _maybe_auto_flush() -> void:
	if auto_flush and _in_batch == 0:
		flush()


func _set_part_cast_shadows_index(idx: int, cast_shadows: int) -> void:
	if idx < 0 or idx >= _cast_shadows.size():
		return
	_cast_shadows[idx] = cast_shadows
	var inst := _instance_rid[idx]
	if inst.is_valid():
		RenderingServer.instance_geometry_set_cast_shadows_setting(inst, cast_shadows)


func _set_part_global_transform(idx: int, gt: Transform3D) -> void:
	var p := _parent[idx]
	var parent_world: Transform3D
	if p == -1:
		parent_world = global_transform if is_inside_tree() else Transform3D.IDENTITY
	else:
		# Ensure parent world is up-to-date.
		if _dirty[p] == 1:
			flush()
		parent_world = _world[p]
	_local[idx] = parent_world.affine_inverse() * gt
	_mark_dirty_subtree(idx)