// Black, starry space with no atmosphere (port of M_SpaceSky: a squared star texture,
// which keeps the bright stars and drops faint haze to black)
shader_type sky;

uniform float star_density : hint_range(20.0, 600.0) = 220.0;
// Fraction of cells that hold a star
uniform float star_chance : hint_range(0.0, 1.0) = 0.25;
// Star radius as a fraction of a cell
uniform float star_size : hint_range(0.01, 0.5) = 0.2;
uniform float star_brightness = 2.5;

vec3 hash3(vec3 p) {
	p = vec3(dot(p, vec3(127.1, 311.7, 74.7)), dot(p, vec3(269.5, 183.3, 246.1)), dot(p, vec3(113.5, 271.9, 124.6)));
	return fract(sin(p) * 43758.5453123);
}

void sky() {
	vec3 scaled = EYEDIR * star_density;
	vec3 cell = floor(scaled);
	vec3 rnd = hash3(cell);
	vec3 offset = scaled - (cell + 0.2 + rnd * 0.6);
	float dist = length(offset);
	float present = step(1.0 - star_chance, rnd.x * 0.5 + rnd.y * 0.5);
	float star = present * smoothstep(star_size, 0.0, dist);
	// Squaring keeps the bright stars and drops the faint ones to black
	star *= star * (0.4 + 0.6 * rnd.z);
	vec3 color = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.75), rnd.y);
	COLOR = color * star * star_brightness;
}
