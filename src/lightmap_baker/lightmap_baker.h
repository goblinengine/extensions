#ifndef LIGHTMAP_BAKER_H
#define LIGHTMAP_BAKER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/lightmap_gi_data.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image_texture_layered.hpp>
#include <godot_cpp/classes/texture_layered.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>
#include <cstdint>

namespace godot {

// Forward declarations
class LightmapBaker;
using BakeProgressFunc = void (*)(float p_progress, const String &p_status, void *p_userdata);

struct MeshData {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uv2s;
	PackedInt32Array indices;
	Transform3D transform;
	Ref<Material> material;
	Node *owner_node = nullptr;
	int sub_instance = -1;
	Vector2i lightmap_size_hint;
	int lightmap_slice = 0;
	Rect2 lightmap_uv_scale;
};

struct LightData {
	Vector3 position;
	Vector3 direction;
	Color color;
	float energy = 1.0f;
	float range = 10.0f;
	float attenuation = 1.0f;
	float size = 0.0f;
	float cos_spot_angle = -1.0f;
	float inv_spot_attenuation = 1.0f;
	int type = 0; // 0=directional, 1=omni, 2=spot
	bool cast_shadow = true;
	String name;
};

class LightmapBaker : public RefCounted {
	GDCLASS(LightmapBaker, RefCounted)

public:
	enum BakeQuality {
		BAKE_QUALITY_LOW = 0,
		BAKE_QUALITY_MEDIUM = 1,
		BAKE_QUALITY_HIGH = 2,
		BAKE_QUALITY_ULTRA = 3,
	};

	enum BakeError {
		BAKE_ERROR_OK = 0,
		BAKE_ERROR_NO_SCENE_ROOT = 1,
		BAKE_ERROR_NO_LIGHTMAPPER = 2,
		BAKE_ERROR_NO_MESHES = 3,
		BAKE_ERROR_MESHES_INVALID = 4,
		BAKE_ERROR_CANT_CREATE_IMAGE = 5,
		BAKE_ERROR_USER_ABORTED = 6,
		BAKE_ERROR_TEXTURE_SIZE_TOO_SMALL = 7,
		BAKE_ERROR_LIGHTMAP_TOO_SMALL = 8,
		BAKE_ERROR_ATLAS_TOO_SMALL = 9,
	};

	LightmapBaker();
	~LightmapBaker();

	// Configuration
	void set_bake_quality(BakeQuality p_quality);
	BakeQuality get_bake_quality() const;

	void set_bounces(int p_bounces);
	int get_bounces() const;

	void set_bounce_indirect_energy(float p_energy);
	float get_bounce_indirect_energy() const;

	void set_bias(float p_bias);
	float get_bias() const;

	void set_max_texture_size(int p_size);
	int get_max_texture_size() const;

	// Optional: override atlas size (0 = auto from bake_quality)
	void set_atlas_size_override(int p_size);
	int get_atlas_size_override() const;

	// Optional: packing padding in pixels
	void set_atlas_padding(int p_padding);
	int get_atlas_padding() const;

	// Post-process: dilate lightmap UV island borders (0 disables)
	void set_seam_dilation_radius(int p_radius);
	int get_seam_dilation_radius() const;

	void set_texel_scale(float p_scale);
	float get_texel_scale() const;

	// Overall multiplier applied to baked lighting (useful to match runtime look)
	void set_lightmap_energy_scale(float p_scale);
	float get_lightmap_energy_scale() const;

	// Simple ambient term added before scaling (linear space)
	void set_ambient_energy(float p_energy);
	float get_ambient_energy() const;

	// Approximate runtime shading closer (diffuse albedo modulation and Lambert 1/PI factor).
	void set_use_material_albedo(bool p_enabled);
	bool get_use_material_albedo() const;
	void set_use_lambert_normalization(bool p_enabled);
	bool get_use_lambert_normalization() const;

	void set_use_denoiser(bool p_enabled);
	bool get_use_denoiser() const;

	void set_use_shadowing(bool p_enabled);
	bool get_use_shadowing() const;

	// Mesh filtering
	void set_mesh_layer_mask(uint32_t p_mask);
	uint32_t get_mesh_layer_mask() const;

	void set_denoiser_strength(float p_strength);
	float get_denoiser_strength() const;

	// Main bake function
	BakeError bake(Node *p_from_node, Ref<LightmapGIData> p_output_data);

