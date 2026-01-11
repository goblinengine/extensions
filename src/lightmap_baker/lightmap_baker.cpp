#include "lightmap_baker.h"

#include <xatlas.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/image_texture_layered.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>

#include <algorithm>
#include <cmath>

#include <mutex>
#include <unordered_map>

namespace godot {

namespace {

static inline uint64_t _lm_mix64(uint64_t x) {
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 33;
	return x;
}

struct _LM_UnwrapCacheKey {
	uint64_t a = 0;
	uint64_t b = 0;
	bool operator==(const _LM_UnwrapCacheKey &o) const { return a == o.a && b == o.b; }
};

struct _LM_UnwrapCacheKeyHash {
	size_t operator()(const _LM_UnwrapCacheKey &k) const {
		uint64_t x = k.a ^ (k.b + 0x9e3779b97f4a7c15ULL + (k.a << 6) + (k.a >> 2));
		return (size_t)_lm_mix64(x);
	}
};

struct _LM_UnwrapCacheEntry {
	PackedInt32Array xrefs;
	PackedVector2Array uv2;
	PackedInt32Array indices;
	Vector2i size_hint;
};

static std::mutex _lm_unwrap_cache_mutex;
static std::unordered_map<_LM_UnwrapCacheKey, _LM_UnwrapCacheEntry, _LM_UnwrapCacheKeyHash> _lm_unwrap_cache;

static PackedInt32Array _lm_build_triangle_indices(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices) {
	if (!p_indices.is_empty()) {
		return p_indices;
	}
	const int vertex_count = p_vertices.size();
	if (vertex_count < 3 || (vertex_count % 3) != 0) {
		return PackedInt32Array();
	}
	PackedInt32Array indices;
	indices.resize(vertex_count);
	for (int i = 0; i < vertex_count; i++) {
		indices.set(i, i);
	}
	return indices;
}

static PackedVector3Array _lm_compute_vertex_normals(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices) {
	const int vertex_count = p_vertices.size();
	PackedVector3Array normals;
	normals.resize(vertex_count);
	for (int i = 0; i < vertex_count; i++) {
		normals.set(i, Vector3());
	}

	const int index_count = p_indices.size();
	for (int i = 0; i + 2 < index_count; i += 3) {
		const int i0 = p_indices[i + 0];
		const int i1 = p_indices[i + 1];
		const int i2 = p_indices[i + 2];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
			continue;
		}
		const Vector3 v0 = p_vertices[i0];
		const Vector3 v1 = p_vertices[i1];
		const Vector3 v2 = p_vertices[i2];
		const Vector3 n = (v1 - v0).cross(v2 - v0);
		normals.set(i0, normals[i0] + n);
		normals.set(i1, normals[i1] + n);
		normals.set(i2, normals[i2] + n);
	}

	for (int i = 0; i < vertex_count; i++) {
		Vector3 n = normals[i];
		const float len = n.length();
		if (len > 1e-8f) {
			normals.set(i, n / len);
		} else {
			normals.set(i, Vector3(0, 1, 0));
		}
	}
	return normals;
}

static void _lm_remap_surface_attributes_by_xref(Array &p_dst_arrays, const Array &p_src_arrays, const PackedInt32Array &p_xrefs, int p_original_vertex_count) {
	const int new_vcount = p_xrefs.size();
	if (new_vcount <= 0 || p_original_vertex_count <= 0) {
		return;
	}
	if (p_dst_arrays.size() < Mesh::ARRAY_MAX || p_src_arrays.size() < Mesh::ARRAY_MAX) {
		return;
	}

	// UV1
	{
		PackedVector2Array uv = p_src_arrays[Mesh::ARRAY_TEX_UV];
		if (uv.size() == p_original_vertex_count) {
			PackedVector2Array out;
			out.resize(new_vcount);
			for (int i = 0; i < new_vcount; i++) {
				const int xref = p_xrefs[i];
				out.set(i, (xref >= 0 && xref < p_original_vertex_count) ? uv[xref] : Vector2());
			}
			p_dst_arrays[Mesh::ARRAY_TEX_UV] = out;
		}
	}

	// Vertex colors
	{
		PackedColorArray col = p_src_arrays[Mesh::ARRAY_COLOR];
		if (col.size() == p_original_vertex_count) {
			PackedColorArray out;
			out.resize(new_vcount);
			for (int i = 0; i < new_vcount; i++) {
				const int xref = p_xrefs[i];
				out.set(i, (xref >= 0 && xref < p_original_vertex_count) ? col[xref] : Color(1, 1, 1, 1));
			}
			p_dst_arrays[Mesh::ARRAY_COLOR] = out;
		}
	}

	// Tangents (4 floats per vertex)
	{
		PackedFloat32Array tan = p_src_arrays[Mesh::ARRAY_TANGENT];
		if (tan.size() == p_original_vertex_count * 4) {
			PackedFloat32Array out;
			out.resize(new_vcount * 4);
			for (int i = 0; i < new_vcount; i++) {
				const int xref = p_xrefs[i];
				const int src = (xref >= 0 && xref < p_original_vertex_count) ? (xref * 4) : 0;
				const int dst = i * 4;
				out.set(dst + 0, tan[src + 0]);
				out.set(dst + 1, tan[src + 1]);
				out.set(dst + 2, tan[src + 2]);
				out.set(dst + 3, tan[src + 3]);
			}
			p_dst_arrays[Mesh::ARRAY_TANGENT] = out;
		}
	}
}

static bool _lm_xatlas_unwrap(float p_texel_size,
		const PackedVector3Array &p_positions_for_unwrap,
		const PackedVector3Array &p_normals_for_unwrap,
		const PackedInt32Array &p_tri_indices,
		PackedInt32Array &r_out_xrefs,
		PackedVector2Array &r_out_uv2,
		PackedInt32Array &r_out_indices,
		Vector2i &r_out_size_hint) {
	const int vertex_count = p_positions_for_unwrap.size();
	if (vertex_count < 3) {
		return false;
	}
	if (p_tri_indices.is_empty() || (p_tri_indices.size() % 3) != 0) {
		return false;
	}
	ERR_FAIL_COND_V_MSG(p_texel_size <= 0.0f, false, "Texel size must be greater than 0.");

	uint64_t tex_bits = 0;
	static_assert(sizeof(float) == sizeof(uint32_t), "unexpected float size");
	memcpy(&tex_bits, &p_texel_size, sizeof(uint32_t));
	const uint64_t a = (uint64_t)(uintptr_t)p_positions_for_unwrap.ptr() ^ ((uint64_t)vertex_count << 32) ^ tex_bits;
	const uint64_t b = (uint64_t)(uintptr_t)p_tri_indices.ptr() ^ ((uint64_t)p_tri_indices.size() << 32) ^ (uint64_t)(uintptr_t)p_normals_for_unwrap.ptr();
	_LM_UnwrapCacheKey key;
	key.a = _lm_mix64(a);
	key.b = _lm_mix64(b);
	{
		std::lock_guard<std::mutex> lock(_lm_unwrap_cache_mutex);
		auto it = _lm_unwrap_cache.find(key);
		if (it != _lm_unwrap_cache.end()) {
			r_out_xrefs = it->second.xrefs;
			r_out_uv2 = it->second.uv2;
			r_out_indices = it->second.indices;
			r_out_size_hint = it->second.size_hint;
			return true;
		}
	}

	// xatlas expects float triples. In float builds, Vector3 is 3 floats so we can pass it directly.
	const void *pos_ptr = nullptr;
	const void *nrm_ptr = nullptr;
	uint32_t pos_stride = 0;
	uint32_t nrm_stride = 0;

#ifdef REAL_T_IS_DOUBLE
	thread_local std::vector<float> pos_f;
	thread_local std::vector<float> nrm_f;
	pos_f.resize((size_t)vertex_count * 3);
	nrm_f.resize((size_t)vertex_count * 3);
	for (int i = 0; i < vertex_count; i++) {
		const Vector3 v = p_positions_for_unwrap[i];
		pos_f[(size_t)i * 3 + 0] = (float)v.x;
		pos_f[(size_t)i * 3 + 1] = (float)v.y;
		pos_f[(size_t)i * 3 + 2] = (float)v.z;
		const Vector3 n = p_normals_for_unwrap[i];
		nrm_f[(size_t)i * 3 + 0] = (float)n.x;
		nrm_f[(size_t)i * 3 + 1] = (float)n.y;
		nrm_f[(size_t)i * 3 + 2] = (float)n.z;
	}
	pos_ptr = pos_f.data();
	nrm_ptr = nrm_f.data();
	pos_stride = sizeof(float) * 3;
	nrm_stride = sizeof(float) * 3;
#else
	static_assert(sizeof(Vector3) == sizeof(float) * 3, "Vector3 must be 3 floats in float builds");
	pos_ptr = p_positions_for_unwrap.ptr();
	nrm_ptr = p_normals_for_unwrap.ptr();
	pos_stride = sizeof(Vector3);
	nrm_stride = sizeof(Vector3);
#endif

	xatlas::MeshDecl input_mesh;
	input_mesh.indexData = p_tri_indices.ptr();
	input_mesh.indexCount = (uint32_t)p_tri_indices.size();
	input_mesh.indexFormat = xatlas::IndexFormat::UInt32;
	input_mesh.vertexCount = (uint32_t)vertex_count;
	input_mesh.vertexPositionData = pos_ptr;
	input_mesh.vertexPositionStride = pos_stride;
	input_mesh.vertexNormalData = nrm_ptr;
	input_mesh.vertexNormalStride = nrm_stride;
	input_mesh.vertexUvData = nullptr;
	input_mesh.vertexUvStride = 0;

	xatlas::ChartOptions chart_options;
	chart_options.fixWinding = true;

	xatlas::PackOptions pack_options;
	pack_options.padding = 1;
	pack_options.maxChartSize = 4094;
	pack_options.blockAlign = true;
	pack_options.texelsPerUnit = 1.0f / p_texel_size;

	xatlas::Atlas *atlas = xatlas::Create();
	if (!atlas) {
		return false;
	}

	xatlas::AddMeshError err = xatlas::AddMesh(atlas, input_mesh, 1);
	if (err != xatlas::AddMeshError::Success) {
		xatlas::Destroy(atlas);
		return false;
	}

	xatlas::Generate(atlas, chart_options, pack_options);

	const int w = (int)atlas->width;
	const int h = (int)atlas->height;
	r_out_size_hint = Vector2i(w, h);
	if (w == 0 || h == 0) {
		xatlas::Destroy(atlas);
		return false;
	}

	const xatlas::Mesh &output = atlas->meshes[0];
	if (output.vertexCount == 0 || output.indexCount == 0) {
		xatlas::Destroy(atlas);
		return false;
	}

	r_out_xrefs.resize((int)output.vertexCount);
	r_out_uv2.resize((int)output.vertexCount);
	r_out_indices.resize((int)output.indexCount);
	for (uint32_t i = 0; i < output.vertexCount; i++) {
		r_out_xrefs.set((int)i, (int)output.vertexArray[i].xref);
		// Normalize UVs to 0..1 by atlas dimensions, like Godot.
		const float u = output.vertexArray[i].uv[0] / (float)w;
		const float v = output.vertexArray[i].uv[1] / (float)h;
		r_out_uv2.set((int)i, Vector2(u, v));
	}
	for (uint32_t i = 0; i < output.indexCount; i++) {
		r_out_indices.set((int)i, (int)output.indexArray[i]);
	}

	xatlas::Destroy(atlas);

	{
		_LM_UnwrapCacheEntry entry;
		entry.xrefs = r_out_xrefs;
		entry.uv2 = r_out_uv2;
		entry.indices = r_out_indices;
		entry.size_hint = r_out_size_hint;
		std::lock_guard<std::mutex> lock(_lm_unwrap_cache_mutex);
		_lm_unwrap_cache.emplace(key, entry);
	}

	return true;
}

} // namespace

