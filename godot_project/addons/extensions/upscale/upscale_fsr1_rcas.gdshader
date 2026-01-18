shader_type canvas_item;

// FSR1 RCAS pass (Godot canvas_item port)

#define FSR_RCAS_LIMIT (0.25-(1.0/16.0))

uniform sampler2D source_tex;
uniform vec2 source_size = vec2(1.0);
uniform float sharpness = 0.2;

void FsrRcasCon(out float con, float sharpness_stops) {
	con = exp2(-sharpness_stops);
}

vec3 FsrRcasLoad(vec2 p) {
	vec2 uv = (p + vec2(0.5)) / source_size;
	return texture(source_tex, uv).rgb;
}

vec3 FsrRcasF(vec2 ip, float con) {
	vec2 sp = ip;
	vec3 b = FsrRcasLoad(sp + vec2(0.0, -1.0));
	vec3 d = FsrRcasLoad(sp + vec2(-1.0, 0.0));
	vec3 e = FsrRcasLoad(sp);
	vec3 f = FsrRcasLoad(sp + vec2(1.0, 0.0));
	vec3 h = FsrRcasLoad(sp + vec2(0.0, 1.0));

	float bL = b.g + 0.5 * (b.b + b.r);
	float dL = d.g + 0.5 * (d.b + d.r);
	float eL = e.g + 0.5 * (e.b + e.r);
	float fL = f.g + 0.5 * (f.b + f.r);
	float hL = h.g + 0.5 * (h.b + h.r);

	float nz = 0.25 * (bL + dL + fL + hL) - eL;
	float mx = max(max(bL, dL), max(eL, max(fL, hL)));
	float mn = min(min(bL, dL), min(eL, min(fL, hL)));
	float rng = max(mx - mn, 1e-6);
	nz = clamp(abs(nz) / rng, 0.0, 1.0);
	nz = 1.0 - 0.5 * nz;

	vec3 mn4 = min(b, min(f, h));
	vec3 mx4 = max(b, max(f, h));
	vec2 peakC = vec2(1.0, -4.0);
	vec3 hitMin = mn4 / (4.0 * mx4);
	vec3 hitMax = (peakC.x - mx4) / (4.0 * mn4 + peakC.y);
	vec3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-FSR_RCAS_LIMIT, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * con;

	lobe *= nz;
	return (lobe * (b + d + h + f) + e) / (4.0 * lobe + 1.0);
}

void fragment() {
	float con;
	FsrRcasCon(con, sharpness);
	vec2 ip = floor(UV * source_size);
	vec3 col = FsrRcasF(ip, con);
	COLOR = vec4(col, 1.0);
}
