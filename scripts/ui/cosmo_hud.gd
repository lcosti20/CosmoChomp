class_name CosmoHUD
extends Control
## Health, carried orbs, rocket fuel, the ray gun timer, banners and the mini map
## (port of BP_CosmoHUD, drawn on the canvas like UE's Receive Draw HUD).
##
## Mini map: an azimuthal equidistant projection of the planet, centered on the player
## with the camera's heading pointing up. Distance from the map's center is true
## great-circle distance; at a range of 180 degrees the whole planet fits in the circle.

@export_group("HUD")
@export var text_color: Color = Color(0.9, 0.95, 1.0)
@export var fuel_color: Color = Color(0.15, 1.0, 0.55)
@export var ray_gun_color: Color = Color(1.0, 0.2, 0.65)
## The ray gun timer blinks once fewer than this many seconds are left
@export var ray_gun_warning_time: float = 3.0
## Overall HUD size multiplier on top of the 1080p-relative scale
@export var hud_scale: float = 2.0
## Point size of UE's large engine font at scale 1
@export var base_font_size: int = 24

@export_group("Mini Map")
@export var show_mini_map: bool = true
## Map diameter as a fraction of the screen's shorter side; capped so it stays within one quadrant
@export_range(0.1, 0.5) var mini_map_size: float = 0.34
## Great-circle distance (degrees) from the player to the map's rim; 180 shows the whole planet
@export_range(10.0, 180.0) var mini_map_range_degrees: float = 75.0
@export var mini_map_background_color: Color = Color(0.01, 0.02, 0.06, 0.78)
## Tile edges that are open passages
@export var mini_map_tile_color: Color = Color(0.35, 0.45, 0.7, 0.3)
@export var mini_map_wall_color: Color = Color(0.8, 0.88, 1.0, 0.95)
@export var mini_map_orb_color: Color = Color(0.35, 1.0, 0.85)
@export var mini_map_rocket_color: Color = Color(1.0, 0.12, 0.1)
@export var mini_map_ray_gun_color: Color = Color(0.2, 1.0, 0.25)
@export var mini_map_alien_color: Color = Color(1.0, 0.9, 0.1)
@export var mini_map_player_color: Color = Color.WHITE

const EDGE_STEPS := 4

var _font: Font = null

# Mini map view (UE: FMiniMapView)
var _view_center: Vector3 = Vector3.ZERO
var _view_up: Vector3 = Vector3.UP
var _view_forward: Vector3 = Vector3.FORWARD
var _view_right: Vector3 = Vector3.RIGHT
var _view_origin: Vector2 = Vector2.ZERO
var _view_radius: float = 1.0
var _view_range: float = 1.0
## Output of _project
var _projected: Vector2 = Vector2.ZERO

# Mini map triangle batch (UE: FMiniMapBatch)
var _points: PackedVector2Array = PackedVector2Array()
var _colors: PackedColorArray = PackedColorArray()


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_font = ThemeDB.fallback_font


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	var screen := get_viewport_rect().size
	var ui_scale := maxf(screen.y / 1080.0, 0.5) * hud_scale
	var line_height := 34.0 * ui_scale
	var left := 40.0 * ui_scale
	var y := 32.0 * ui_scale

	# Only levels running a Cosmo Chomp round (with a maze) get the HUD
	var game_mode := CosmoGameMode.find(self)
	if game_mode == null or game_mode.get_maze() == null:
		return

	var player := game_mode.get_player()
	var rocket := game_mode.get_rocket()

	if show_mini_map:
		_draw_mini_map(game_mode, player, screen)

	if player and player.has_method("player_get_health"):
		_draw_text("Health  %d / %d" % [player.player_get_health(), player.player_get_max_health()], text_color, left, y, ui_scale)
		y += line_height
		_draw_text("Carrying  %d orbs" % player.player_get_carried_orbs(), text_color, left, y, ui_scale)
		y += line_height

	if is_instance_valid(rocket):
		var required := maxi(rocket.required_orbs, 1)
		_draw_text("Rocket fuel  %d / %d" % [rocket.get_stored_orbs(), required], text_color, left, y, ui_scale)
		y += line_height
		_draw_bar(left, y, 300.0 * ui_scale, 12.0 * ui_scale, float(rocket.get_stored_orbs()) / required, fuel_color)
		y += line_height

	if player and player.has_method("player_has_ray_gun") and player.player_has_ray_gun():
		_draw_ray_gun_timer(player, ui_scale, screen)

	var banner := game_mode.get_banner_text()
	if not banner.is_empty():
		var banner_scale := ui_scale * 1.4
		var banner_size := _text_size(banner, banner_scale)
		var x := (screen.x - banner_size.x) * 0.5
		var banner_y := screen.y * 0.22
		draw_rect(Rect2(x - 24.0, banner_y - 12.0, banner_size.x + 48.0, banner_size.y + 24.0), Color(0.0, 0.0, 0.0, 0.5))
		_draw_text(banner, text_color, x, banner_y, banner_scale)


