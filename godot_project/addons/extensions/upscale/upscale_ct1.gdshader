shader_type canvas_item;

// CT1 (Cheap Triangulation v1) - original implementation for this project.
// Goal: very low-cost edge-aware interpolation using a 2x2 neighborhood.

uniform sampler2D source_tex;
uniform vec2 source_size = vec2(1.0);

float luma(vec3 c) {
	// Simple perceptual luma.
	return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 sample_px(ivec2 ip) {
	ip = clamp(ip, ivec2(0), ivec2(source_size) - ivec2(1));
	vec2 uv = (vec2(ip) + vec2(0.5)) / source_size;
	return texture(source_tex, uv).rgb;
}

vec3 tri_interp_diag00_11(vec3 c00, vec3 c10, vec3 c01, vec3 c11, vec2 f) {
	// Split along y = x (diagonal from (0,0) to (1,1)).
	if (f.y > f.x) {
		// Triangle: (0,0)=c00, (0,1)=c01, (1,1)=c11
		float w00 = 1.0 - f.y;
		float w01 = f.y - f.x;
		float w11 = f.x;
		return c00 * w00 + c01 * w01 + c11 * w11;
	} else {
		// Triangle: (0,0)=c00, (1,0)=c10, (1,1)=c11
		float w00 = 1.0 - f.x;
		float w10 = f.x - f.y;
		float w11 = f.y;
		return c00 * w00 + c10 * w10 + c11 * w11;
	}
}

vec3 tri_interp_diag10_01(vec3 c00, vec3 c10, vec3 c01, vec3 c11, vec2 f) {
	// Split along y = 1-x (diagonal from (1,0) to (0,1)).
	float t = 1.0 - f.x;
	if (f.y > t) {
		// Triangle: (1,0)=c10, (1,1)=c11, (0,1)=c01
		// Coordinates in that tri: shift origin to (1,0): p=(f.x-1, f.y-0)
		// Use a simple area-based weight formulation.
		float w10 = 1.0 - f.y;
		float w11 = f.y - t;
		float w01 = t;
		return c10 * w10 + c11 * w11 + c01 * w01;
	} else {
		// Triangle: (0,0)=c00, (1,0)=c10, (0,1)=c01
		float w00 = 1.0 - f.x - f.y;
		float w10 = f.x;
		float w01 = f.y;
		return c00 * w00 + c10 * w10 + c01 * w01;
	}
}

void fragment() {
	// Map output UV -> source pixel space.
	vec2 p = UV * source_size - vec2(0.5);
	ivec2 base = ivec2(floor(p));
	vec2 f = fract(p);

	vec3 c00 = sample_px(base + ivec2(0, 0));
	vec3 c10 = sample_px(base + ivec2(1, 0));
	vec3 c01 = sample_px(base + ivec2(0, 1));
	vec3 c11 = sample_px(base + ivec2(1, 1));

	float d00_11 = abs(luma(c00) - luma(c11));
	float d10_01 = abs(luma(c10) - luma(c01));

	// Choose diagonal that connects the more similar pair.
	bool use_diag00_11 = (d00_11 <= d10_01);

	vec3 col = use_diag00_11
		? tri_interp_diag00_11(c00, c10, c01, c11, f)
		: tri_interp_diag10_01(c00, c10, c01, c11, f);

	COLOR = vec4(col, 1.0);
}
