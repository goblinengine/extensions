#include "upscale_viewport.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr const char *META_INTERNAL = "__upscale_viewport_internal";

static Ref<Shader> _try_load_shader(const String &p_path) {
	Ref<Resource> res = ResourceLoader::get_singleton()->load(p_path);
	Ref<Shader> shader = res;
	return shader;
}

UpscaleViewport::UpscaleViewport() {
	set_clip_contents(true);
	// Don't block UI layered above this Control.
	set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	set_process_input(true);
	set_process_unhandled_input(true);
}

UpscaleViewport::~UpscaleViewport() {
	// Godot will free children.
}

void UpscaleViewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &UpscaleViewport::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &UpscaleViewport::is_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");

	ClassDB::bind_method(D_METHOD("set_reparent_children", "enabled"), &UpscaleViewport::set_reparent_children);
	ClassDB::bind_method(D_METHOD("is_reparent_children"), &UpscaleViewport::is_reparent_children);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reparent_children"), "set_reparent_children", "is_reparent_children");

	ClassDB::bind_method(D_METHOD("set_render_scale", "scale"), &UpscaleViewport::set_render_scale);
	ClassDB::bind_method(D_METHOD("get_render_scale"), &UpscaleViewport::get_render_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_scale", PROPERTY_HINT_RANGE, "0.1,1.0,0.01"), "set_render_scale", "get_render_scale");

	ClassDB::bind_method(D_METHOD("set_min_render_size", "size"), &UpscaleViewport::set_min_render_size);
	ClassDB::bind_method(D_METHOD("get_min_render_size"), &UpscaleViewport::get_min_render_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "min_render_size"), "set_min_render_size", "get_min_render_size");

	ClassDB::bind_method(D_METHOD("set_resolution_mode", "mode"), &UpscaleViewport::set_resolution_mode);
	ClassDB::bind_method(D_METHOD("get_resolution_mode"), &UpscaleViewport::get_resolution_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "resolution_mode", PROPERTY_HINT_ENUM, "Scale,Preserve Width,Preserve Height"), "set_resolution_mode", "get_resolution_mode");

	ClassDB::bind_method(D_METHOD("set_upscaler", "upscaler"), &UpscaleViewport::set_upscaler);
	ClassDB::bind_method(D_METHOD("get_upscaler"), &UpscaleViewport::get_upscaler);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "upscaler", PROPERTY_HINT_ENUM, "Bilinear,Nearest,FSR1,CT1,CT2,Pixel Nearest,Custom (single pass)"), "set_upscaler", "get_upscaler");

	ClassDB::bind_method(D_METHOD("set_pixel_scale_factor", "factor"), &UpscaleViewport::set_pixel_scale_factor);
	ClassDB::bind_method(D_METHOD("get_pixel_scale_factor"), &UpscaleViewport::get_pixel_scale_factor);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pixel_scale_factor", PROPERTY_HINT_RANGE, "1,8,1"), "set_pixel_scale_factor", "get_pixel_scale_factor");

	ClassDB::bind_method(D_METHOD("set_sharpness", "sharpness"), &UpscaleViewport::set_sharpness);
	ClassDB::bind_method(D_METHOD("get_sharpness"), &UpscaleViewport::get_sharpness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sharpness", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_sharpness", "get_sharpness");

	ClassDB::bind_method(D_METHOD("set_custom_upscaler_shader", "shader"), &UpscaleViewport::set_custom_upscaler_shader);
	ClassDB::bind_method(D_METHOD("get_custom_upscaler_shader"), &UpscaleViewport::get_custom_upscaler_shader);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_upscaler_shader", PROPERTY_HINT_RESOURCE_TYPE, "Shader"), "set_custom_upscaler_shader", "get_custom_upscaler_shader");

	ClassDB::bind_method(D_METHOD("get_content_root"), &UpscaleViewport::get_content_root);
	// Internal helper (used via call_deferred).
	ClassDB::bind_method(D_METHOD("_reparent_foreign_children"), &UpscaleViewport::reparent_foreign_children);

	BIND_ENUM_CONSTANT(UPSCALER_BILINEAR);
	BIND_ENUM_CONSTANT(UPSCALER_NEAREST);
	BIND_ENUM_CONSTANT(UPSCALER_FSR1);
	BIND_ENUM_CONSTANT(UPSCALER_CT1);
	BIND_ENUM_CONSTANT(UPSCALER_CT2);
	BIND_ENUM_CONSTANT(UPSCALER_PIXEL_NEAREST);
	BIND_ENUM_CONSTANT(UPSCALER_CUSTOM_SINGLE_PASS);

	BIND_ENUM_CONSTANT(RESOLUTION_MODE_SCALE);
	BIND_ENUM_CONSTANT(RESOLUTION_MODE_PRESERVE_WIDTH);
	BIND_ENUM_CONSTANT(RESOLUTION_MODE_PRESERVE_HEIGHT);
}

