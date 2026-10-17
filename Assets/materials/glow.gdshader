// Unlit emissive glow (port of M_Glow): Color x Intensity, no lighting
shader_type spatial;
render_mode unshaded, cull_back, shadows_disabled;

uniform vec4 color : source_color = vec4(0.1, 1.0, 0.6, 1.0);
uniform float intensity = 8.0;

void fragment() {
	// UE's default intensity (8) maps to the plain color; brighter uses go over 1 and bloom
	ALBEDO = color.rgb * clamp(intensity / 8.0, 0.0, 4.0);
}