struct _LM_RayTri {
	Vector3 a;
	Vector3 b;
	Vector3 c;
};

struct LightmapBaker::RayMesh {
	AABB aabb;
	std::vector<_LM_RayTri> tris;
};

LightmapBaker::LightmapBaker() {
	// Read project settings as defaults (can be overridden per-bake)
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps != nullptr) {
		// Bake quality: rendering/lightmapping/bake_quality/<low|medium|high|ultra>_quality_ray_count
		// Default to medium quality if no settings found
		if (ps->has_setting("rendering/lightmapping/bake_quality/medium_quality_ray_count")) {
			int ray_count = (int)ps->get_setting("rendering/lightmapping/bake_quality/medium_quality_ray_count");
			// Map ray count to quality level (approximate)
			if (ray_count <= 64) {
				bake_quality = BAKE_QUALITY_LOW;
			} else if (ray_count <= 256) {
				bake_quality = BAKE_QUALITY_MEDIUM;
			} else if (ray_count <= 1024) {
				bake_quality = BAKE_QUALITY_HIGH;
			} else {
				bake_quality = BAKE_QUALITY_ULTRA;
			}
		}

		// Bounces: rendering/lightmapping/bake_quality/high_quality_probe_ray_count maps to indirect
		// Actually use rendering/global_illumination/gi/use_half_resolution as a quality hint
		if (ps->has_setting("rendering/lightmapping/primitive_meshes/texel_size")) {
			float texel = (float)ps->get_setting("rendering/lightmapping/primitive_meshes/texel_size");
			// texel_size is the default size, we use it inversely
			texel_scale = 1.0f / Math::max(0.01f, texel * 5.0f); // Approximate mapping
		}
	}
}

LightmapBaker::~LightmapBaker() {
	gathered_meshes.clear();
	gathered_lights.clear();
}

void LightmapBaker::_bind_methods() {
	// Configuration setters/getters
	ClassDB::bind_method(D_METHOD("set_bake_quality", "quality"), &LightmapBaker::set_bake_quality);
	ClassDB::bind_method(D_METHOD("get_bake_quality"), &LightmapBaker::get_bake_quality);

	ClassDB::bind_method(D_METHOD("set_bounces", "bounces"), &LightmapBaker::set_bounces);
	ClassDB::bind_method(D_METHOD("get_bounces"), &LightmapBaker::get_bounces);

	ClassDB::bind_method(D_METHOD("set_bounce_indirect_energy", "energy"), &LightmapBaker::set_bounce_indirect_energy);
	ClassDB::bind_method(D_METHOD("get_bounce_indirect_energy"), &LightmapBaker::get_bounce_indirect_energy);

	ClassDB::bind_method(D_METHOD("set_bias", "bias"), &LightmapBaker::set_bias);
	ClassDB::bind_method(D_METHOD("get_bias"), &LightmapBaker::get_bias);

	ClassDB::bind_method(D_METHOD("set_max_texture_size", "size"), &LightmapBaker::set_max_texture_size);
	ClassDB::bind_method(D_METHOD("get_max_texture_size"), &LightmapBaker::get_max_texture_size);

	ClassDB::bind_method(D_METHOD("set_atlas_size_override", "size"), &LightmapBaker::set_atlas_size_override);
	ClassDB::bind_method(D_METHOD("get_atlas_size_override"), &LightmapBaker::get_atlas_size_override);

	ClassDB::bind_method(D_METHOD("set_atlas_padding", "padding"), &LightmapBaker::set_atlas_padding);
	ClassDB::bind_method(D_METHOD("get_atlas_padding"), &LightmapBaker::get_atlas_padding);

	ClassDB::bind_method(D_METHOD("set_seam_dilation_radius", "radius"), &LightmapBaker::set_seam_dilation_radius);
	ClassDB::bind_method(D_METHOD("get_seam_dilation_radius"), &LightmapBaker::get_seam_dilation_radius);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seam_dilation_radius", PROPERTY_HINT_RANGE, "0,8,1"), "set_seam_dilation_radius", "get_seam_dilation_radius");

	ClassDB::bind_method(D_METHOD("set_texel_scale", "scale"), &LightmapBaker::set_texel_scale);
	ClassDB::bind_method(D_METHOD("get_texel_scale"), &LightmapBaker::get_texel_scale);

	ClassDB::bind_method(D_METHOD("set_lightmap_energy_scale", "scale"), &LightmapBaker::set_lightmap_energy_scale);
	ClassDB::bind_method(D_METHOD("get_lightmap_energy_scale"), &LightmapBaker::get_lightmap_energy_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lightmap_energy_scale", PROPERTY_HINT_RANGE, "0,8,0.01"), "set_lightmap_energy_scale", "get_lightmap_energy_scale");

	ClassDB::bind_method(D_METHOD("set_ambient_energy", "energy"), &LightmapBaker::set_ambient_energy);
	ClassDB::bind_method(D_METHOD("get_ambient_energy"), &LightmapBaker::get_ambient_energy);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_energy", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_ambient_energy", "get_ambient_energy");

	ClassDB::bind_method(D_METHOD("set_use_material_albedo", "enabled"), &LightmapBaker::set_use_material_albedo);
	ClassDB::bind_method(D_METHOD("get_use_material_albedo"), &LightmapBaker::get_use_material_albedo);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_material_albedo"), "set_use_material_albedo", "get_use_material_albedo");

	ClassDB::bind_method(D_METHOD("set_use_lambert_normalization", "enabled"), &LightmapBaker::set_use_lambert_normalization);
	ClassDB::bind_method(D_METHOD("get_use_lambert_normalization"), &LightmapBaker::get_use_lambert_normalization);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_lambert_normalization"), "set_use_lambert_normalization", "get_use_lambert_normalization");

	ClassDB::bind_method(D_METHOD("set_use_denoiser", "enabled"), &LightmapBaker::set_use_denoiser);
	ClassDB::bind_method(D_METHOD("get_use_denoiser"), &LightmapBaker::get_use_denoiser);

	ClassDB::bind_method(D_METHOD("set_denoiser_strength", "strength"), &LightmapBaker::set_denoiser_strength);
	ClassDB::bind_method(D_METHOD("get_denoiser_strength"), &LightmapBaker::get_denoiser_strength);

	ClassDB::bind_method(D_METHOD("set_use_shadowing", "enabled"), &LightmapBaker::set_use_shadowing);
	ClassDB::bind_method(D_METHOD("get_use_shadowing"), &LightmapBaker::get_use_shadowing);

	ClassDB::bind_method(D_METHOD("set_auto_unwrap_uv2", "enabled"), &LightmapBaker::set_auto_unwrap_uv2);
	ClassDB::bind_method(D_METHOD("get_auto_unwrap_uv2"), &LightmapBaker::get_auto_unwrap_uv2);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_unwrap_uv2"), "set_auto_unwrap_uv2", "get_auto_unwrap_uv2");

	ClassDB::bind_method(D_METHOD("set_mesh_layer_mask", "mask"), &LightmapBaker::set_mesh_layer_mask);
	ClassDB::bind_method(D_METHOD("get_mesh_layer_mask"), &LightmapBaker::get_mesh_layer_mask);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_layer_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_mesh_layer_mask", "get_mesh_layer_mask");

	// Main bake methods
	ClassDB::bind_method(D_METHOD("bake", "from_node", "output_data"), &LightmapBaker::bake);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("lightmap_unwrap", "mesh", "transform", "texel_size"), &LightmapBaker::lightmap_unwrap, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_gathered_mesh_count"), &LightmapBaker::get_gathered_mesh_count);
	ClassDB::bind_method(D_METHOD("get_gathered_light_count"), &LightmapBaker::get_gathered_light_count);

	// Enums
	BIND_ENUM_CONSTANT(BAKE_QUALITY_LOW);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_HIGH);
	BIND_ENUM_CONSTANT(BAKE_QUALITY_ULTRA);

	BIND_ENUM_CONSTANT(BAKE_ERROR_OK);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_SCENE_ROOT);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_LIGHTMAPPER);
	BIND_ENUM_CONSTANT(BAKE_ERROR_NO_MESHES);
	BIND_ENUM_CONSTANT(BAKE_ERROR_MESHES_INVALID);
	BIND_ENUM_CONSTANT(BAKE_ERROR_CANT_CREATE_IMAGE);
	BIND_ENUM_CONSTANT(BAKE_ERROR_USER_ABORTED);
	BIND_ENUM_CONSTANT(BAKE_ERROR_TEXTURE_SIZE_TOO_SMALL);
	BIND_ENUM_CONSTANT(BAKE_ERROR_LIGHTMAP_TOO_SMALL);
	BIND_ENUM_CONSTANT(BAKE_ERROR_ATLAS_TOO_SMALL);
}

