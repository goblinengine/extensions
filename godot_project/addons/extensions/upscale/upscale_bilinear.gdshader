shader_type canvas_item;

uniform sampler2D source_tex;

void fragment() {
	COLOR = texture(source_tex, UV);
}