void UpscaleViewport::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_READY: {
			ensure_internal_nodes();
			update_pipeline();
			update_viewport_sizes();
			update_shader_params();
			if (reparent_children && !Engine::get_singleton()->is_editor_hint()) {
				queue_reparent_foreign_children();
			}
		} break;
		case NOTIFICATION_RESIZED: {
			update_viewport_sizes();
			update_shader_params();
		} break;
		case NOTIFICATION_CHILD_ORDER_CHANGED: {
			if (reparent_children && !Engine::get_singleton()->is_editor_hint()) {
				queue_reparent_foreign_children();
			}
		} break;
		default:
			break;
	}
}

void UpscaleViewport::ensure_internal_nodes() {
	if (render_viewport) {
		return;
	}

	render_viewport = memnew(SubViewport);
	render_viewport->set_name("_RenderViewport");
	render_viewport->set_meta(META_INTERNAL, true);
	render_viewport->set_disable_3d(false);
	render_viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
	render_viewport->set_transparent_background(false);
	render_viewport->set_handle_input_locally(true);
	render_viewport->set_disable_input(false);
	add_child(render_viewport);

	content_root = memnew(Node);
	content_root->set_name("_ContentRoot");
	content_root->set_meta(META_INTERNAL, true);
	render_viewport->add_child(content_root);

	// Optional pass-1 viewport (created lazily in update_pipeline)

	final_rect = memnew(ColorRect);
	final_rect->set_name("_FinalRect");
	final_rect->set_meta(META_INTERNAL, true);
	final_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	final_rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(final_rect);
}

void UpscaleViewport::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}
	enabled = p_enabled;
	update_pipeline();
	update_viewport_sizes();
	update_shader_params();
}

void UpscaleViewport::set_reparent_children(bool p_enabled) {
	if (reparent_children == p_enabled) {
		return;
	}
	reparent_children = p_enabled;
	if (reparent_children) {
		queue_reparent_foreign_children();
	}
}

void UpscaleViewport::set_render_scale(float p_render_scale) {
	p_render_scale = CLAMP(p_render_scale, 0.1f, 1.0f);
	if (Math::is_equal_approx(render_scale, p_render_scale)) {
		return;
	}
	render_scale = p_render_scale;
	update_viewport_sizes();
	update_shader_params();
}

void UpscaleViewport::set_min_render_size(Vector2i p_size) {
	p_size.x = MAX(1, p_size.x);
	p_size.y = MAX(1, p_size.y);
	if (min_render_size == p_size) {
		return;
	}
	min_render_size = p_size;
	update_viewport_sizes();
	update_shader_params();
}

void UpscaleViewport::set_resolution_mode(ResolutionMode p_mode) {
	if (resolution_mode == p_mode) {
		return;
	}
	resolution_mode = p_mode;
	update_viewport_sizes();
	update_shader_params();
}

void UpscaleViewport::set_upscaler(Upscaler p_upscaler) {
	if (upscaler == p_upscaler) {
		return;
	}
	upscaler = p_upscaler;
	update_pipeline();
	update_shader_params();
}

void UpscaleViewport::set_pixel_scale_factor(int32_t p_factor) {
	p_factor = CLAMP(p_factor, 1, 8);
	if (pixel_scale_factor == p_factor) {
		return;
	}
	pixel_scale_factor = p_factor;
	update_shader_params();
}

