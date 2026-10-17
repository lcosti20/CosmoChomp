class_name Alien
extends PlanetCharacter
## One-eyed alien that roams the maze (port of BP_Alien).
##
## Navigation runs on the maze's tile graph (navmesh doesn't work on a sphere): it
## wanders between random tiles, chases the player when they're a few steps away
## through the maze, and runs from a player holding the ray gun. Distances to the
## player come from the game mode's shared distance field (refreshed every 0.3 s),
## so aliens don't each run their own search.

signal killed(alien: Alien)

enum State { WANDER, CHASE, FLEE, ATTACK, DEAD }

@export_group("Behavior")
## Starts chasing when the player is this many steps away through the maze
@export var chase_tile_range: int = 4
## UE: 260 cm/s
@export var wander_speed: float = 2.6
## Slower than the player's 5 m/s so a careful player can outrun them (UE: 400 cm/s)
@export var chase_speed: float = 4.0
## UE: 300 cm/s
@export var flee_speed: float = 3.0
## Seconds between route updates
@export var repath_interval: float = 0.35

@export_group("Attack")
## UE: 170 cm
@export var attack_range: float = 1.7
@export var attack_damage: float = 1.0
## Delay between starting the swing and the hit landing
@export var attack_windup: float = 0.35
@export var attack_cooldown: float = 1.6

@export_group("Visuals")
## UE: 190 cm
@export var mesh_height: float = 1.9
## The model faces +Z; turn it to face the body's forward (-Z)
@export var mesh_yaw: float = 180.0
@export var idle_animation: StringName = &"Idle"
@export var walk_animation: StringName = &"Walk"
@export var attack_animation: StringName = &"Running attack"
@export var attack_sound: AudioStream = preload("res://assets/sounds/claw-attack/yodguard-scorpion-claw-attack-4-482507.mp3")
@export var death_sound: AudioStream = preload("res://assets/sounds/slime/freesound_community-slime-2-30099.mp3")

@onready var mesh: Node3D = $Mesh
@onready var collision_shape: CollisionShape3D = $CollisionShape3D

var state: State = State.WANDER
## Tile the alien is standing on, and the adjacent tile it is walking to
var current_tile: int = -1
var next_tile: int = -1
## Destination while wandering
var wander_goal: int = -1
var repath_timer: float = 0.0
var attack_cooldown_remaining: float = 0.0
var attack_phase: float = 0.0
var death_timer: float = 0.0
var attack_landed: bool = false

var maze: HexMaze = null
var player: Node3D = null
var player_distances: PackedInt32Array = PackedInt32Array()
var exits: PackedInt32Array = PackedInt32Array()
var current_animation: StringName = &""

var _best_distance: int = 0
var _best_dot: float = -2.0
var _best_tile: int = -1
var _facing_dir: Vector3 = Vector3.ZERO
var _animation_player: AnimationPlayer = null
var _mesh_base_transform: Transform3D = Transform3D.IDENTITY


func _ready() -> void:
	super._ready()
	add_to_group("aliens")
	max_walk_speed = wander_speed
	braking_deceleration_walking = 15.0
	var capsule := collision_shape.shape as CapsuleShape3D
	PlanetMath.fit_model_to_capsule(mesh, capsule.height * 0.5, mesh_height, mesh_yaw)
	_mesh_base_transform = mesh.transform
	_animation_player = mesh.find_child("AnimationPlayer", true, false) as AnimationPlayer
	_play_anim(idle_animation, true)
	# Stagger route updates between aliens
	repath_timer = randf_range(0.0, repath_interval)
	var mazes := get_tree().get_nodes_in_group("maze")
	if mazes.size() > 0:
		maze = mazes[0] as HexMaze


func _physics_process(delta: float) -> void:
	# The gravity component ticks first (UE: Add Tick Prerequisite Component)
	planetary_gravity.update_frame()
	if state == State.DEAD:
		_tick_dead(delta)
		return
	_tick_alive(delta)
	update_movement(delta)


func get_state() -> State:
	return state


func is_dead() -> bool:
	return state == State.DEAD


## UE: TickDead. Shrink away, then disappear.
func _tick_dead(delta: float) -> void:
	death_timer += delta
	var alpha := clampf(1.0 - death_timer / 0.6, 0.0, 1.0)
	# Scale the visual about the body origin (UE scales the whole actor)
	var shrink := maxf(alpha, 0.01)
	mesh.transform = Transform3D(_mesh_base_transform.basis.scaled(Vector3.ONE * shrink), _mesh_base_transform.origin * shrink)
	if alpha <= 0.0:
		queue_free()


