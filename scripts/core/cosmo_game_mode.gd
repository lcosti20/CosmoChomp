class_name CosmoGameMode
extends Node
## Runs a round of Cosmo Chomp in any level that contains a HexMaze (port of BP_CosmoGameMode).
##
## Generates the maze around the planet, lands the rocket and player on the start tile,
## fills the maze with energy orbs, and keeps aliens and the ray gun spawning until the
## player escapes or is overrun. Also keeps a shared distance field to the player
## (refreshed every 0.3 s) that every alien reads instead of running its own search.

const ROCKET_SCENE := preload("res://scenes/gameplay/rocket.tscn")
const ORB_SCENE := preload("res://scenes/gameplay/energy_orb.tscn")
## Loaded at runtime: the alien script refers back to this class
const ALIEN_SCENE_PATH := "res://scenes/characters/alien.tscn"
const RAY_GUN_SCENE := preload("res://scenes/gameplay/ray_gun_pickup.tscn")

@export_group("Rules")
## Fraction of the maze's orbs that must be delivered to refuel the rocket
@export_range(0.05, 1.0) var orbs_required_fraction: float = 0.5
@export var num_aliens: int = 4
## Grace period before aliens appear
@export var alien_spawn_delay: float = 4.0
@export var alien_respawn_delay: float = 8.0
## Delay before the first ray gun appears, and before a new one appears after it's used up
@export var ray_gun_respawn_delay: float = 15.0
## Also place a ray gun beside the rocket at the start of the round (handy for testing)
@export var ray_gun_at_start: bool = true
## Seconds before the level restarts after winning or losing
@export var restart_delay: float = 6.0
## UE: 70 cm
@export var orb_height: float = 0.7
## Seconds between refreshes of the shared distance field to the player
@export var player_distance_interval: float = 0.3

## The player pawn; found in the "player" group when left empty
@export var player_path: NodePath
## The PlayerStart; the player lands on the tile nearest to it
@export var player_start_path: NodePath

var maze: HexMaze = null
var rocket: Rocket = null
## The ray gun currently lying in the maze; there's never more than one
var active_ray_gun: RayGunPickup = null
var start_tile: int = -1
var round_ready: bool = false
var round_over: bool = false
var banner: String = ""
var banner_end_time: float = 0.0
var player_distances: PackedInt32Array = PackedInt32Array()

## Alien capsule half height plus a little clearance, read from the alien scene
var _alien_half_height: float = 0.9
## Duration of a collected ray gun, read from the pickup scene
var _ray_gun_duration: float = 10.0
var _elapsed: float = 0.0
var _alien_scene: PackedScene = null


## The game mode running the current level (UE: Get Game Mode), or null.
static func find(context: Node) -> CosmoGameMode:
	if context == null or not context.is_inside_tree():
		return null
	return context.get_tree().get_first_node_in_group("game_mode") as CosmoGameMode


func _ready() -> void:
	add_to_group("game_mode")
	_read_class_defaults()
	_play_music()
	# UE: Delay Until Next Tick, so every actor has run BeginPlay first
	_setup_round.call_deferred()
	var distance_timer := Timer.new()
	distance_timer.wait_time = player_distance_interval
	distance_timer.autostart = true
	distance_timer.timeout.connect(_update_player_distances)
	add_child(distance_timer)


func _process(delta: float) -> void:
	_elapsed += delta


## UE: Get Game Time in Seconds (pauses with the game)
func get_game_time() -> float:
	return _elapsed


func _read_class_defaults() -> void:
	_alien_scene = load(ALIEN_SCENE_PATH) as PackedScene
	var alien := _alien_scene.instantiate()
	var capsule := (alien.get_node("CollisionShape3D") as CollisionShape3D).shape as CapsuleShape3D
	_alien_half_height = capsule.height * 0.5
	alien.free()
	var ray_gun := RAY_GUN_SCENE.instantiate() as RayGunPickup
	_ray_gun_duration = ray_gun.duration
	ray_gun.free()


func _play_music() -> void:
	var game_instance := get_node_or_null("/root/GameInstance")
	if game_instance:
		game_instance.start_music()


func get_maze() -> HexMaze:
	return maze


func get_rocket() -> Rocket:
	return rocket


func get_active_ray_gun() -> RayGunPickup:
	return active_ray_gun


## Steps through the maze from the player to every tile (refreshed every 0.3 s).
func get_player_distances() -> PackedInt32Array:
	return player_distances


func get_player() -> Node3D:
	var node := get_node_or_null(player_path) as Node3D if not player_path.is_empty() else null
	return node if node else get_tree().get_first_node_in_group("player") as Node3D


# Round flow
# ------------------------------------------------