void UpscaleViewport::set_sharpness(float p_sharpness) {
	p_sharpness = CLAMP(p_sharpness, 0.0f, 2.0f);
	if (Math::is_equal_approx(sharpness, p_sharpness)) {
		return;
	}
	sharpness = p_sharpness;
	update_shader_params();
}

void UpscaleViewport::set_custom_upscaler_shader(const Ref<Shader> &p_shader) {
	custom_upscaler_shader = p_shader;
	if (upscaler == UPSCALER_CUSTOM_SINGLE_PASS) {
		update_pipeline();
		update_shader_params();
	}
}

Vector2i UpscaleViewport::get_output_size() const {
	// Prefer the parent viewport's visible rect so this behaves correctly with Stretch/Extend.
	Viewport *vp = get_viewport();
	if (vp) {
		Rect2 vr = vp->get_visible_rect();
		Vector2 vs = vr.size;
		int32_t vx = (int32_t)Math::round(vs.x);
		int32_t vy = (int32_t)Math::round(vs.y);
		if (vx > 0 && vy > 0) {
			return Vector2i(vx, vy);
		}
	}
	Vector2 s = get_size();
	Vector2i si((int32_t)Math::round(s.x), (int32_t)Math::round(s.y));
	return Vector2i(MAX(1, si.x), MAX(1, si.y));
}

Vector2i UpscaleViewport::compute_render_size(Vector2i p_output_size) const {
	Vector2 target = Vector2(p_output_size) * render_scale;
	Vector2i rs((int32_t)Math::round(target.x), (int32_t)Math::round(target.y));

	switch (resolution_mode) {
		case RESOLUTION_MODE_PRESERVE_WIDTH: {
			float aspect = (p_output_size.y <= 0) ? 1.0f : (float)p_output_size.y / (float)p_output_size.x;
			rs.y = (int32_t)Math::round((float)rs.x * aspect);
		} break;
		case RESOLUTION_MODE_PRESERVE_HEIGHT: {
			float inv_aspect = (p_output_size.x <= 0) ? 1.0f : (float)p_output_size.x / (float)p_output_size.y;
			rs.x = (int32_t)Math::round((float)rs.y * inv_aspect);
		} break;
		case RESOLUTION_MODE_SCALE:
		default:
			break;
	}

	rs.x = MAX(min_render_size.x, MAX(1, rs.x));
	rs.y = MAX(min_render_size.y, MAX(1, rs.y));
	return rs;
}