func _tick_alive(delta: float) -> void:
	if maze:
		_tick_moving(delta)


func _tick_moving(delta: float) -> void:
	_refresh_player()
	_update_current_tile()
	attack_cooldown_remaining -= delta
	if state == State.ATTACK:
		_tick_attack(delta)
		_update_facing(delta)
	else:
		_tick_roam(delta)


func _tick_roam(delta: float) -> void:
	repath_timer -= delta
	_maybe_repath()
	_steer()
	_maybe_attack()
	_update_facing(delta)
	_update_walk_anim()


## Only a player who is still in play is worth chasing
func _refresh_player() -> void:
	player = null
	var pawn := get_tree().get_first_node_in_group("player") as Node3D
	if pawn and pawn.has_method("player_is_in_play") and pawn.player_is_in_play():
		player = pawn


## UE: UpdateCurrentTile. A full search once, then only the current tile and its neighbors.
func _update_current_tile() -> void:
	if current_tile == -1:
		current_tile = maze.find_nearest_tile(global_position)
	else:
		_update_tile_local()


func _update_tile_local() -> void:
	var up := (global_position - planetary_gravity.get_planet_center()).normalized()
	_best_dot = -2.0
	_best_tile = current_tile
	_consider_tile(current_tile, up)
	for neighbor: int in maze.get_neighbors(current_tile):
		_consider_tile(neighbor, up)
	current_tile = _best_tile


func _consider_tile(tile: int, up: Vector3) -> void:
	var dot := maze.get_tile_up(tile).dot(up)
	if dot > _best_dot:
		_best_dot = dot
		_best_tile = tile


func _maybe_repath() -> void:
	if repath_timer <= 0.0:
		_update_route()
		repath_timer = repath_interval


## UE: UpdateRoute. Reads the game mode's shared distance field to the player.
func _update_route() -> void:
	player_distances = PackedInt32Array()
	if is_instance_valid(player):
		var game_mode := CosmoGameMode.find(self)
		if game_mode:
			player_distances = game_mode.get_player_distances()
	_route_decide()


## Steps from tile to the player through the maze (-1 if unknown).
func _distance_at(tile: int) -> int:
	if tile >= 0 and tile < player_distances.size():
		return player_distances[tile]
	return -1


func _route_decide() -> void:
	var steps := _distance_at(current_tile)
	exits = maze.get_open_neighbors(current_tile)
	var armed: bool = is_instance_valid(player) and player.player_has_ray_gun()
	if armed and steps != -1:
		_route_flee()
	elif steps != -1 and steps <= chase_tile_range:
		_route_chase()
	else:
		_route_wander()


## Run for whichever exit leads furthest from the armed player
func _route_flee() -> void:
	state = State.FLEE
	next_tile = current_tile
	_best_distance = _distance_at(current_tile)
	for exit_tile: int in exits:
		_consider_flee_exit(exit_tile)
	wander_goal = -1


func _consider_flee_exit(exit_tile: int) -> void:
	var steps := _distance_at(exit_tile)
	if steps > _best_distance or (steps == _best_distance and randi() % 2 == 0):
		_best_distance = steps
		next_tile = exit_tile


## Step downhill on the distance field toward the player
func _route_chase() -> void:
	state = State.CHASE
	next_tile = current_tile
	var steps := _distance_at(current_tile)
	for exit_tile: int in exits:
		_consider_chase_exit(exit_tile, steps - 1)
	wander_goal = -1


func _consider_chase_exit(exit_tile: int, steps: int) -> void:
	if _distance_at(exit_tile) == steps and next_tile == current_tile:
		next_tile = exit_tile


func _route_wander() -> void:
	state = State.WANDER
	if wander_goal == -1 or wander_goal == current_tile:
		wander_goal = randi_range(0, maze.get_num_tiles() - 1)
	_follow_wander_path()


func _follow_wander_path() -> void:
	var path := maze.find_path(current_tile, wander_goal)
	if path.size() > 1:
		next_tile = path[1]
	else:
		next_tile = current_tile
		if path.is_empty():
			wander_goal = -1


func _steer() -> void:
	var speed := chase_speed if state == State.CHASE else (flee_speed if state == State.FLEE else wander_speed)
	var steps := _distance_at(current_tile)
	if state == State.CHASE and is_instance_valid(player) and steps == 0:
		# Same tile: go straight for the player
		_steer_towards(player.global_position, speed)
	elif next_tile != -1:
		_steer_along_corridor(speed)


