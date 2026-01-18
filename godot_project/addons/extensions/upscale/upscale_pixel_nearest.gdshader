shader_type canvas_item;

uniform sampler2D source_tex;
uniform vec2 source_size = vec2(1.0);
uniform vec2 output_size = vec2(1.0);
uniform int pixel_scale_factor = 2;

void fragment() {
	// Pixel-art friendly: snap output pixels to integer source pixels.
	int f = max(pixel_scale_factor, 1);
	vec2 out_px = floor(UV * output_size);
	vec2 scaled_px = floor(out_px / float(f));
	vec2 src_uv = (scaled_px + vec2(0.5)) / source_size;
	COLOR = texture(source_tex, src_uv);
}
