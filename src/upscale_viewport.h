#pragma once

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class UpscaleViewport : public Control {
	GDCLASS(UpscaleViewport, Control);

public:
		enum Upscaler {
			UPSCALER_BILINEAR = 0,
			UPSCALER_NEAREST = 1,
			UPSCALER_FSR1 = 2,
			UPSCALER_CT1 = 3,
			UPSCALER_CT2 = 4,
			UPSCALER_PIXEL_NEAREST = 10,
			UPSCALER_CUSTOM_SINGLE_PASS = 100,
		};

	enum ResolutionMode {
		RESOLUTION_MODE_SCALE = 0,
		RESOLUTION_MODE_PRESERVE_WIDTH = 1,
		RESOLUTION_MODE_PRESERVE_HEIGHT = 2,
	};

	UpscaleViewport();
	~UpscaleViewport();

	void set_enabled(bool p_enabled);
	bool is_enabled() const { return enabled; }

	void set_reparent_children(bool p_enabled);
	bool is_reparent_children() const { return reparent_children; }

	void set_render_scale(float p_render_scale);
	float get_render_scale() const { return render_scale; }

	void set_min_render_size(Vector2i p_size);
	Vector2i get_min_render_size() const { return min_render_size; }

	void set_resolution_mode(ResolutionMode p_mode);
	ResolutionMode get_resolution_mode() const { return resolution_mode; }

	void set_upscaler(Upscaler p_upscaler);
	Upscaler get_upscaler() const { return upscaler; }

	void set_pixel_scale_factor(int32_t p_factor);
	int32_t get_pixel_scale_factor() const { return pixel_scale_factor; }

	void set_sharpness(float p_sharpness);
	float get_sharpness() const { return sharpness; }

	void set_custom_upscaler_shader(const Ref<Shader> &p_shader);
	Ref<Shader> get_custom_upscaler_shader() const { return custom_upscaler_shader; }

	Node *get_content_root() const { return content_root; }

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void _gui_input(const Ref<InputEvent> &p_event) override;
	void _input(const Ref<InputEvent> &p_event) override;
	void _unhandled_input(const Ref<InputEvent> &p_event) override;

private:
	bool enabled = true;
	bool reparent_children = true;
	float render_scale = 0.5f;
	Vector2i min_render_size = Vector2i(320, 180);
	ResolutionMode resolution_mode = RESOLUTION_MODE_SCALE;
	Upscaler upscaler = UPSCALER_FSR1;
	int32_t pixel_scale_factor = 2;
	float sharpness = 0.2f;

	SubViewport *render_viewport = nullptr;
	SubViewport *pass1_viewport = nullptr;
	Node *content_root = nullptr;
	ColorRect *pass1_rect = nullptr;
	ColorRect *final_rect = nullptr;

	Ref<ShaderMaterial> pass1_material;
	Ref<ShaderMaterial> final_material;
	Ref<Shader> custom_upscaler_shader;

	bool reparent_queued = false;

	void ensure_internal_nodes();
	void queue_reparent_foreign_children();
	void reparent_foreign_children();

	Vector2i get_output_size() const;
	Vector2i compute_render_size(Vector2i p_output_size) const;

	void update_pipeline();
	void update_viewport_sizes();
	void update_shader_params();

	void push_scaled_input_to_render_viewport(const Ref<InputEvent> &p_event, bool p_unhandled);
};

} // namespace godot

VARIANT_ENUM_CAST(UpscaleViewport::Upscaler);
VARIANT_ENUM_CAST(UpscaleViewport::ResolutionMode);