void UpscaleViewport::update_pipeline() {
	ensure_internal_nodes();

	// Decide whether we need a 2-pass pipeline.
	bool wants_two_pass = enabled && (upscaler == UPSCALER_FSR1 || upscaler == UPSCALER_CT2);

	if (!wants_two_pass) {
		if (pass1_viewport) {
			pass1_viewport->queue_free();
			pass1_viewport = nullptr;
			pass1_rect = nullptr;
		}
		pass1_material.unref();
	}

	if (!enabled) {
		final_rect->set_visible(false);
		return;
	}
	final_rect->set_visible(true);

	if (wants_two_pass && !pass1_viewport) {
		pass1_viewport = memnew(SubViewport);
		pass1_viewport->set_name("_Pass1Viewport");
		pass1_viewport->set_meta(META_INTERNAL, true);
		pass1_viewport->set_disable_3d(true);
		pass1_viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
		pass1_viewport->set_transparent_background(false);
		add_child(pass1_viewport);

		pass1_rect = memnew(ColorRect);
		pass1_rect->set_name("_Pass1Rect");
		pass1_rect->set_meta(META_INTERNAL, true);
		pass1_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		pass1_rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		pass1_viewport->add_child(pass1_rect);
	}

	if (upscaler == UPSCALER_FSR1) {
		Ref<Shader> easu = _try_load_shader("res://addons/extensions/upscale/upscale_fsr1_easu.gdshader");
		Ref<Shader> rcas = _try_load_shader("res://addons/extensions/upscale/upscale_fsr1_rcas.gdshader");

		if (easu.is_null() || rcas.is_null()) {
			UtilityFunctions::push_warning("UpscaleViewport: FSR shaders not found at res://addons/extensions/upscale/ (using fallback bilinear).\n");
			upscaler = UPSCALER_BILINEAR;
			update_pipeline();
			return;
		}

		if (!pass1_material.is_valid()) {
			pass1_material.instantiate();
		}
		pass1_material->set_shader(easu);
		pass1_rect->set_material(pass1_material);

		if (!final_material.is_valid()) {
			final_material.instantiate();
		}
		final_material->set_shader(rcas);
		final_rect->set_material(final_material);
		return;
	}

	if (upscaler == UPSCALER_CT2) {
		Ref<Shader> ct2_up = _try_load_shader("res://addons/extensions/upscale/upscale_ct2_upscale.gdshader");
		Ref<Shader> ct2_sh = _try_load_shader("res://addons/extensions/upscale/upscale_ct2_sharpen.gdshader");

		if (ct2_up.is_null() || ct2_sh.is_null()) {
			UtilityFunctions::push_warning("UpscaleViewport: CT2 shaders not found at res://addons/extensions/upscale/ (using fallback CT1).\n");
			upscaler = UPSCALER_CT1;
			update_pipeline();
			return;
		}

		if (!pass1_material.is_valid()) {
			pass1_material.instantiate();
		}
		pass1_material->set_shader(ct2_up);
		pass1_rect->set_material(pass1_material);

		if (!final_material.is_valid()) {
			final_material.instantiate();
		}
		final_material->set_shader(ct2_sh);
		final_rect->set_material(final_material);
		return;
	}

	// Single-pass material.
	Ref<Shader> shader;
	switch (upscaler) {
		case UPSCALER_CT1:
			shader = _try_load_shader("res://addons/extensions/upscale/upscale_ct1.gdshader");
			break;
		case UPSCALER_NEAREST:
			shader = _try_load_shader("res://addons/extensions/upscale/upscale_nearest.gdshader");
			break;
		case UPSCALER_PIXEL_NEAREST:
			shader = _try_load_shader("res://addons/extensions/upscale/upscale_pixel_nearest.gdshader");
			break;
		case UPSCALER_CUSTOM_SINGLE_PASS:
			shader = custom_upscaler_shader;
			break;
		case UPSCALER_BILINEAR:
		default:
			shader = _try_load_shader("res://addons/extensions/upscale/upscale_bilinear.gdshader");
			break;
	}

	if (shader.is_null()) {
		UtilityFunctions::push_warning("UpscaleViewport: Upscaler shader missing; falling back to bilinear.\n");
		shader = _try_load_shader("res://addons/extensions/upscale/upscale_bilinear.gdshader");
	}

	if (!final_material.is_valid()) {
		final_material.instantiate();
	}
	final_material->set_shader(shader);
	final_rect->set_material(final_material);
}

void UpscaleViewport::update_viewport_sizes() {
	if (!render_viewport) {
		return;
	}
	Vector2i out_size = get_output_size();
	Vector2i render_size = compute_render_size(out_size);

	render_viewport->set_size(render_size);
	// Critical: keep a stable 2D coordinate system matching the output.
	// This avoids distortion when your project uses Stretch/Extend for canvas items.
	render_viewport->set_size_2d_override(out_size);
	render_viewport->set_size_2d_override_stretch(true);

	if (pass1_viewport) {
		pass1_viewport->set_size(out_size);
		pass1_viewport->set_size_2d_override(out_size);
		pass1_viewport->set_size_2d_override_stretch(true);
	}
}

void UpscaleViewport::update_shader_params() {
	if (!enabled || !render_viewport || !final_material.is_valid()) {
		return;
	}

	Vector2i out_size = get_output_size();
	Vector2i render_size = render_viewport->get_size();

	Ref<Texture2D> input_tex;
	Vector2i input_size;
	if (pass1_viewport) {
		input_tex = pass1_viewport->get_texture();
		input_size = pass1_viewport->get_size();
	} else {
		input_tex = render_viewport->get_texture();
		input_size = render_size;
	}

	final_material->set_shader_parameter("source_tex", input_tex);
	final_material->set_shader_parameter("source_size", Vector2(input_size));
	final_material->set_shader_parameter("output_size", Vector2(out_size));
	final_material->set_shader_parameter("sharpness", sharpness);
	final_material->set_shader_parameter("strength", sharpness * 0.25f);
	final_material->set_shader_parameter("pixel_scale_factor", pixel_scale_factor);

	if (pass1_material.is_valid() && pass1_viewport) {
		pass1_material->set_shader_parameter("source_tex", render_viewport->get_texture());
		pass1_material->set_shader_parameter("source_size", Vector2(render_size));
		pass1_material->set_shader_parameter("output_size", Vector2(out_size));
	}
}

