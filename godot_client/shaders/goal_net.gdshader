shader_type spatial;
render_mode cull_disabled, blend_mix, depth_draw_never, unshaded;

uniform vec3 net_color : source_color = vec3(0.95, 0.96, 0.98);
uniform float cell_size = 0.14;
uniform float line_thickness = 0.012;
uniform float net_alpha = 0.35;

varying vec2 v_net_uv;

void vertex() {
	v_net_uv = VERTEX.xy;
}

void fragment() {
	// Diamond grid rotated 45 degrees in exact local meters
	vec2 rotated = vec2(v_net_uv.x + v_net_uv.y, v_net_uv.x - v_net_uv.y) * 0.70710678;
	vec2 grid = abs(fract(rotated / cell_size + 0.5) - 0.5) * cell_size;
	float d = min(grid.x, grid.y);
	float aa = max(fwidth(d), 0.004);
	float strand = 1.0 - smoothstep(line_thickness - aa, line_thickness + aa, d);

	ALBEDO = net_color;
	ALPHA = strand * net_alpha + 0.03 * (1.0 - strand);
}