int LightmapBaker::lightmap_unwrap(const Ref<ArrayMesh> &p_mesh, const Transform3D &p_transform, float p_texel_size) {
	if (p_mesh.is_null()) {
		UtilityFunctions::push_error("LightmapBaker::lightmap_unwrap: mesh is null");
		return ERR_INVALID_PARAMETER;
	}

	float texel_size = p_texel_size;
	if (!(texel_size > 0.0f)) {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		if (ps != nullptr && ps->has_setting("rendering/lightmapping/primitive_meshes/texel_size")) {
			texel_size = (float)ps->get_setting("rendering/lightmapping/primitive_meshes/texel_size");
		}
		if (!(texel_size > 0.0f)) {
			texel_size = 0.1f;
		}
	}

	struct _SurfaceTmp {
		Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_TRIANGLES;
		Array arrays;
		Ref<Material> material;
		String name;
	};
	std::vector<_SurfaceTmp> rebuilt;
	rebuilt.reserve((size_t)p_mesh->get_surface_count());

	const bool is_identity = p_transform.is_equal_approx(Transform3D());
	Vector2i computed_size_hint;

	for (int surface_idx = 0; surface_idx < p_mesh->get_surface_count(); surface_idx++) {
		Array arrays = p_mesh->surface_get_arrays(surface_idx);
		if (arrays.is_empty() || arrays.size() < Mesh::ARRAY_MAX) {
			continue;
		}
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		PackedVector2Array uv2_existing = arrays[Mesh::ARRAY_TEX_UV2];
		if (vertices.is_empty()) {
			continue;
		}
		if (!uv2_existing.is_empty() && uv2_existing.size() == vertices.size()) {
			_SurfaceTmp tmp;
			tmp.primitive = p_mesh->surface_get_primitive_type(surface_idx);
			tmp.arrays = arrays;
			tmp.material = p_mesh->surface_get_material(surface_idx);
			tmp.name = p_mesh->surface_get_name(surface_idx);
			rebuilt.push_back(tmp);
			continue;
		}

		PackedInt32Array tri_indices = _lm_build_triangle_indices(vertices, indices);
		if (tri_indices.is_empty() || (tri_indices.size() % 3) != 0) {
			continue;
		}
		if (normals.size() != vertices.size()) {
			normals = _lm_compute_vertex_normals(vertices, tri_indices);
		}

		PackedVector3Array unwrap_pos;
		PackedVector3Array unwrap_nrm;
		const PackedVector3Array *pos_for_unwrap = &vertices;
		const PackedVector3Array *nrm_for_unwrap = &normals;
		if (!is_identity) {
			unwrap_pos.resize(vertices.size());
			unwrap_nrm.resize(normals.size());
			Basis nxf = p_transform.basis.inverse().transposed();
			for (int i = 0; i < vertices.size(); i++) {
				unwrap_pos.set(i, p_transform.xform(vertices[i]));
				unwrap_nrm.set(i, nxf.xform(normals[i]).normalized());
			}
			pos_for_unwrap = &unwrap_pos;
			nrm_for_unwrap = &unwrap_nrm;
		}

		PackedInt32Array out_xrefs;
		PackedVector2Array out_uv2;
		PackedInt32Array out_indices;
		Vector2i out_hint;
		if (!_lm_xatlas_unwrap(texel_size, *pos_for_unwrap, *nrm_for_unwrap, tri_indices, out_xrefs, out_uv2, out_indices, out_hint)) {
			continue;
		}
		computed_size_hint.x = MAX(computed_size_hint.x, out_hint.x);
		computed_size_hint.y = MAX(computed_size_hint.y, out_hint.y);

		Array surface_arrays;
		surface_arrays.resize(Mesh::ARRAY_MAX);
		PackedVector3Array out_vertices;
		PackedVector3Array out_normals;
		out_vertices.resize(out_xrefs.size());
		out_normals.resize(out_xrefs.size());
		for (int i = 0; i < out_xrefs.size(); i++) {
			const int xref = out_xrefs[i];
			out_vertices.set(i, (xref >= 0 && xref < vertices.size()) ? vertices[xref] : Vector3());
			out_normals.set(i, (xref >= 0 && xref < normals.size()) ? normals[xref] : Vector3(0, 1, 0));
		}
		surface_arrays[Mesh::ARRAY_VERTEX] = out_vertices;
		surface_arrays[Mesh::ARRAY_NORMAL] = out_normals;
		surface_arrays[Mesh::ARRAY_TEX_UV2] = out_uv2;
		surface_arrays[Mesh::ARRAY_INDEX] = out_indices;
		_lm_remap_surface_attributes_by_xref(surface_arrays, arrays, out_xrefs, vertices.size());

		_SurfaceTmp tmp;
		tmp.primitive = p_mesh->surface_get_primitive_type(surface_idx);
		tmp.arrays = surface_arrays;
		tmp.material = p_mesh->surface_get_material(surface_idx);
		tmp.name = p_mesh->surface_get_name(surface_idx);
		rebuilt.push_back(tmp);
	}

	if (rebuilt.empty()) {
		return ERR_UNAVAILABLE;
	}

	p_mesh->clear_surfaces();
	for (size_t i = 0; i < rebuilt.size(); i++) {
		p_mesh->add_surface_from_arrays(rebuilt[i].primitive, rebuilt[i].arrays);
		p_mesh->surface_set_material((int)i, rebuilt[i].material);
		if (!rebuilt[i].name.is_empty()) {
			p_mesh->surface_set_name((int)i, rebuilt[i].name);
		}
	}

	// Propagate the unwrap size hint (critical for downstream baking/atlas packing).
	if (computed_size_hint.x > 0 && computed_size_hint.y > 0) {
		Vector2i existing_hint = p_mesh->get_lightmap_size_hint();
		if (existing_hint.x <= 0 || existing_hint.y <= 0) {
			p_mesh->set_lightmap_size_hint(computed_size_hint);
		} else {
			p_mesh->set_lightmap_size_hint(Vector2i(MAX(existing_hint.x, computed_size_hint.x), MAX(existing_hint.y, computed_size_hint.y)));
		}
	}

	return OK;
}

