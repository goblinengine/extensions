shader_type canvas_item;

// CT2 pass 2: edge-adaptive sharpening on the upscaled image.
// This is an original implementation (not CUT) intended to improve soft edges.

uniform sampler2D source_tex;
uniform vec2 source_size = vec2(1.0);

uniform float strength = 0.25; // 0..1

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 tap(vec2 uv) {
	return texture(source_tex, uv).rgb;
}

void fragment() {
	vec2 texel = 1.0 / source_size;

	vec3 c = tap(UV);
	vec3 n = tap(UV + vec2(0.0, -texel.y));
	vec3 s = tap(UV + vec2(0.0, texel.y));
	vec3 w = tap(UV + vec2(-texel.x, 0.0));
	vec3 e = tap(UV + vec2(texel.x, 0.0));

	float lc = luma(c);
	float gx = luma(e) - luma(w);
	float gy = luma(s) - luma(n);
	float edge = clamp(sqrt(gx * gx + gy * gy) * 2.0, 0.0, 1.0);

	// Local blur for unsharp mask.
	vec3 blur = (n + s + w + e) * 0.25;
	vec3 detail = c - blur;

	// Stronger on soft edges, weaker on flat/noisy regions.
	float amt = strength * smoothstep(0.05, 0.25, edge);
	vec3 outc = c + detail * amt;

	COLOR = vec4(clamp(outc, 0.0, 1.0), 1.0);
}