func _font_size(text_scale: float) -> int:
	return maxi(roundi(base_font_size * text_scale), 1)


func _text_size(text: String, text_scale: float) -> Vector2:
	var font_size := _font_size(text_scale)
	return Vector2(_font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x, _font.get_height(font_size))


## Draws text with its top-left corner at (x, y), like UE's Canvas DrawText.
func _draw_text(text: String, color: Color, x: float, y: float, text_scale: float) -> void:
	var font_size := _font_size(text_scale)
	draw_string(_font, Vector2(x, y + _font.get_ascent(font_size)), text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)


func _draw_bar(x: float, y: float, width: float, height: float, fill: float, color: Color) -> void:
	draw_rect(Rect2(x, y, width, height), Color(0.0, 0.0, 0.0, 0.5))
	draw_rect(Rect2(x, y, width * clampf(fill, 0.0, 1.0), height), color)


## Centered near the bottom: seconds left and a bar that drains with them
func _draw_ray_gun_timer(player: Node, ui_scale: float, screen: Vector2) -> void:
	var remaining: float = player.player_get_ray_gun_time_remaining()
	var fill := remaining / maxf(player.player_get_ray_gun_duration(), 0.0001)
	var label := "RAY GUN  %ds" % ceili(remaining)
	var hint := "Click / F / RT to fire"

	var bar_width := 420.0 * ui_scale
	var bar_height := 16.0 * ui_scale
	var x := (screen.x - bar_width) * 0.5
	var y := screen.y - 130.0 * ui_scale

	# Blink for the last few seconds so running out isn't a surprise
	var running_out := remaining < ray_gun_warning_time
	var blink_off := running_out and fmod(Time.get_ticks_msec() / 1000.0, 0.4) < 0.2
	var color := text_color if blink_off else ray_gun_color

	var label_size := _text_size(label, ui_scale)
	var padding := 12.0 * ui_scale
	draw_rect(Rect2(x - padding, y - padding, bar_width + padding * 2.0, label_size.y + 8.0 * ui_scale + bar_height + padding * 2.0), Color(0.0, 0.0, 0.0, 0.5))
	_draw_text(label, color, x, y, ui_scale)

	var hint_scale := ui_scale * 0.7
	var hint_size := _text_size(hint, hint_scale)
	_draw_text(hint, text_color, x + bar_width - hint_size.x, y + (label_size.y - hint_size.y) * 0.5, hint_scale)

	_draw_bar(x, y + label_size.y + 8.0 * ui_scale, bar_width, bar_height, fill, color)


# Mini map
# ------------------------------------------------

func _draw_mini_map(game_mode: CosmoGameMode, player: Node3D, screen: Vector2) -> void:
	var maze := game_mode.get_maze()
	if player == null or maze == null or maze.get_num_tiles() == 0:
		return

	_view_center = maze.get_planet_center()
	_view_up = (player.global_position - _view_center).normalized()
	if _view_up.is_zero_approx():
		return

	# Map up is where the camera looks, flattened onto the surface; looking straight
	# down, the camera's own up takes over
	var player_forward := -player.global_basis.z.normalized()
	var heading := player_forward
	var camera := get_viewport().get_camera_3d()
	if camera:
		heading = -camera.global_basis.z
		if PlanetMath.project_on_plane(heading, _view_up).length_squared() < 0.01:
			heading = camera.global_basis.y
	_view_forward = PlanetMath.flatten_direction(heading, _view_up)
	if _view_forward.is_zero_approx():
		_view_forward = PlanetMath.flatten_direction(player_forward, _view_up)
	_view_right = _view_forward.cross(_view_up)

	# Top-right corner, never more than a quarter of the screen
	var shorter := minf(screen.x, screen.y)
	var margin := 0.03 * shorter
	_view_radius = minf(mini_map_size * shorter, 0.5 * shorter - 2.0 * margin) * 0.5
	_view_origin = Vector2(screen.x - margin - _view_radius, margin + _view_radius)
	_view_range = deg_to_rad(clampf(mini_map_range_degrees, 10.0, 180.0))

	var unit := _view_radius / 40.0
	_points.clear()
	_colors.clear()
	_disc(_view_origin, _view_radius, mini_map_background_color, 64)

	# Tile edges: faint where open, bright where walled. Each edge is split up because
	# great circles bend under the projection, more so toward the rim.
	var tile_thickness := maxf(unit * 0.3, 1.0)
	var wall_thickness := maxf(unit * 0.6, 1.5)
	for tile: int in maze.get_num_tiles():
		for neighbor: int in maze.tile_neighbors[tile]:
			if neighbor < tile:
				continue
			var edge := maze.get_edge_corners(tile, neighbor)
			if edge.x < 0:
				continue
			var wall := not maze.is_passage(tile, neighbor)
			var a := maze.corners[edge.x]
			var b := maze.corners[edge.y]
			var previous := Vector2.ZERO
			var has_previous := _project(a, false)
			if has_previous:
				previous = _projected
			for step: int in range(1, EDGE_STEPS + 1):
				var point_visible := _project(a.lerp(b, float(step) / EDGE_STEPS).normalized(), false)
				var point := _projected
				# Near the rim the far side of the planet wraps around the circle; skip
				# pieces that would jump across it
				if point_visible and has_previous and previous.distance_to(point) < _view_radius * 0.3:
					_line(previous, point, wall_thickness if wall else tile_thickness, mini_map_wall_color if wall else mini_map_tile_color)
				previous = point
				has_previous = point_visible

	for orb: Node3D in get_tree().get_nodes_in_group("energy_orbs"):
		if _project_location(orb.global_position, false):
			_disc(_projected, unit * 0.45, mini_map_orb_color, 8)

	for alien: Node in get_tree().get_nodes_in_group("aliens"):
		var alien_3d := alien as Alien
		if alien_3d and not alien_3d.is_dead() and _project_location(alien_3d.global_position, false):
			_square(_projected, unit * 1.0, mini_map_alien_color)

	# The rocket and ray gun stay on the rim when out of range, pointing the way to them
	for ray_gun: Node3D in get_tree().get_nodes_in_group("ray_gun_pickups"):
		if _project_location(ray_gun.global_position, true):
			_arrow(_projected, Vector2(1.0, 0.0), unit * 1.6, mini_map_ray_gun_color)

	var rocket := game_mode.get_rocket()
	if is_instance_valid(rocket) and _project_location(rocket.global_position, true):
		_arrow(_projected, Vector2(0.0, -1.0), unit * 1.9, mini_map_rocket_color)

	# The player, pointing where the astronaut faces
	_arrow(_view_origin, _screen_direction(player_forward), unit * 1.5, mini_map_player_color)

	_ring(_view_origin, _view_radius, maxf(unit * 0.5, 1.5), mini_map_wall_color)

	var indices := PackedInt32Array()
	indices.resize(_points.size())
	for index: int in _points.size():
		indices[index] = index
	RenderingServer.canvas_item_add_triangle_array(get_canvas_item(), indices, _points, _colors)