// Configuration methods
void LightmapBaker::set_bake_quality(BakeQuality p_quality) {
	bake_quality = p_quality;
}

LightmapBaker::BakeQuality LightmapBaker::get_bake_quality() const {
	return bake_quality;
}

void LightmapBaker::set_bounces(int p_bounces) {
	bounces = p_bounces;
}

int LightmapBaker::get_bounces() const {
	return bounces;
}

void LightmapBaker::set_bounce_indirect_energy(float p_energy) {
	bounce_indirect_energy = p_energy;
}

float LightmapBaker::get_bounce_indirect_energy() const {
	return bounce_indirect_energy;
}

void LightmapBaker::set_mesh_layer_mask(uint32_t p_mask) {
	mesh_layer_mask = p_mask;
}

uint32_t LightmapBaker::get_mesh_layer_mask() const {
	return mesh_layer_mask;
}

void LightmapBaker::set_bias(float p_bias) {
	bias = p_bias;
}

float LightmapBaker::get_bias() const {
	return bias;
}

void LightmapBaker::set_max_texture_size(int p_size) {
	max_texture_size = p_size;
}

int LightmapBaker::get_max_texture_size() const {
	return max_texture_size;
}

void LightmapBaker::set_atlas_size_override(int p_size) {
	atlas_size_override = p_size;
}

int LightmapBaker::get_atlas_size_override() const {
	return atlas_size_override;
}

void LightmapBaker::set_atlas_padding(int p_padding) {
	atlas_padding = p_padding;
}

int LightmapBaker::get_atlas_padding() const {
	return atlas_padding;
}

void LightmapBaker::set_seam_dilation_radius(int p_radius) {
	seam_dilation_radius = p_radius;
}

int LightmapBaker::get_seam_dilation_radius() const {
	return seam_dilation_radius;
}

void LightmapBaker::set_texel_scale(float p_scale) {
	texel_scale = p_scale;
}

float LightmapBaker::get_texel_scale() const {
	return texel_scale;
}

void LightmapBaker::set_lightmap_energy_scale(float p_scale) {
	lightmap_energy_scale = p_scale;
}

float LightmapBaker::get_lightmap_energy_scale() const {
	return lightmap_energy_scale;
}

void LightmapBaker::set_ambient_energy(float p_energy) {
	ambient_energy = p_energy;
}

float LightmapBaker::get_ambient_energy() const {
	return ambient_energy;
}

void LightmapBaker::set_use_material_albedo(bool p_enabled) {
	use_material_albedo = p_enabled;
}

bool LightmapBaker::get_use_material_albedo() const {
	return use_material_albedo;
}

void LightmapBaker::set_use_lambert_normalization(bool p_enabled) {
	use_lambert_normalization = p_enabled;
}

bool LightmapBaker::get_use_lambert_normalization() const {
	return use_lambert_normalization;
}

void LightmapBaker::set_use_denoiser(bool p_enabled) {
	use_denoiser = p_enabled;
}

bool LightmapBaker::get_use_denoiser() const {
	return use_denoiser;
}

void LightmapBaker::set_denoiser_strength(float p_strength) {
	denoiser_strength = p_strength;
}

float LightmapBaker::get_denoiser_strength() const {
	return denoiser_strength;
}

void LightmapBaker::set_use_shadowing(bool p_enabled) {
	use_shadowing = p_enabled;
}

bool LightmapBaker::get_use_shadowing() const {
	return use_shadowing;
}

void LightmapBaker::set_auto_unwrap_uv2(bool p_enabled) {
	auto_unwrap_uv2 = p_enabled;
}

bool LightmapBaker::get_auto_unwrap_uv2() const {
	return auto_unwrap_uv2;
}

// Main bake entry point
LightmapBaker::BakeError LightmapBaker::bake(Node *p_from_node, Ref<LightmapGIData> p_output_data) {
	return bake_with_progress(p_from_node, p_output_data, nullptr, nullptr);
}

LightmapBaker::BakeError LightmapBaker::bake_with_progress(Node *p_from_node, Ref<LightmapGIData> p_output_data,
														   BakeProgressFunc p_progress_func, void *p_userdata) {
	if (p_from_node == nullptr) {
		return BAKE_ERROR_NO_SCENE_ROOT;
	}

	if (p_output_data.is_null()) {
		UtilityFunctions::push_error("LightmapBaker: output LightmapGIData is null");
		return BAKE_ERROR_NO_MESHES;
	}

	// Clear previous data
	gathered_meshes.clear();
	gathered_lights.clear();
	ray_meshes.clear();

	_report_progress(0.0f, "Gathering meshes and lights...", p_progress_func, p_userdata);

	// Gather geometry and lights from scene
	_find_meshes_and_lights(p_from_node, gathered_meshes, gathered_lights);

	if (gathered_meshes.empty()) {
		UtilityFunctions::push_error("No meshes with lightmap UV2 found in scene");
		return BAKE_ERROR_NO_MESHES;
	}

	UtilityFunctions::print("Found ", (int)gathered_meshes.size(), " meshes with lightmap UVs");
	UtilityFunctions::print("Found ", (int)gathered_lights.size(), " lights");

	// Validate meshes
	if (!_validate_meshes(gathered_meshes)) {
		return BAKE_ERROR_MESHES_INVALID;
	}

	_report_progress(0.1f, "Baking direct lighting...", p_progress_func, p_userdata);

	// Phase 1: Direct lighting
	BakeError error = _bake_direct_light(p_output_data, p_progress_func, p_userdata);
	if (error != BAKE_ERROR_OK) {
		return error;
	}

	_report_progress(0.9f, "Finalizing lightmaps...", p_progress_func, p_userdata);

	_report_progress(1.0f, "Bake complete!", p_progress_func, p_userdata);

	return BAKE_ERROR_OK;
}

void LightmapBaker::_find_meshes_and_lights(Node *p_at_node, std::vector<MeshData> &r_meshes, std::vector<LightData> &r_lights) {
	if (p_at_node == nullptr) {
		return;
	}

	// Try to cast to MeshInstance3D
	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_at_node);
	if (mesh_instance != nullptr && mesh_instance->is_visible_in_tree()) {
		_process_mesh_instance(mesh_instance, r_meshes);
	}

	// Try to cast to Light3D
	Light3D *light = Object::cast_to<Light3D>(p_at_node);
	if (light != nullptr && light->is_visible_in_tree()) {
		_process_light(light, r_lights);
	}

	// Recursively process children
	for (int i = 0; i < p_at_node->get_child_count(); i++) {
		_find_meshes_and_lights(p_at_node->get_child(i), r_meshes, r_lights);
	}
}

void LightmapBaker::_process_mesh_instance(MeshInstance3D *p_mesh, std::vector<MeshData> &r_meshes) {
	if ((p_mesh->get_layer_mask() & mesh_layer_mask) == 0) {
		return;
	}

	Ref<Mesh> mesh = p_mesh->get_mesh();
	if (mesh.is_null()) {
		return;
	}

	// Check if mesh has UV2 (required for lightmapping)
	auto mesh_has_uv2 = [&]() {
		for (int i = 0; i < mesh->get_surface_count(); i++) {
			Array arrays = mesh->surface_get_arrays(i);
			if (arrays.is_empty()) {
				continue;
			}
			PackedVector2Array uv2 = arrays[Mesh::ARRAY_TEX_UV2];
			if (!uv2.is_empty()) {
				return true;
			}
		}
		return false;
	};

	bool has_uv2 = mesh_has_uv2();
	if (!has_uv2 && auto_unwrap_uv2) {
		Ref<ArrayMesh> array_mesh = mesh;
		if (array_mesh.is_valid()) {
			// NOTE: This modifies the mesh resource in-place.
			// For most runtime chunk meshes that's desired; if you share a mesh across instances,
			// it will gain UV2 everywhere.
			const int err = LightmapBaker::lightmap_unwrap(array_mesh, Transform3D(), 0.0f);
			if (err != OK) {
				UtilityFunctions::push_warning("LightmapBaker: auto_unwrap_uv2 failed for mesh '" + p_mesh->get_name() + "' (err=" + String::num_int64(err) + ")");
			} else {
				has_uv2 = mesh_has_uv2();
			}
		} else {
			UtilityFunctions::push_warning("LightmapBaker: auto_unwrap_uv2 is enabled but mesh '" + p_mesh->get_name() + "' is not an ArrayMesh; skipping");
		}
	}

	if (!has_uv2) {
		UtilityFunctions::print("Mesh ", p_mesh->get_name(), " has no UV2 channel, skipping");
		return;
	}

	// Extract mesh data
	for (int surface_idx = 0; surface_idx < mesh->get_surface_count(); surface_idx++) {
		Array arrays = mesh->surface_get_arrays(surface_idx);
		if (arrays.is_empty()) {
			continue;
		}
		PackedVector2Array uv2s = arrays[Mesh::ARRAY_TEX_UV2];
		if (uv2s.is_empty()) {
			continue;
		}

		MeshData mesh_data;
		mesh_data.vertices = arrays[Mesh::ARRAY_VERTEX];
		mesh_data.normals = arrays[Mesh::ARRAY_NORMAL];
		mesh_data.uv2s = uv2s;
		mesh_data.indices = arrays[Mesh::ARRAY_INDEX];
		mesh_data.transform = p_mesh->get_global_transform();
		mesh_data.owner_node = p_mesh;
		mesh_data.sub_instance = surface_idx;
		mesh_data.lightmap_size_hint = mesh->get_lightmap_size_hint();
		mesh_data.lightmap_slice = 0; // TODO: Handle multiple slices
		mesh_data.lightmap_uv_scale = Rect2(Vector2(0, 0), Vector2(1, 1));

		// Get material for albedo
		Ref<Material> mat = p_mesh->get_surface_override_material(surface_idx);
		if (mat.is_null()) {
			mat = mesh->surface_get_material(surface_idx);
		}
		mesh_data.material = mat;

		r_meshes.push_back(mesh_data);
	}
}