func _setup_round() -> void:
	# Levels opt in to a Cosmo Chomp round by placing a HexMaze
	maze = get_tree().get_first_node_in_group("maze") as HexMaze
	if maze == null:
		print("CosmoGameMode: no HexMaze in the level, skipping round setup")
		return

	maze.build_grid()
	start_tile = _compute_start_tile()
	maze.generate_maze(start_tile)

	_spawn_rocket()
	_spawn_orbs()
	round_ready = true
	_place_all_players()
	_update_player_distances()

	get_tree().create_timer(maxf(alien_spawn_delay, 0.01), false).timeout.connect(_spawn_aliens)

	if ray_gun_at_start:
		_spawn_ray_gun_near_rocket()

	# Skipped if the gun by the rocket is still lying there; collecting it schedules the next one
	get_tree().create_timer(maxf(ray_gun_respawn_delay, 0.01), false).timeout.connect(_spawn_ray_gun)

	show_banner("Collect energy orbs and bring them back to your rocket!", 6.0)


## Land on the tile closest to wherever the player started
func _compute_start_tile() -> int:
	var start_location := maze.get_planet_center() + Vector3.UP * maze.get_planet_radius()
	var player := get_player()
	var player_start := get_node_or_null(player_start_path) as Node3D if not player_start_path.is_empty() else null
	if player:
		start_location = player.global_position
	elif player_start:
		start_location = player_start.global_position
	return maze.find_nearest_tile(start_location)


func _spawn_rocket() -> void:
	rocket = ROCKET_SCENE.instantiate() as Rocket
	_add_on_tile(rocket, start_tile, 0.0)
	rocket.launched.connect(_on_rocket_launched)


## One orb per tile, like pellets in each cell of the maze
func _spawn_orbs() -> void:
	var orb_count := 0
	for tile: int in maze.get_num_tiles():
		if tile != start_tile:
			_spawn_orb_on_tile(tile)
			orb_count += 1
	if rocket:
		rocket.required_orbs = clampi(ceili(orb_count * orbs_required_fraction), 1, maxi(orb_count, 1))


func _spawn_orb_on_tile(tile: int) -> void:
	_add_on_tile(ORB_SCENE.instantiate(), tile, orb_height)


func _place_all_players() -> void:
	var player := get_player()
	if player:
		_place_player(player)


## Stand beside the rocket, toward one of the start tile's neighbors, facing the rocket
## (the camera starts behind the player and would otherwise be pushed in by the rocket).
func _place_player(pawn: Node3D) -> void:
	if not round_ready or maze == null or pawn == null:
		return
	var outward := _compute_outward()
	var half_height := 1.0
	var shape := pawn.get_node_or_null("CollisionShape3D") as CollisionShape3D
	if shape and shape.shape is CapsuleShape3D:
		half_height = (shape.shape as CapsuleShape3D).height * 0.5
	var location := maze.project_to_surface(maze.get_tile_location(start_tile) + outward * maze.get_tile_spacing() * 0.42, half_height + 0.2)
	_place_player_at(pawn, location, maze.get_surface_rotation(location, -outward))


## Direction along the surface from the start tile toward its first neighbor
func _compute_outward() -> Vector3:
	var neighbors := maze.get_neighbors(start_tile)
	var tile_up := maze.get_tile_up(start_tile)
	var outward := Vector3.FORWARD if neighbors.is_empty() else maze.get_tile_up(neighbors[0]) - tile_up
	return PlanetMath.flatten_direction(outward, tile_up)


## UE: Teleport To
func _place_player_at(pawn: Node3D, location: Vector3, rotation_basis: Basis) -> void:
	pawn.global_transform = Transform3D(rotation_basis, location)
	if pawn is CharacterBody3D:
		(pawn as CharacterBody3D).velocity = Vector3.ZERO
		(pawn as CharacterBody3D).up_direction = rotation_basis.y


## Spawns count orbs around location (the player was hurt and dropped them). They land
## on the current tile and the tiles open to it, so every orb stays reachable.
func scatter_orbs(location: Vector3, count: int) -> void:
	if maze == null or count <= 0:
		return
	var tile := maze.find_nearest_tile(location)
	var scatter_tiles := maze.get_open_neighbors(tile)
	scatter_tiles.append(tile)
	for index: int in count:
		_scatter_one_orb(scatter_tiles)


func _scatter_one_orb(scatter_tiles: PackedInt32Array) -> void:
	var drop_tile := scatter_tiles[randi_range(0, scatter_tiles.size() - 1)]
	var up := maze.get_tile_up(drop_tile)
	var axes := PlanetMath.tangent_basis(up)
	var angle := randf_range(0.0, TAU)
	var distance := randf_range(0.0, 0.28) * maze.get_tile_spacing()
	var offset := (axes[0] * cos(angle) + axes[1] * sin(angle)) * distance
	var orb_location := maze.project_to_surface(maze.get_tile_location(drop_tile) + offset, orb_height)
	var orb := ORB_SCENE.instantiate() as EnergyOrb
	orb.initial_pickup_delay = 1.5
	_add_at(orb, Transform3D(maze.get_surface_rotation(orb_location, axes[0]), orb_location))