## Azimuthal equidistant projection of a unit direction from the planet center into
## _projected. If clamp_to_rim, things beyond the map's range are pinned to its rim
## (along their bearing) instead of hidden.
func _project(direction: Vector3, clamp_to_rim: bool) -> bool:
	var cos_angle := clampf(_view_up.dot(direction), -1.0, 1.0)
	var angle := acos(cos_angle)
	if angle > _view_range and not clamp_to_rim:
		return false
	# Bearing from the player, measured in the tangent plane at the player
	var tangent := direction - _view_up * cos_angle
	var bearing := Vector2(tangent.dot(_view_right), tangent.dot(_view_forward)).normalized()
	_projected = _view_origin + Vector2(bearing.x, -bearing.y) * (minf(angle / _view_range, 1.0) * _view_radius)
	return true


func _project_location(world_location: Vector3, clamp_to_rim: bool) -> bool:
	return _project((world_location - _view_center).normalized(), clamp_to_rim)


## Screen direction for a world direction along the surface at the player
func _screen_direction(world_direction: Vector3) -> Vector2:
	return Vector2(world_direction.dot(_view_right), -world_direction.dot(_view_forward)).normalized()


func _triangle(a: Vector2, b: Vector2, c: Vector2, color: Color) -> void:
	_points.append_array([a, b, c])
	_colors.append_array([color, color, color])


func _line(a: Vector2, b: Vector2, thickness: float, color: Color) -> void:
	var side := (b - a).normalized().orthogonal() * thickness * 0.5
	if side.is_zero_approx():
		return
	_triangle(a + side, b + side, b - side, color)
	_triangle(a + side, b - side, a - side, color)


func _disc(center: Vector2, radius: float, color: Color, sides: int = 10) -> void:
	for index: int in sides:
		var a0 := TAU * index / sides
		var a1 := TAU * (index + 1) / sides
		_triangle(center, center + Vector2(cos(a0), sin(a0)) * radius, center + Vector2(cos(a1), sin(a1)) * radius, color)


func _ring(center: Vector2, radius: float, thickness: float, color: Color, sides: int = 64) -> void:
	for index: int in sides:
		var a0 := TAU * index / sides
		var a1 := TAU * (index + 1) / sides
		_line(center + Vector2(cos(a0), sin(a0)) * radius, center + Vector2(cos(a1), sin(a1)) * radius, thickness, color)


func _square(center: Vector2, half_size: float, color: Color) -> void:
	var x := Vector2(half_size, 0.0)
	var y := Vector2(0.0, half_size)
	_triangle(center - x - y, center + x - y, center + x + y, color)
	_triangle(center - x - y, center + x + y, center - x + y, color)


## Isosceles triangle pointing along direction (a unit screen vector)
func _arrow(center: Vector2, direction: Vector2, size: float, color: Color) -> void:
	var side := direction.orthogonal()
	_triangle(center + direction * size, center - direction * size * 0.7 + side * size * 0.8, center - direction * size * 0.7 - side * size * 0.8, color)