void LightmapBaker::_process_light(Light3D *p_light, std::vector<LightData> &r_lights) {
	if (p_light->get_bake_mode() != Light3D::BAKE_STATIC) {
		return; // Only bake static lights.
	}

	LightData light_data;
	light_data.color = p_light->get_color();
	light_data.energy = p_light->get_param(Light3D::PARAM_ENERGY);
	light_data.position = p_light->get_global_transform().origin;
	light_data.name = p_light->get_name();
	light_data.cast_shadow = p_light->has_shadow();

	// Type-specific properties
	DirectionalLight3D *dir_light = Object::cast_to<DirectionalLight3D>(p_light);
	if (dir_light != nullptr) {
		light_data.type = 0; // LIGHT_TYPE_DIRECTIONAL
		light_data.direction = -dir_light->get_global_transform().basis.get_column(Vector3::AXIS_Z).normalized();
		light_data.range = 1000000.0f; // Infinite
		r_lights.push_back(light_data);
		return;
	}

	OmniLight3D *omni_light = Object::cast_to<OmniLight3D>(p_light);
	if (omni_light != nullptr) {
		light_data.type = 1; // LIGHT_TYPE_OMNI
		light_data.range = omni_light->get_param(Light3D::PARAM_RANGE);
		light_data.attenuation = omni_light->get_param(Light3D::PARAM_ATTENUATION);
		r_lights.push_back(light_data);
		return;
	}

	SpotLight3D *spot_light = Object::cast_to<SpotLight3D>(p_light);
	if (spot_light != nullptr) {
		light_data.type = 2; // LIGHT_TYPE_SPOT
		light_data.direction = -spot_light->get_global_transform().basis.get_column(Vector3::AXIS_Z).normalized();
		light_data.range = spot_light->get_param(Light3D::PARAM_RANGE);
		light_data.attenuation = spot_light->get_param(Light3D::PARAM_ATTENUATION);
		float spot_angle = spot_light->get_param(Light3D::PARAM_SPOT_ANGLE);
		light_data.cos_spot_angle = Math::cos(Math::deg_to_rad(spot_angle));
		light_data.inv_spot_attenuation = 1.0f / spot_light->get_param(Light3D::PARAM_SPOT_ATTENUATION);
		r_lights.push_back(light_data);
		return;
	}
}

bool LightmapBaker::_validate_meshes(const std::vector<MeshData> &p_meshes) {
	for (const auto &mesh : p_meshes) {
		if (mesh.vertices.is_empty()) {
			UtilityFunctions::push_error("Mesh has no vertices");
			return false;
		}
		if (mesh.uv2s.is_empty()) {
			UtilityFunctions::push_error("Mesh has no UV2 coordinates");
			return false;
		}
		if (mesh.uv2s.size() != mesh.vertices.size()) {
			UtilityFunctions::push_error("Vertex count doesn't match UV2 count");
			return false;
		}
	}
	return true;
}

// Baking stages (Phase 1 - basic implementation)
LightmapBaker::BakeError LightmapBaker::_bake_direct_light(Ref<LightmapGIData> p_output_data, BakeProgressFunc p_progress, void *p_userdata) {
	if (gathered_meshes.empty()) {
		return BAKE_ERROR_NO_MESHES;
	}

	int atlas_size = atlas_size_override;
	if (atlas_size <= 0) {
		atlas_size = 512;
		switch (bake_quality) {
			case BAKE_QUALITY_LOW: atlas_size = 256; break;
			case BAKE_QUALITY_MEDIUM: atlas_size = 512; break;
			case BAKE_QUALITY_HIGH: atlas_size = 1024; break;
			case BAKE_QUALITY_ULTRA: atlas_size = 2048; break;
		}
	}
	atlas_size = std::min(atlas_size, max_texture_size);
	if (atlas_size < 32) {
		return BAKE_ERROR_LIGHTMAP_TOO_SMALL;
	}
	const int padding = std::max(0, atlas_padding);

	_report_progress(0.15f, "Building ray meshes for shadowing...", p_progress, p_userdata);
	_build_ray_meshes();

	// Bake per-mesh (per-surface) lightmaps first.
	Vector<Ref<Image>> mesh_lightmaps;
	mesh_lightmaps.resize((int)gathered_meshes.size());

	for (int i = 0; i < (int)gathered_meshes.size(); i++) {
		float t = (gathered_meshes.size() <= 1) ? 0.0f : (float)i / (float)(gathered_meshes.size() - 1);
		_report_progress(0.2f + 0.35f * t, "Rasterizing UV2 and evaluating lights...", p_progress, p_userdata);

		Vector2i hint = gathered_meshes[(size_t)i].lightmap_size_hint;
		int w = hint.x > 0 ? hint.x : atlas_size;
		int h = hint.y > 0 ? hint.y : atlas_size;
		w = std::clamp((int)Math::round((float)w * texel_scale), 32, atlas_size);
		h = std::clamp((int)Math::round((float)h * texel_scale), 32, atlas_size);

		Ref<Image> img = _create_lightmap_image(w, h);
		if (img.is_null()) {
			return BAKE_ERROR_CANT_CREATE_IMAGE;
		}

		// Alpha is used as a coverage mask: 0=empty texel (outside UV2 islands), 1=valid.
		img->fill(Color(0, 0, 0, 0));
		_rasterize_mesh_direct_lighting(gathered_meshes[(size_t)i], img);
		mesh_lightmaps.set(i, img);
	}

	_report_progress(0.6f, "Packing lightmaps into atlases...", p_progress, p_userdata);
	Vector<Ref<Image>> atlas_layers = _pack_lightmaps_to_atlas(gathered_meshes, mesh_lightmaps, atlas_size, padding);
	if (atlas_layers.is_empty()) {
		return BAKE_ERROR_ATLAS_TOO_SMALL;
	}

	// Phase 2: Indirect lighting (bounces) — modifies mesh_lightmaps in place
	if (bounces > 0) {
		_report_progress(0.65f, "Baking indirect lighting...", p_progress, p_userdata);
		BakeError error = _bake_indirect_light(mesh_lightmaps, p_progress, p_userdata);
		if (error != BAKE_ERROR_OK) {
			UtilityFunctions::push_warning("Indirect pass failed, using direct lighting only");
		}
	}

	_report_progress(0.75f, "Dilating seams...", p_progress, p_userdata);
	_dilate_lightmaps(mesh_lightmaps, std::max(0, seam_dilation_radius));

	// Update atlas pixel data in-place (layout/UV scale doesn't change with bounces/dilation).
	_report_progress(0.8f, "Updating atlas layers...", p_progress, p_userdata);
	for (int i = 0; i < mesh_lightmaps.size() && i < (int)gathered_meshes.size(); i++) {
		Ref<Image> src = mesh_lightmaps[i];
		if (src.is_null() || src->is_empty()) {
			continue;
		}
		const MeshData &md = gathered_meshes[(size_t)i];
		if (md.lightmap_slice < 0 || md.lightmap_slice >= atlas_layers.size()) {
			continue;
		}
		Ref<Image> dst = atlas_layers[md.lightmap_slice];
		if (dst.is_null() || dst->is_empty()) {
			continue;
		}

		Vector2 uv_offset = md.lightmap_uv_scale.position;
		Vector2i pos(
				(int)Math::round(uv_offset.x * (float)dst->get_width()),
				(int)Math::round(uv_offset.y * (float)dst->get_height()));

		if (pos.x < 0 || pos.y < 0 || pos.x + src->get_width() > dst->get_width() || pos.y + src->get_height() > dst->get_height()) {
			UtilityFunctions::push_warning("LightmapBaker: atlas blit out of bounds for surface index " + String::num_int64(i));
			continue;
		}

		dst->blit_rect(src, Rect2i(Vector2i(0, 0), src->get_size()), pos);
	}

	// Validate atlas layers before texture creation.
	if (atlas_layers.is_empty()) {
		UtilityFunctions::push_error("LightmapBaker: atlas_layers is empty");
		return BAKE_ERROR_CANT_CREATE_IMAGE;
	}
	bool has_valid_atlas = false;
	for (int i = 0; i < atlas_layers.size(); i++) {
		if (!atlas_layers[i].is_null() && !atlas_layers[i]->is_empty()) {
			has_valid_atlas = true;
			break;
		}
	}
	if (!has_valid_atlas) {
		UtilityFunctions::push_error("LightmapBaker: atlas_layers are all null/empty");
		return BAKE_ERROR_CANT_CREATE_IMAGE;
	}

	_report_progress(0.85f, "Creating Texture2DArray...", p_progress, p_userdata);
	// Drop the alpha coverage mask before uploading (kept only for dilation during baking).
	for (int i = 0; i < atlas_layers.size(); i++) {
		if (atlas_layers[i].is_null() || atlas_layers[i]->is_empty()) {
			continue;
		}
		if (atlas_layers[i]->get_format() != Image::FORMAT_RGBH) {
			atlas_layers[i]->convert(Image::FORMAT_RGBH);
		}
	}
	Ref<Texture2DArray> tex_array = _create_texture_array_from_images(atlas_layers);
	if (tex_array.is_null()) {
		UtilityFunctions::push_error("Failed to create Texture2DArray from atlas layers");
		return BAKE_ERROR_CANT_CREATE_IMAGE;
	}

	_write_output_data(p_output_data, tex_array);
	return BAKE_ERROR_OK;
}