func _spawn_aliens() -> void:
	for index: int in num_aliens:
		_spawn_alien()


func _spawn_alien() -> void:
	if round_over or maze == null:
		return
	var alien := _alien_scene.instantiate() as Alien
	_add_on_tile(alien, _pick_distant_tile(_get_player_tile(), 0.5), _alien_half_height + 0.1)
	alien.killed.connect(_on_alien_killed)


func _spawn_ray_gun() -> void:
	if round_over or maze == null or is_instance_valid(active_ray_gun):
		return
	var tile := _pick_distant_tile(_get_player_tile(), 0.35)
	var axes := PlanetMath.tangent_basis(maze.get_tile_up(tile))
	if _place_ray_gun(maze.get_tile_location(tile, 0.9), axes[0]):
		show_banner("A ray gun has appeared - follow the pink beacon!", 4.0)


## To the side of the rocket, within sight of the player (who stands toward the first
## neighbor, facing the rocket).
func _spawn_ray_gun_near_rocket() -> void:
	if maze == null:
		return
	var tile_up := maze.get_tile_up(start_tile)
	var outward := _compute_outward()
	# UE: Cross(TileUp, Outward), its right-handed equivalent here
	var side := outward.cross(tile_up).normalized()
	var location := maze.project_to_surface(maze.get_tile_location(start_tile) + (side * 0.75 + outward * 0.25) * maze.get_tile_spacing() * 0.42, 0.9)
	_place_ray_gun(location, side)


func _place_ray_gun(location: Vector3, forward: Vector3) -> RayGunPickup:
	var ray_gun := RAY_GUN_SCENE.instantiate() as RayGunPickup
	_add_at(ray_gun, Transform3D(maze.get_surface_rotation(location, forward), location))
	ray_gun.collected.connect(_on_ray_gun_collected)
	active_ray_gun = ray_gun
	return ray_gun


func _on_alien_killed(_alien: Alien) -> void:
	get_tree().create_timer(maxf(alien_respawn_delay, 0.01), false).timeout.connect(_spawn_alien)


## The next gun shows up a while after this one runs out
func _on_ray_gun_collected() -> void:
	active_ray_gun = null
	get_tree().create_timer(_ray_gun_duration + maxf(ray_gun_respawn_delay, 0.01), false).timeout.connect(_spawn_ray_gun)


func _on_rocket_launched() -> void:
	_end_round(true)


func handle_player_death(_player: Node) -> void:
	_end_round(false)


func _end_round(escaped: bool) -> void:
	if round_over:
		return
	round_over = true
	show_banner("Rocket refueled - you escaped the planet!" if escaped else "The aliens got you!", restart_delay + 1.0)
	get_tree().create_timer(maxf(restart_delay, 0.01), false).timeout.connect(_restart_round)


## Reloads the current level
func _restart_round() -> void:
	get_tree().reload_current_scene()


# Helpers
# ------------------------------------------------

func show_banner(text: String, duration: float) -> void:
	banner = text
	banner_end_time = get_game_time() + duration


## Banner text for the HUD; empty when there's nothing to announce
func get_banner_text() -> String:
	return banner if get_game_time() < banner_end_time else ""


func _get_player_tile() -> int:
	var player := get_player()
	return maze.find_nearest_tile(player.global_position) if player and maze else start_tile


## Random tile at least min_fraction of the maze's furthest distance away from from_tile
func _pick_distant_tile(from_tile: int, min_fraction: float) -> int:
	var distances := maze.compute_distances(from_tile)
	var max_distance := 0
	for distance: int in distances:
		max_distance = maxi(max_distance, distance)

	var candidates: Array[int] = []
	for tile: int in distances.size():
		if tile != start_tile and distances[tile] >= max_distance * min_fraction:
			candidates.append(tile)

	if candidates.is_empty():
		return randi_range(0, maze.get_num_tiles() - 1)
	return candidates[randi_range(0, candidates.size() - 1)]


func _update_player_distances() -> void:
	if maze == null or maze.get_num_tiles() == 0:
		return
	var player := get_player()
	if player == null:
		player_distances = PackedInt32Array()
		return
	player_distances = maze.compute_distances(maze.find_nearest_tile(player.global_position))


## Transform on a tile, height above the surface, with a random heading so props and
## aliens don't all line up.
func _tile_transform(tile: int, height: float) -> Transform3D:
	var location := maze.get_tile_location(tile, height)
	var axes := PlanetMath.tangent_basis(maze.get_tile_up(tile))
	var angle := randf_range(0.0, TAU)
	return Transform3D(maze.get_surface_rotation(location, axes[0] * cos(angle) + axes[1] * sin(angle)), location)


func _add_on_tile(node: Node3D, tile: int, height: float) -> void:
	_add_at(node, _tile_transform(tile, height))


## UE: Spawn Actor (Always Spawn)
func _add_at(node: Node3D, xform: Transform3D) -> void:
	node.transform = xform
	get_tree().current_scene.add_child(node)