	// UV2 generation only (does not bake).
	// Static so you can call: LightmapBaker.lightmap_unwrap(mesh, xform, texel_size)
	// Returns an Error code (OK on success).
	// If p_texel_size <= 0, uses the project setting rendering/lightmapping/primitive_meshes/texel_size (fallback 0.1).
	static int lightmap_unwrap(const Ref<ArrayMesh> &p_mesh, const Transform3D &p_transform, float p_texel_size = 0.0f);

	// Advanced: Step-by-step baking with progress callback
	BakeError bake_with_progress(Node *p_from_node, Ref<LightmapGIData> p_output_data,
								 BakeProgressFunc p_progress_func = nullptr, void *p_userdata = nullptr);

	// Debug/inspection
	int get_gathered_mesh_count() const { return gathered_meshes.size(); }
	int get_gathered_light_count() const { return gathered_lights.size(); }

protected:
	static void _bind_methods();

private:
	// Configuration
	BakeQuality bake_quality = BAKE_QUALITY_MEDIUM;
	int bounces = 3;
	float bounce_indirect_energy = 1.0f;
	float bias = 0.0005f;
	int max_texture_size = 16384;
	int atlas_size_override = 0;
	int atlas_padding = 2;
	int seam_dilation_radius = 2;
	float texel_scale = 1.0f;
	float lightmap_energy_scale = 1.0f;
	float ambient_energy = 0.0f;
	bool use_material_albedo = true;
	bool use_lambert_normalization = true;
	bool use_denoiser = true;
	float denoiser_strength = 0.1f;
	bool use_shadowing = true;
	uint32_t mesh_layer_mask = 0xFFFFFFFFu;

	// State during bake
	std::vector<MeshData> gathered_meshes;
	std::vector<LightData> gathered_lights;
	struct RayMesh;
	std::vector<RayMesh> ray_meshes;

	// Helper functions
	void _find_meshes_and_lights(Node *p_at_node, std::vector<MeshData> &r_meshes, std::vector<LightData> &r_lights);
	void _process_mesh_instance(MeshInstance3D *p_mesh, std::vector<MeshData> &r_meshes);
	void _process_light(Light3D *p_light, std::vector<LightData> &r_lights);

	// Validation
	bool _validate_meshes(const std::vector<MeshData> &p_meshes);

	// Baking stages
	BakeError _bake_direct_light(Ref<LightmapGIData> p_output_data, BakeProgressFunc p_progress = nullptr, void *p_userdata = nullptr);
	BakeError _bake_indirect_light(Vector<Ref<Image>> &p_lightmaps, BakeProgressFunc p_progress = nullptr, void *p_userdata = nullptr);
	BakeError _bake_light_probes(Ref<LightmapGIData> p_output_data, BakeProgressFunc p_progress = nullptr, void *p_userdata = nullptr);

	// Post-processing
	void _apply_seam_blending(Vector<Ref<Image>> &p_textures);
	void _apply_denoising(Vector<Ref<Image>> &p_textures);
	void _dilate_lightmaps(Vector<Ref<Image>> &p_lightmaps, int p_dilation_radius = 1);

	// Texture management
	Ref<Image> _create_lightmap_image(int p_width, int p_height);
	Vector<Ref<Image>> _pack_lightmaps_to_atlas(std::vector<MeshData> &p_meshes, const Vector<Ref<Image>> &p_lightmaps, int p_atlas_size, int p_padding);
	Ref<Texture2DArray> _create_texture_array_from_images(const Vector<Ref<Image>> &p_layers);
	void _write_output_data(Ref<LightmapGIData> p_output_data, const Ref<Texture2DArray> &p_tex_array);

	// CPU rasterization in UV2 space
	void _rasterize_mesh_direct_lighting(const MeshData &p_mesh, Ref<Image> p_target);
	Color _evaluate_direct_lighting(const Vector3 &p_world_pos, const Vector3 &p_world_normal) const;
	bool _is_shadowed(const Vector3 &p_world_pos, const Vector3 &p_world_normal, const LightData &p_light) const;
	void _build_ray_meshes();

	// Utility
	void _report_progress(float p_progress, const String &p_status, BakeProgressFunc p_callback, void *p_userdata);
	bool _check_cancel_requested();

	// GPU resources (will be used in Phase 2)
	// RID compute_pipeline;
	// Vector<RID> gpu_textures;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::LightmapBaker::BakeQuality);
VARIANT_ENUM_CAST(godot::LightmapBaker::BakeError);

#endif // LIGHTMAP_BAKER_H
