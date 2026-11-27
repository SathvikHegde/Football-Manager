shader_type spatial;
render_mode cull_back, diffuse_burley, specular_schlick_ggx;

uniform vec3 grass_dark : source_color = vec3(0.11, 0.34, 0.14);
uniform vec3 grass_light : source_color = vec3(0.16, 0.43, 0.18);
uniform vec3 apron_color : source_color = vec3(0.08, 0.26, 0.11);
uniform vec3 line_color : source_color = vec3(0.93, 0.95, 0.93);

varying vec3 v_pos;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float sd_segment(vec2 p, vec2 a, vec2 b) {
	vec2 pa = p - a;
	vec2 ba = b - a;
	float denom = max(dot(ba, ba), 0.0001);
	float h = clamp(dot(pa, ba) / denom, 0.0, 1.0);
	return length(pa - ba * h);
}

void vertex() {
	v_pos = VERTEX;
}

void fragment() {
	float x = v_pos.x;
	float z = v_pos.z;
	vec2 p = vec2(x, z);
	vec2 p_sym = vec2(abs(x), abs(z));

	// 20 mowed grass stripes across 105m length (5.25m each)
	float stripe_idx = floor((x + 52.5) / 5.25);
	float stripe = mod(stripe_idx, 2.0);
	bool inside_field = (p_sym.x <= 52.5 && p_sym.y <= 34.0);
	vec3 turf = inside_field ? mix(grass_dark, grass_light, stripe) : apron_color;

	// Subtle high-frequency procedural grass grain
	float grain = (value_noise(p * 3.5) - 0.5) * 0.02 + (value_noise(p * 14.0) - 0.5) * 0.012;
	turf = clamp(turf + vec3(grain * 0.6, grain, grain * 0.5), vec3(0.0), vec3(1.0));

	// Exact FIFA / fm_core pitch markings (width 0.12m -> half-width 0.06m)
	float dist = 1000.0;

	// Touchlines (|z| = 34.0, |x| <= 52.5) and Goal lines (|x| = 52.5, |z| <= 34.0)
	dist = min(dist, sd_segment(p_sym, vec2(0.0, 34.0), vec2(52.5, 34.0)));
	dist = min(dist, sd_segment(p_sym, vec2(52.5, 0.0), vec2(52.5, 34.0)));

	// Halfway line (x = 0.0, |z| <= 34.0)
	dist = min(dist, sd_segment(p_sym, vec2(0.0, 0.0), vec2(0.0, 34.0)));

	// Center circle (r = 9.15m)
	dist = min(dist, abs(length(p) - 9.15));

	// Penalty areas (|x| in [36.0, 52.5], |z| <= 20.16)
	dist = min(dist, sd_segment(p_sym, vec2(36.0, 0.0), vec2(36.0, 20.16)));
	dist = min(dist, sd_segment(p_sym, vec2(36.0, 20.16), vec2(52.5, 20.16)));

	// Goal areas / 6-yard boxes (|x| in [47.0, 52.5], |z| <= 9.16)
	dist = min(dist, sd_segment(p_sym, vec2(47.0, 0.0), vec2(47.0, 9.16)));
	dist = min(dist, sd_segment(p_sym, vec2(47.0, 9.16), vec2(52.5, 9.16)));

	// Penalty arcs (center (41.5, 0), r = 9.15m, constrained to |x| < 36.0)
	vec2 pen_rel = p_sym - vec2(41.5, 0.0);
	float pen_max_ang = acos(5.5 / 9.15);
	float pen_ang = (dot(pen_rel, pen_rel) < 0.000001) ? 0.0 : clamp(atan(pen_rel.y, -pen_rel.x), 0.0, pen_max_ang);
	vec2 pen_arc_pt = vec2(41.5, 0.0) + 9.15 * vec2(-cos(pen_ang), sin(pen_ang));
	dist = min(dist, length(p_sym - pen_arc_pt));

	// Corner arcs (center (52.5, 34.0), r = 1.0m, constrained inside pitch)
	vec2 corner_rel = vec2(52.5, 34.0) - p_sym;
	float corner_ang = (dot(corner_rel, corner_rel) < 0.000001) ? 0.0 : clamp(atan(corner_rel.y, corner_rel.x), 0.0, 1.57079633);
	vec2 corner_arc_pt = vec2(52.5, 34.0) - 1.0 * vec2(cos(corner_ang), sin(corner_ang));
	dist = min(dist, length(p_sym - corner_arc_pt));

	// Center spot & penalty spots (r = 0.22m)
	float d_center_spot = length(p);
	float d_pen_spot = length(p_sym - vec2(41.5, 0.0));
	float d_spot = min(d_center_spot, d_pen_spot);

	float half_w = 0.06;
	float footprint = length(fwidth(v_pos.xz)) * 0.5;
	float aa_line = max(max(fwidth(dist), footprint), 0.015);
	float line_mask = 1.0 - smoothstep(half_w - aa_line, half_w + aa_line, dist);

	float aa_spot = max(max(fwidth(d_spot), footprint), 0.015);
	float spot_mask = 1.0 - smoothstep(0.22 - aa_spot, 0.22 + aa_spot, d_spot);

	float white_mask = max(line_mask, spot_mask);

	ALBEDO = mix(turf, line_color, white_mask);
	ROUGHNESS = mix(0.85, 0.72, white_mask);
	SPECULAR = 0.15;
}