LightmapBaker::BakeError LightmapBaker::_bake_indirect_light(Vector<Ref<Image>> &p_lightmaps, BakeProgressFunc p_progress, void *p_userdata) {
	if (p_lightmaps.is_empty() || bounces <= 0) {
		return BAKE_ERROR_OK;
	}

	Vector<Ref<Image>> bounce_accum = p_lightmaps;

	for (int bounce = 0; bounce < bounces; bounce++) {
		float progress = 0.5f + (float)bounce / (float)std::max(1, bounces) * 0.3f;
		_report_progress(progress, "Computing bounce " + String::num_int64(bounce + 1) + "/" + String::num_int64((int64_t)bounces), p_progress, p_userdata);

		Vector<Ref<Image>> bounce_light;
		bounce_light.resize(p_lightmaps.size());
		for (int i = 0; i < p_lightmaps.size(); i++) {
			Ref<Image> img = _create_lightmap_image(p_lightmaps[i]->get_width(), p_lightmaps[i]->get_height());
			if (img.is_null()) return BAKE_ERROR_CANT_CREATE_IMAGE;
			img->fill(Color(0, 0, 0, 0));
			bounce_light.set(i, img);
		}

		// Rasterize bounce lighting using neighbor sampling
		for (int i = 0; i < p_lightmaps.size(); i++) {
			Ref<Image> src = bounce_accum[i];
			Ref<Image> dst = bounce_light[i];
			if (src.is_null() || dst.is_null()) continue;

			for (int y = 0; y < dst->get_height(); y++) {
				for (int x = 0; x < dst->get_width(); x++) {
					Color direct_here = src->get_pixel(x, y);
					if (direct_here.a < 0.5f) continue;

					Vector3 indirect(0, 0, 0);
					int sample_count = 0;

					// Sample nearby pixels as indirect sources
					const int samples[] = { -2, -1, 1, 2 };
					for (int dx : samples) {
						int nx = x + dx;
						if (nx >= 0 && nx < dst->get_width()) {
							Color neighbor = src->get_pixel(nx, y);
							if (neighbor.a > 0.5f) {
								indirect += Vector3(neighbor.r, neighbor.g, neighbor.b);
								sample_count++;
							}
						}
					}
					for (int dy : samples) {
						int ny = y + dy;
						if (ny >= 0 && ny < dst->get_height()) {
							Color neighbor = src->get_pixel(x, ny);
							if (neighbor.a > 0.5f) {
								indirect += Vector3(neighbor.r, neighbor.g, neighbor.b);
								sample_count++;
							}
						}
					}

					if (sample_count > 0) {
						indirect /= (float)sample_count;
						indirect *= 0.5f;
						dst->set_pixel(x, y, Color(indirect.x, indirect.y, indirect.z, 1.0f));
					}
				}
			}
		}

		// Dilate empty texels around UV islands (0 disables).
		_dilate_lightmaps(bounce_light, seam_dilation_radius > 0 ? 1 : 0);

		// Accumulate with falloff
		float bounce_energy = bounce_indirect_energy * Math::pow(0.5f, (float)(bounce + 1));
		for (int i = 0; i < p_lightmaps.size(); i++) {
			Ref<Image> dst = p_lightmaps[i];
			Ref<Image> src = bounce_light[i];
			if (dst.is_null() || src.is_null()) {
				UtilityFunctions::push_warning("Bounce " + String::num_int64(bounce + 1) + ": null lightmap at index " + String::num_int64(i));
				continue;
			}
			if (dst->is_empty() || src->is_empty()) {
				UtilityFunctions::push_warning("Bounce " + String::num_int64(bounce + 1) + ": empty lightmap at index " + String::num_int64(i));
				continue;
			}

			for (int y = 0; y < dst->get_height(); y++) {
				for (int x = 0; x < dst->get_width(); x++) {
					Color direct = dst->get_pixel(x, y);
					Color bounce = src->get_pixel(x, y) * bounce_energy;
					Color result(direct.r + bounce.r, direct.g + bounce.g, direct.b + bounce.b, direct.a);
					dst->set_pixel(x, y, result);
				}
			}
		}

		bounce_accum = bounce_light;
	}

	return BAKE_ERROR_OK;
}

LightmapBaker::BakeError LightmapBaker::_bake_light_probes(Ref<LightmapGIData> p_output_data, BakeProgressFunc p_progress, void *p_userdata) {
	// TODO: Implement light probe generation
	return BAKE_ERROR_OK;
}

// Post-processing
void LightmapBaker::_apply_seam_blending(Vector<Ref<Image>> &p_textures) {
	// TODO: Implement seam blending
}

void LightmapBaker::_apply_denoising(Vector<Ref<Image>> &p_textures) {
	// TODO: Implement denoising (could integrate with Intel OIDN)
}

void LightmapBaker::_dilate_lightmaps(Vector<Ref<Image>> &p_lightmaps, int p_dilation_radius) {
	if (p_dilation_radius <= 0) return;

	for (Ref<Image> &img : p_lightmaps) {
		if (img.is_null()) continue;

		Ref<Image> dilated = _create_lightmap_image(img->get_width(), img->get_height());
		if (dilated.is_null()) continue;

		// Copy original
		for (int y = 0; y < img->get_height(); y++) {
			for (int x = 0; x < img->get_width(); x++) {
				dilated->set_pixel(x, y, img->get_pixel(x, y));
			}
		}

		// Dilate empty pixels
		for (int y = 0; y < img->get_height(); y++) {
			for (int x = 0; x < img->get_width(); x++) {
				Color pix = img->get_pixel(x, y);
				if (pix.a > 0.5f) continue; // Already filled

				Vector3 accum(0, 0, 0);
				int count = 0;
				for (int dy = -p_dilation_radius; dy <= p_dilation_radius; dy++) {
					for (int dx = -p_dilation_radius; dx <= p_dilation_radius; dx++) {
						if (dx == 0 && dy == 0) continue;
						int nx = x + dx;
						int ny = y + dy;
						if (nx >= 0 && nx < img->get_width() && ny >= 0 && ny < img->get_height()) {
							Color neighbor = img->get_pixel(nx, ny);
							if (neighbor.a > 0.5f) {
								accum += Vector3(neighbor.r, neighbor.g, neighbor.b);
								count++;
							}
						}
					}
				}

				if (count > 0) {
					accum /= (float)count;
					dilated->set_pixel(x, y, Color(accum.x, accum.y, accum.z, 1.0f));
				}
			}
		}

		img = dilated;
	}
}

// Texture management
Ref<Image> LightmapBaker::_create_lightmap_image(int p_width, int p_height) {
	if (p_width <= 0 || p_height <= 0) {
		return Ref<Image>();
	}
	// RGBAH so alpha can be used as a coverage mask for dilation.
	Ref<Image> img = Image::create(p_width, p_height, false, Image::FORMAT_RGBAH);
	if (img.is_null() || img->is_empty()) {
		UtilityFunctions::push_error("LightmapBaker: failed to create Image (" + String::num_int64(p_width) + "x" + String::num_int64(p_height) + ")");
		return Ref<Image>();
	}
	return img;
}