void UpscaleViewport::queue_reparent_foreign_children() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (reparent_queued) {
		return;
	}
	reparent_queued = true;
	call_deferred("_reparent_foreign_children");
}

void UpscaleViewport::reparent_foreign_children() {
	reparent_queued = false;
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (!reparent_children) {
		return;
	}
	if (!is_inside_tree() || !content_root) {
		return;
	}

	// Move any non-internal children under the render viewport's content root.
	TypedArray<Node> to_move;
	for (int i = 0; i < get_child_count(); i++) {
		Node *c = Object::cast_to<Node>(get_child(i));
		if (!c) {
			continue;
		}
		Variant is_internal = c->get_meta(META_INTERNAL, Variant(false));
		if ((bool)is_internal) {
			continue;
		}
		to_move.append(c);
	}

	for (int i = 0; i < to_move.size(); i++) {
		Node *n = Object::cast_to<Node>(to_move[i]);
		if (!n || n->get_parent() != this) {
			continue;
		}
		remove_child(n);
		content_root->add_child(n);
	}
}

void UpscaleViewport::_gui_input(const Ref<InputEvent> &p_event) {
	// Intentionally empty: we forward input via Node::_input/_unhandled_input
	// so mouse capture + non-GUI keys work consistently.
	(void)p_event;
}

void UpscaleViewport::_input(const Ref<InputEvent> &p_event) {
	push_scaled_input_to_render_viewport(p_event, false);
}

void UpscaleViewport::_unhandled_input(const Ref<InputEvent> &p_event) {
	push_scaled_input_to_render_viewport(p_event, true);
}

void UpscaleViewport::push_scaled_input_to_render_viewport(const Ref<InputEvent> &p_event, bool p_unhandled) {
	if (!enabled || !render_viewport || p_event.is_null()) {
		return;
	}
	Ref<InputEvent> ev = p_event->duplicate();
	if (ev.is_null()) {
		return;
	}

	Vector2i out_size = get_output_size();
	Vector2i render_size = render_viewport->get_size();

	Ref<InputEventMouse> m = ev;
	if (m.is_valid()) {
		Vector2 pos = m->get_position();
		Vector2 scaled = Vector2(
			(out_size.x > 0) ? (pos.x * (float)render_size.x / (float)out_size.x) : pos.x,
			(out_size.y > 0) ? (pos.y * (float)render_size.y / (float)out_size.y) : pos.y);
		m->set_position(scaled);
		m->set_global_position(scaled);
	}
	Ref<InputEventScreenTouch> t = ev;
	if (t.is_valid()) {
		Vector2 pos = t->get_position();
		Vector2 scaled = Vector2(
			(out_size.x > 0) ? (pos.x * (float)render_size.x / (float)out_size.x) : pos.x,
			(out_size.y > 0) ? (pos.y * (float)render_size.y / (float)out_size.y) : pos.y);
		t->set_position(scaled);
	}
	Ref<InputEventScreenDrag> d = ev;
	if (d.is_valid()) {
		Vector2 pos = d->get_position();
		Vector2 rel = d->get_relative();
		Vector2 scaled_pos = Vector2(
			(out_size.x > 0) ? (pos.x * (float)render_size.x / (float)out_size.x) : pos.x,
			(out_size.y > 0) ? (pos.y * (float)render_size.y / (float)out_size.y) : pos.y);
		Vector2 scaled_rel = Vector2(
			(out_size.x > 0) ? (rel.x * (float)render_size.x / (float)out_size.x) : rel.x,
			(out_size.y > 0) ? (rel.y * (float)render_size.y / (float)out_size.y) : rel.y);
		d->set_position(scaled_pos);
		d->set_relative(scaled_rel);
	}

	if (p_unhandled) {
		render_viewport->push_unhandled_input(ev, true);
	} else {
		render_viewport->push_input(ev, true);
	}
}

} // namespace godot