## Follow the line between tile centers, which passes through the middle of each opening.
## Heading straight for the next center could clip the posts beside the opening.
func _steer_along_corridor(speed: float) -> void:
	var from := maze.get_tile_location(current_tile, 0.0)
	var to := maze.get_tile_location(next_tile, 0.0)
	var segment := to - from
	var segment_length_sq := segment.length_squared()
	if segment_length_sq > 0.0001:
		var position_on_surface := maze.project_to_surface(global_position, 0.0)
		var progress := (position_on_surface - from).dot(segment) / segment_length_sq
		var lookahead := 0.35 * maze.get_tile_spacing() / segment.length()
		_steer_towards(from + segment * clampf(progress + lookahead, 0.0, 1.0), speed)
	else:
		_steer_towards(to, speed)


func _steer_towards(target: Vector3, speed: float) -> void:
	var up := planetary_gravity.get_current_surface_up()
	var direction := PlanetMath.project_on_plane(target - global_position, up)
	# UE: at least 15 cm away
	if direction.length_squared() >= 0.0225:
		max_walk_speed = speed
		add_movement_input(direction.normalized())


func _maybe_attack() -> void:
	if not is_instance_valid(player) or state == State.FLEE:
		return
	if attack_cooldown_remaining <= 0.0 and player.global_position.distance_to(global_position) <= attack_range:
		_start_attack()


func _start_attack() -> void:
	state = State.ATTACK
	attack_phase = 0.0
	attack_landed = false
	_play_anim(attack_animation, false)
	SoundUtil.play_at(self, attack_sound, global_position)


func _tick_attack(delta: float) -> void:
	attack_phase += delta
	_attack_land()
	_attack_end()


func _attack_land() -> void:
	if attack_landed or attack_phase < attack_windup:
		return
	attack_landed = true
	# Some leeway, so stepping back at the last moment is needed to dodge
	if is_instance_valid(player) and player.global_position.distance_to(global_position) <= attack_range * 1.3:
		player.take_damage(attack_damage, self)


func _attack_end() -> void:
	if attack_phase >= attack_windup + 0.5:
		state = State.CHASE
		attack_cooldown_remaining = attack_cooldown
		repath_timer = 0.0


func _update_facing(delta: float) -> void:
	var up := planetary_gravity.get_current_surface_up()
	_facing_dir = PlanetMath.project_on_plane(velocity, up)
	_face_player_if_attacking(up)
	_fix_facing(up)
	planetary_gravity.orient_owner_to_surface(delta, _facing_dir.normalized(), 8.0)


func _face_player_if_attacking(up: Vector3) -> void:
	if state == State.ATTACK and is_instance_valid(player):
		_facing_dir = PlanetMath.project_on_plane(player.global_position - global_position, up)


## Standing still: keep the current facing (UE: under 10 cm/s)
func _fix_facing(up: Vector3) -> void:
	if _facing_dir.length_squared() < 0.01:
		_facing_dir = PlanetMath.project_on_plane(get_forward_vector(), up)


## UE: moving faster than 30 cm/s
func _update_walk_anim() -> void:
	if velocity.length_squared() > 0.09:
		_play_anim(walk_animation, true)
	else:
		_play_anim(idle_animation, true)


func _play_anim(animation: StringName, loop: bool) -> void:
	if _animation_player == null or animation == &"" or animation == current_animation:
		return
	var resolved := _resolve_animation(animation)
	if resolved == &"":
		return
	current_animation = animation
	_animation_player.get_animation(resolved).loop_mode = Animation.LOOP_LINEAR if loop else Animation.LOOP_NONE
	_animation_player.play(resolved)


func _resolve_animation(animation: StringName) -> StringName:
	if _animation_player.has_animation(animation):
		return animation
	for anim_name: StringName in _animation_player.get_animation_list():
		if String(anim_name).ends_with("|" + String(animation)):
			return anim_name
	return &""


## UE: Event AnyDamage. Any hit kills.
func take_damage(_damage: float, _instigator: Node = null) -> void:
	if state != State.DEAD:
		_die()


func _die() -> void:
	state = State.DEAD
	death_timer = 0.0
	collision_shape.set_deferred("disabled", true)
	stop_movement_immediately()
	disable_movement()
	SoundUtil.play_at(self, death_sound, global_position)
	killed.emit(self)