Vector<Ref<Image>> LightmapBaker::_pack_lightmaps_to_atlas(std::vector<MeshData> &p_meshes, const Vector<Ref<Image>> &p_lightmaps, int p_atlas_size, int p_padding) {
	if (p_meshes.empty() || p_lightmaps.is_empty() || p_meshes.size() != (size_t)p_lightmaps.size()) {
		return Vector<Ref<Image>>();
	}
	if (p_atlas_size <= 0) {
		return Vector<Ref<Image>>();
	}

	struct Item {
		int idx = -1;
		int w = 0;
		int h = 0;
	};

	Vector<Item> items;
	items.resize(p_lightmaps.size());
	for (int i = 0; i < p_lightmaps.size(); i++) {
		Ref<Image> img = p_lightmaps[i];
		if (img.is_null() || img->is_empty() || img->get_width() <= 0 || img->get_height() <= 0) {
			return Vector<Ref<Image>>();
		}
		Item it;
		it.idx = i;
		it.w = img->get_width() + p_padding * 2;
		it.h = img->get_height() + p_padding * 2;
		items.set(i, it);
	}

	// Sort by height (simple shelf packer works better).
	struct _ItemHeightComparator {
		bool operator()(const Item &a, const Item &b) const {
			return a.h > b.h;
		}
	};
	items.sort_custom<_ItemHeightComparator>();

	struct Placement {
		int slice = 0;
		Vector2i pos;
	};
	Vector<Placement> placement;
	placement.resize(p_lightmaps.size());

	int slice = 0;
	int x = 0;
	int y = 0;
	int shelf_h = 0;

	for (int k = 0; k < items.size(); k++) {
		Item it = items[k];
		if (it.w > p_atlas_size || it.h > p_atlas_size) {
			return Vector<Ref<Image>>();
		}

		if (x + it.w > p_atlas_size) {
			y += shelf_h;
			x = 0;
			shelf_h = 0;
		}
		if (y + it.h > p_atlas_size) {
			slice++;
			x = 0;
			y = 0;
			shelf_h = 0;
		}

		Placement pl;
		pl.slice = slice;
		pl.pos = Vector2i(x + p_padding, y + p_padding);
		placement.set(it.idx, pl);

		x += it.w;
		shelf_h = std::max(shelf_h, it.h);
	}

	Vector<Ref<Image>> atlas_layers;
	atlas_layers.resize(slice + 1);
	for (int s = 0; s <= slice; s++) {
		Ref<Image> atlas = _create_lightmap_image(p_atlas_size, p_atlas_size);
		if (atlas.is_null() || atlas->is_empty()) {
			return Vector<Ref<Image>>();
		}
		atlas->fill(Color(0, 0, 0, 0));
		atlas_layers.set(s, atlas);
	}

	Vector2 inv_atlas = Vector2(1.0f / (float)p_atlas_size, 1.0f / (float)p_atlas_size);
	for (int i = 0; i < p_lightmaps.size(); i++) {
		Ref<Image> src = p_lightmaps[i];
		const Placement &pl = placement[i];
		Ref<Image> dst = atlas_layers[pl.slice];
		if (src.is_null() || src->is_empty() || dst.is_null() || dst->is_empty()) {
			return Vector<Ref<Image>>();
		}
		Vector2i ssize = src->get_size();
		if (ssize.x <= 0 || ssize.y <= 0) {
			return Vector<Ref<Image>>();
		}
		dst->blit_rect(src, Rect2i(Vector2i(0, 0), ssize), pl.pos);

		MeshData &md = p_meshes[(size_t)i];
		md.lightmap_slice = pl.slice;
		Vector2 uv_offset = Vector2((float)pl.pos.x, (float)pl.pos.y) * inv_atlas;
		Vector2 uv_scale = Vector2((float)src->get_width(), (float)src->get_height()) * inv_atlas;
		md.lightmap_uv_scale = Rect2(uv_offset, uv_scale);
	}

	return atlas_layers;
}

// Utility
void LightmapBaker::_report_progress(float p_progress, const String &p_status, BakeProgressFunc p_callback, void *p_userdata) {
	UtilityFunctions::print("[", String::num(p_progress * 100.0f, 1), "%] ", p_status);
	if (p_callback != nullptr) {
		p_callback(p_progress, p_status, p_userdata);
	}
}

bool LightmapBaker::_check_cancel_requested() {
	// TODO: Implement cancellation support
	return false;
}

Ref<Texture2DArray> LightmapBaker::_create_texture_array_from_images(const Vector<Ref<Image>> &p_layers) {
	if (p_layers.is_empty()) {
		return Ref<Texture2DArray>();
	}
	if (p_layers[0].is_null() || p_layers[0]->is_empty()) {
		return Ref<Texture2DArray>();
	}
	const Vector2i expected_size = p_layers[0]->get_size();
	const Image::Format expected_format = p_layers[0]->get_format();
	if (expected_size.x <= 0 || expected_size.y <= 0) {
		return Ref<Texture2DArray>();
	}

	TypedArray<Ref<Image>> images;
	images.resize(p_layers.size());
	for (int i = 0; i < p_layers.size(); i++) {
		if (p_layers[i].is_null() || p_layers[i]->is_empty()) {
			UtilityFunctions::push_error("LightmapBaker: Texture2DArray layer is null/empty at index " + String::num_int64(i));
			return Ref<Texture2DArray>();
		}
		if (p_layers[i]->get_size() != expected_size) {
			UtilityFunctions::push_error("LightmapBaker: Texture2DArray layer size mismatch at index " + String::num_int64(i));
			return Ref<Texture2DArray>();
		}
		if (p_layers[i]->get_format() != expected_format) {
			UtilityFunctions::push_error("LightmapBaker: Texture2DArray layer format mismatch at index " + String::num_int64(i));
			return Ref<Texture2DArray>();
		}
		images[i] = p_layers[i];
	}

	Ref<Texture2DArray> tex;
	tex.instantiate();
	Error err = tex->create_from_images(images);
	if (err != OK) {
		UtilityFunctions::push_error("LightmapBaker: Texture2DArray::create_from_images failed (err=" + String::num_int64((int64_t)err) + ")");
		return Ref<Texture2DArray>();
	}
	return tex;
}

void LightmapBaker::_write_output_data(Ref<LightmapGIData> p_output_data, const Ref<Texture2DArray> &p_tex_array) {
	TypedArray<Ref<TextureLayered>> textures;
	textures.resize(1);
	textures[0] = p_tex_array;
	p_output_data->set_lightmap_textures(textures);
	p_output_data->set_light_texture(p_tex_array);
	p_output_data->set_uses_spherical_harmonics(false);

	p_output_data->clear_users();
	for (size_t i = 0; i < gathered_meshes.size(); i++) {
		const MeshData &md = gathered_meshes[i];
		if (md.owner_node == nullptr) {
			continue;
		}
		p_output_data->add_user(md.owner_node->get_path(), md.lightmap_uv_scale, md.lightmap_slice, md.sub_instance);
	}
}

static inline float _edge_function(const Vector2 &a, const Vector2 &b, const Vector2 &c) {
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

void LightmapBaker::_rasterize_mesh_direct_lighting(const MeshData &p_mesh, Ref<Image> p_target) {
	const int w = p_target->get_width();
	const int h = p_target->get_height();

	Color surface_albedo(1, 1, 1, 1);
	if (use_material_albedo && p_mesh.material.is_valid()) {
		BaseMaterial3D *bm = Object::cast_to<BaseMaterial3D>(p_mesh.material.ptr());
		if (bm != nullptr) {
			surface_albedo = bm->get_albedo();
		}
	}

	const int vertex_count = p_mesh.vertices.size();
	if (vertex_count < 3 || p_mesh.uv2s.size() != vertex_count) {
		return;
	}

	auto sample_triangle = [&](int i0, int i1, int i2) {
		Vector2 uv0 = p_mesh.uv2s[i0];
		Vector2 uv1 = p_mesh.uv2s[i1];
		Vector2 uv2 = p_mesh.uv2s[i2];

		Vector2 p0 = uv0 * Vector2((float)w, (float)h);
		Vector2 p1 = uv1 * Vector2((float)w, (float)h);
		Vector2 p2 = uv2 * Vector2((float)w, (float)h);

		float area = _edge_function(p0, p1, p2);
		if (Math::abs(area) < 1e-8f) {
			return;
		}
		float inv_area = 1.0f / area;

		int min_x = (int)Math::floor(std::min({ p0.x, p1.x, p2.x }));
		int max_x = (int)Math::ceil(std::max({ p0.x, p1.x, p2.x }));
		int min_y = (int)Math::floor(std::min({ p0.y, p1.y, p2.y }));
		int max_y = (int)Math::ceil(std::max({ p0.y, p1.y, p2.y }));
		min_x = std::clamp(min_x, 0, w - 1);
		max_x = std::clamp(max_x, 0, w - 1);
		min_y = std::clamp(min_y, 0, h - 1);
		max_y = std::clamp(max_y, 0, h - 1);

		Vector3 v0 = p_mesh.transform.xform(p_mesh.vertices[i0]);
		Vector3 v1 = p_mesh.transform.xform(p_mesh.vertices[i1]);
		Vector3 v2 = p_mesh.transform.xform(p_mesh.vertices[i2]);

		Vector3 nn0 = p_mesh.normals.is_empty() ? Vector3(0, 1, 0) : p_mesh.normals[i0];
		Vector3 nn1 = p_mesh.normals.is_empty() ? Vector3(0, 1, 0) : p_mesh.normals[i1];
		Vector3 nn2 = p_mesh.normals.is_empty() ? Vector3(0, 1, 0) : p_mesh.normals[i2];
		Vector3 n0 = p_mesh.transform.basis.xform(nn0).normalized();
		Vector3 n1 = p_mesh.transform.basis.xform(nn1).normalized();
		Vector3 n2 = p_mesh.transform.basis.xform(nn2).normalized();

		for (int y = min_y; y <= max_y; y++) {
			for (int x = min_x; x <= max_x; x++) {
				Vector2 p((float)x + 0.5f, (float)y + 0.5f);
				float w0 = _edge_function(p1, p2, p) * inv_area;
				float w1 = _edge_function(p2, p0, p) * inv_area;
				float w2 = _edge_function(p0, p1, p) * inv_area;
				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
					continue;
				}
				Vector3 world_pos = v0 * w0 + v1 * w1 + v2 * w2;
				Vector3 world_nrm = (n0 * w0 + n1 * w1 + n2 * w2).normalized();
				Color lit = _evaluate_direct_lighting(world_pos, world_nrm);
				lit.r *= surface_albedo.r;
				lit.g *= surface_albedo.g;
				lit.b *= surface_albedo.b;
				lit.a = 1.0f;
				p_target->set_pixel(x, y, lit);
			}
		}
	};

	if (!p_mesh.indices.is_empty()) {
		const int idx_count = p_mesh.indices.size();
		for (int i = 0; i + 2 < idx_count; i += 3) {
			int i0 = p_mesh.indices[i + 0];
			int i1 = p_mesh.indices[i + 1];
			int i2 = p_mesh.indices[i + 2];
			if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
				continue;
			}
			sample_triangle(i0, i1, i2);
		}
	} else {
		for (int i = 0; i + 2 < vertex_count; i += 3) {
			sample_triangle(i, i + 1, i + 2);
		}
	}
}

