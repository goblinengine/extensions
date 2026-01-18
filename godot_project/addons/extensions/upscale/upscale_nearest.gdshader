shader_type canvas_item;

uniform sampler2D source_tex;
uniform vec2 source_size = vec2(1.0);

vec2 nearest_uv(vec2 uv) {
	vec2 px = uv * source_size;
	vec2 ip = floor(px) + vec2(0.5);
	return ip / source_size;
}

void fragment() {
	COLOR = texture(source_tex, nearest_uv(UV));
}