Color LightmapBaker::_evaluate_direct_lighting(const Vector3 &p_world_pos, const Vector3 &p_world_normal) const {
	const float amb = std::max(0.0f, ambient_energy);
	Vector3 accum(amb, amb, amb);
	Vector3 n = p_world_normal.normalized();

	for (const LightData &l : gathered_lights) {
		Vector3 L;
		float atten = 1.0f;

		if (l.type == 0) {
			L = (-l.direction).normalized();
		} else {
			Vector3 to_light = l.position - p_world_pos;
			float dist = to_light.length();
			if (dist <= 1e-4f) {
				continue;
			}
			L = to_light / dist;

			float range = std::max(0.001f, l.range);
			float x = std::max(0.0f, 1.0f - dist / range);
			atten = Math::pow(x, std::max(0.0001f, l.attenuation));

			if (l.type == 2) {
				float spot_dot = L.dot((-l.direction).normalized());
				if (spot_dot < l.cos_spot_angle) {
					atten = 0.0f;
				} else {
					float edge = (spot_dot - l.cos_spot_angle) / std::max(1e-4f, 1.0f - l.cos_spot_angle);
					atten *= Math::pow(edge, std::max(0.01f, l.inv_spot_attenuation));
				}
			}
		}

		float ndotl = std::max(0.0f, n.dot(L));
		if (ndotl <= 0.0f) {
			continue;
		}
		if (use_lambert_normalization) {
			ndotl *= (float)(1.0 / Math_PI);
		}
		if (use_shadowing && l.cast_shadow) {
			if (_is_shadowed(p_world_pos, n, l)) {
				continue;
			}
		}
		Vector3 col(l.color.r, l.color.g, l.color.b);
		accum += col * (l.energy * ndotl * atten);
	}

	accum *= std::max(0.0f, lightmap_energy_scale);
	return Color(accum.x, accum.y, accum.z, 1.0f);
}

static inline bool _ray_intersects_aabb(const Vector3 &orig, const Vector3 &dir, const AABB &aabb, float tmax) {
	Vector3 inv_dir(1.0f / (dir.x == 0.0f ? 1e-20f : dir.x), 1.0f / (dir.y == 0.0f ? 1e-20f : dir.y), 1.0f / (dir.z == 0.0f ? 1e-20f : dir.z));
	Vector3 t0 = (aabb.position - orig) * inv_dir;
	Vector3 t1 = (aabb.position + aabb.size - orig) * inv_dir;
	Vector3 tmin_v(Math::min(t0.x, t1.x), Math::min(t0.y, t1.y), Math::min(t0.z, t1.z));
	Vector3 tmax_v(Math::max(t0.x, t1.x), Math::max(t0.y, t1.y), Math::max(t0.z, t1.z));
	float tmin = Math::max(0.0f, Math::max(tmin_v.x, Math::max(tmin_v.y, tmin_v.z)));
	float tmax_hit = Math::min(tmax, Math::min(tmax_v.x, Math::min(tmax_v.y, tmax_v.z)));
	return tmax_hit >= tmin;
}

static inline bool _ray_intersects_tri(const Vector3 &orig, const Vector3 &dir, const _LM_RayTri &tri, float tmax, float &r_t) {
	// Moller–Trumbore
	const float eps = 1e-7f;
	Vector3 e1 = tri.b - tri.a;
	Vector3 e2 = tri.c - tri.a;
	Vector3 p = dir.cross(e2);
	float det = e1.dot(p);
	if (Math::abs(det) < eps) {
		return false;
	}
	float inv_det = 1.0f / det;
	Vector3 tvec = orig - tri.a;
	float u = tvec.dot(p) * inv_det;
	if (u < 0.0f || u > 1.0f) {
		return false;
	}
	Vector3 q = tvec.cross(e1);
	float v = dir.dot(q) * inv_det;
	if (v < 0.0f || u + v > 1.0f) {
		return false;
	}
	float t = e2.dot(q) * inv_det;
	if (t > eps && t < tmax) {
		r_t = t;
		return true;
	}
	return false;
}

bool LightmapBaker::_is_shadowed(const Vector3 &p_world_pos, const Vector3 &p_world_normal, const LightData &p_light) const {
	Vector3 origin = p_world_pos + p_world_normal * bias;
	Vector3 dir;
	float max_dist = 1e20f;

	if (p_light.type == 0) {
		dir = (-p_light.direction).normalized();
	} else {
		Vector3 to_light = p_light.position - origin;
		float dist = to_light.length();
		if (dist <= 1e-4f) {
			return false;
		}
		dir = to_light / dist;
		max_dist = Math::max(0.0f, dist - bias);
		if (max_dist <= 1e-4f) {
			return false;
		}
	}

	for (const RayMesh &rm : ray_meshes) {
		if (!_ray_intersects_aabb(origin, dir, rm.aabb, max_dist)) {
			continue;
		}
		for (const _LM_RayTri &tri : rm.tris) {
			float t = 0.0f;
			if (_ray_intersects_tri(origin, dir, tri, max_dist, t)) {
				return true;
			}
		}
	}

	return false;
}

void LightmapBaker::_build_ray_meshes() {
	ray_meshes.clear();
	ray_meshes.reserve(gathered_meshes.size());

	for (const MeshData &md : gathered_meshes) {
		RayMesh rm;
		rm.aabb = AABB();
		bool aabb_inited = false;

		const int vcount = md.vertices.size();
		if (vcount < 3) {
			continue;
		}

		auto push_tri = [&](int i0, int i1, int i2) {
			Vector3 a = md.transform.xform(md.vertices[i0]);
			Vector3 b = md.transform.xform(md.vertices[i1]);
			Vector3 c = md.transform.xform(md.vertices[i2]);
			_LM_RayTri t{ a, b, c };
			rm.tris.push_back(t);

			if (!aabb_inited) {
				rm.aabb.position = a;
				rm.aabb.size = Vector3(0, 0, 0);
				aabb_inited = true;
			}
			rm.aabb.expand_to(a);
			rm.aabb.expand_to(b);
			rm.aabb.expand_to(c);
		};

		if (!md.indices.is_empty()) {
			const int icount = md.indices.size();
			rm.tris.reserve((size_t)(icount / 3));
			for (int i = 0; i + 2 < icount; i += 3) {
				int i0 = md.indices[i + 0];
				int i1 = md.indices[i + 1];
				int i2 = md.indices[i + 2];
				if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vcount || i1 >= vcount || i2 >= vcount) {
					continue;
				}
				push_tri(i0, i1, i2);
			}
		} else {
			rm.tris.reserve((size_t)(vcount / 3));
			for (int i = 0; i + 2 < vcount; i += 3) {
				push_tri(i, i + 1, i + 2);
			}
		}

		if (!rm.tris.empty()) {
			ray_meshes.push_back(std::move(rm));
		}
	}
}

} // namespace godot
