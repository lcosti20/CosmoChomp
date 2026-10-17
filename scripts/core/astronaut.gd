class_name Astronaut
extends PlanetCharacter
## The player: a stranded astronaut walking around the planet (port of BP_Astronaut,
## which implements BPI_CosmoPlayer).
##
## Look input isn't applied to the camera directly. It accumulates in look_yaw (unlimited)
## and look_pitch (clamped), and every frame the control rotation is rebuilt from the
## gravity component's drift-free surface frame: turned by look_yaw around up, then
## tilted by look_pitch around the turned right axis. The camera boom follows the control
## rotation, so the camera orbits the astronaut relative to the planet's surface.
## The orb, health and ray gun queries other scripts call are the BPI_CosmoPlayer methods.

@export_group("Look")
@export var look_yaw_rate: float = 1.0
@export var look_pitch_rate: float = 1.0
@export var max_look_pitch: float = 85.0
@export var body_rotation_interp_speed: float = 10.0
## Degrees per mouse count (UE: Mouse2D axis sensitivity 0.07 in DefaultInput.ini; IA_MouseLook
## feeds the scaled delta straight into DoLook)
@export var mouse_look_scale: float = 0.07
## Degrees per physics tick at full stick deflection (UE: IA_Look adds the stick value once per frame)
@export var stick_look_scale: float = 1.0
## UE: IMC_MouseLook negates mouse Y but IMC_Default doesn't negate the right stick, so in the
## original pushing the stick up looks down. Kept for parity; turn off for a non-inverted stick.
@export var invert_stick_pitch: bool = true

@export_group("Rules")
@export var max_health: int = 3
## Seconds of invulnerability after being hurt
@export var invulnerability_duration: float = 2.0

@export_group("Ray Gun")
## Seconds between ray gun shots while fire is held
@export var fire_interval: float = 0.25
## Shots follow the planet's curvature for this distance (UE: 2500 cm)
@export var fire_range: float = 25.0
@export var fire_damage: float = 1.0
@export var beam_color: Color = Color(1.0, 0.1, 0.6)
## Size of the held gun (UE: 55 cm)
@export var ray_gun_held_size: float = 0.55

@export_group("Presentation")
## Walk animation play rate at full speed; slower movement plays it proportionally slower
@export var walk_play_rate: float = 1.6
## Ground speed above which the walk animation plays instead of idle (UE: 40 cm/s)
@export var walk_speed_threshold: float = 0.4
## Ground speed above which footsteps play (UE: 150 cm/s)
@export var footstep_speed_threshold: float = 1.5
@export var astronaut_height: float = 1.75
## The model faces +Z; turn it to face the body's forward (-Z)
@export var astronaut_yaw: float = 180.0
@export var idle_animation: StringName = &"Idle"
@export var walk_animation: StringName = &"Walk"

@export_group("Sounds")
@export var hurt_sound: AudioStream = preload("res://assets/sounds/ough/freesound_community-ough-47202.mp3")
@export var drop_orbs_sound: AudioStream = preload("res://assets/sounds/bang/dragon-studio-distant-bang-472364.mp3")
@export var shoot_sound: AudioStream = preload("res://assets/sounds/gun-shoot/floraphonic-scifi-gun-shoot-1-266417.mp3")

const RAY_GUN_BEAM_SCENE := preload("res://scenes/gameplay/ray_gun_beam.tscn")
const RAY_GUN_MATERIAL := preload("res://assets/materials/ray_gun.tres")

@onready var mesh: Node3D = $Mesh
@onready var camera_boom: SpringArm3D = $CameraBoom
@onready var follow_camera: Camera3D = $CameraBoom/FollowCamera
@onready var ray_gun_holder: Node3D = $RayGunHolder
@onready var ray_gun_mesh: Node3D = $RayGunHolder/RayGunMesh
@onready var footstep_audio: AudioStreamPlayer = $FootstepAudio
@onready var collision_shape: CollisionShape3D = $CollisionShape3D

var look_yaw: float = 0.0
var look_pitch: float = 0.0
## UE: the controller's control rotation (Godot basis, -Z forward)
var control_basis: Basis = Basis.IDENTITY
var has_controller: bool = true

var health: int = 3
var carried_orbs: int = 0
var dead: bool = false
var in_rocket: bool = false
var invulnerability_remaining: float = 0.0
var ray_gun_time_remaining: float = 0.0
var ray_gun_duration: float = 0.0
var fire_cooldown_remaining: float = 0.0
var ground_speed: float = 0.0
var current_surface_up: Vector3 = Vector3.UP

var mesh_base_transform: Transform3D = Transform3D.IDENTITY
var current_animation: StringName = &""
var _animation_player: AnimationPlayer = null

# Shot state (UE: ShotAim, ShotCenter, ShotMuzzle, ArcRadius, ArcUp, ArcForward, BeamPoints, HitActor)
var _shot_aim: Vector3 = Vector3.FORWARD
var _shot_center: Vector3 = Vector3.ZERO
var _shot_muzzle: Vector3 = Vector3.ZERO
var _arc_radius: float = 1.0
var _arc_up: Vector3 = Vector3.UP
var _arc_forward: Vector3 = Vector3.FORWARD
var _beam_points: PackedVector3Array = PackedVector3Array()
var _hit_actor: Node = null
var _shot_done: bool = false


func _ready() -> void:
	super._ready()
	add_to_group("player")
	health = max_health
	PlanetMath.fit_model_to_capsule(mesh, _capsule_half_height(), astronaut_height, astronaut_yaw)
	PlanetMath.apply_material(ray_gun_mesh, RAY_GUN_MATERIAL)
	PlanetMath.fit_static_mesh(ray_gun_mesh, ray_gun_held_size, false)
	ray_gun_mesh.visible = false
	_animation_player = mesh.find_child("AnimationPlayer", true, false) as AnimationPlayer
	_set_animation_looping(idle_animation)
	_set_animation_looping(walk_animation)
	_play_astro_anim(idle_animation)
	mesh_base_transform = mesh.transform
	camera_boom.add_excluded_object(get_rid())
	follow_camera.make_current()
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _capsule_half_height() -> float:
	var capsule := collision_shape.shape as CapsuleShape3D
	return capsule.height * 0.5 if capsule else 0.9


func _physics_process(delta: float) -> void:
	# The gravity component ticks first (UE: Add Tick Prerequisite Component)
	planetary_gravity.update_frame()
	_read_input()
	update_movement(delta)

	_update_gameplay(delta)
	if has_controller:
		_update_control_rotation()
		_update_body_facing(delta)


# Input (UE: IA_Move / IA_Look / IA_MouseLook / IA_Jump / IA_Fire)
# ------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		var motion := (event as InputEventMouseMotion).relative * mouse_look_scale
		# Mouse up looks up (UE: IMC_MouseLook negates Y)
		do_look(motion.x, -motion.y)
	elif event.is_action_pressed("jump"):
		do_jump_start()
	elif event.is_action_released("jump"):
		do_jump_end()


func _read_input() -> void:
	var move := Input.get_vector("move_left", "move_right", "move_back", "move_forward")
	if move != Vector2.ZERO:
		do_move(move.x, move.y)
	var look := Input.get_vector("look_left", "look_right", "look_down", "look_up", 0.2)
	if look != Vector2.ZERO:
		var pitch := -look.y if invert_stick_pitch else look.y
		do_look(look.x * stick_look_scale, pitch * stick_look_scale)
	if Input.is_action_pressed("fire"):
		do_fire()


## UE: DoMove
func do_move(right: float, forward: float) -> void:
	if has_controller and in_play():
		_move_apply(right, forward)


## UE: MoveApply. Forward is the camera direction flattened onto the surface.
func _move_apply(right: float, forward: float) -> void:
	var up := planetary_gravity.get_current_surface_up()
	var control_forward := PlanetMath.project_on_plane(-control_basis.z, up)
	var forward_dir: Vector3
	if control_forward.length_squared() < 0.0001:
		forward_dir = PlanetMath.project_on_plane(get_forward_vector(), up).normalized()
	else:
		forward_dir = control_forward.normalized()
	current_surface_up = up
	add_movement_input(forward_dir, forward)
	# UE: Cross(Up, Forward) is right in its left-handed frame; Godot's right is Forward x Up
	add_movement_input(forward_dir.cross(up).normalized(), right)


## UE: DoLook
func do_look(yaw: float, pitch: float) -> void:
	look_yaw += yaw * look_yaw_rate
	look_pitch = clampf(look_pitch + pitch * look_pitch_rate, -max_look_pitch, max_look_pitch)


func do_jump_start() -> void:
	if in_play():
		jump()


func do_jump_end() -> void:
	stop_jumping()


## UE: UpdateControlRotation. Surface frame, turned by look_yaw around up, then tilted
## by look_pitch around the turned frame's right axis.
func _update_control_rotation() -> void:
	var frame_up := planetary_gravity.frame_up
	var frame_forward := planetary_gravity.frame_forward
	# Positive yaw turns right (clockwise seen from above)
	var yawed_forward := frame_forward.rotated(frame_up, -deg_to_rad(look_yaw))
	var right_axis := yawed_forward.cross(frame_up).normalized()
	var pitched_forward := yawed_forward.rotated(right_axis, deg_to_rad(look_pitch))
	var pitched_up := frame_up.rotated(right_axis, deg_to_rad(look_pitch))
	control_basis = PlanetMath.basis_from_forward_up(pitched_forward, pitched_up)
	# The boom uses the control rotation (UE: Use Pawn Control Rotation)
	camera_boom.global_basis = control_basis


## UE: UpdateBodyFacing. Face the direction of travel; otherwise keep facing, re-flattened.
func _update_body_facing(delta: float) -> void:
	var up := planetary_gravity.get_current_surface_up()
	current_surface_up = up
	var desired := _compute_desired(up)
	var target := Quaternion(PlanetMath.basis_from_forward_up(desired, up))
	var current := global_basis.get_rotation_quaternion()
	# Non-sweeping: trust the surface-normal math, avoid collision-deflection drift
	global_basis = Basis(current.slerp(target, clampf(delta * body_rotation_interp_speed, 0.0, 1.0)).normalized())


## UE: ComputeDesired
func _compute_desired(up: Vector3) -> Vector3:
	var planar_velocity := PlanetMath.project_on_plane(velocity, up)
	var planar_forward := PlanetMath.project_on_plane(get_forward_vector(), up)
	# UE: squared speed above 100 cm^2/s^2 (10 cm/s)
	if planar_velocity.length_squared() > 0.01:
		return planar_velocity.normalized()
	elif planar_forward.length() < PlanetMath.NEARLY_ZERO:
		return PlanetMath.flatten_direction(Vector3.FORWARD, up)
	return planar_forward.normalized()


# Gameplay
# ------------------------------------------------

## UE: UpdateGameplay
func _update_gameplay(delta: float) -> void:
	fire_cooldown_remaining -= delta
	invulnerability_remaining = maxf(invulnerability_remaining - delta, 0.0)
	_tick_ray_gun(delta)
	_update_mesh_effects()
	_update_footsteps()


func _tick_ray_gun(delta: float) -> void:
	if ray_gun_time_remaining > 0.0:
		ray_gun_time_remaining -= delta
		_clamp_ray_gun()


func _clamp_ray_gun() -> void:
	if ray_gun_time_remaining <= 0.0:
		ray_gun_time_remaining = 0.0
		ray_gun_mesh.visible = false


## UE: UpdateMeshEffects. Blink while invulnerable, then pick the animation.
func _update_mesh_effects() -> void:
	if not in_play():
		return
	var blink_hidden := invulnerability_remaining > 0.0 and fmod(invulnerability_remaining, 0.2) < 0.1
	mesh.visible = not blink_hidden
	_update_anim()


## UE: UpdateAnim. Walk while moving along the ground, idle otherwise; the walk speeds
## up with movement speed.
func _update_anim() -> void:
	ground_speed = PlanetMath.project_on_plane(velocity, current_surface_up).length()
	if is_moving_on_ground() and ground_speed > walk_speed_threshold:
		_play_astro_anim(walk_animation)
		_set_play_rate(walk_play_rate * clampf(ground_speed / max_walk_speed, 0.4, 1.0))
	else:
		_play_astro_anim(idle_animation)
		_set_play_rate(1.0)


func _update_footsteps() -> void:
	var playing := footstep_audio.playing
	var walking := in_play() and is_moving_on_ground() and ground_speed > footstep_speed_threshold
	if walking:
		if not playing:
			footstep_audio.play()
	elif playing:
		footstep_audio.stop()


## UE: PlayAstroAnim
func _play_astro_anim(animation: StringName) -> void:
	if _animation_player == null or animation == &"" or animation == current_animation:
		return
	var resolved := _resolve_animation(animation)
	if resolved == &"":
		return
	current_animation = animation
	_animation_player.play(resolved)


func _set_play_rate(rate: float) -> void:
	if _animation_player:
		_animation_player.speed_scale = rate


func _resolve_animation(animation: StringName) -> StringName:
	if _animation_player.has_animation(animation):
		return animation
	for anim_name: StringName in _animation_player.get_animation_list():
		if String(anim_name).ends_with(String(animation)):
			return anim_name
	return &""


func _set_animation_looping(animation: StringName) -> void:
	if _animation_player == null:
		return
	var resolved := _resolve_animation(animation)
	if resolved != &"":
		_animation_player.get_animation(resolved).loop_mode = Animation.LOOP_LINEAR


## UE: Event AnyDamage
func take_damage(damage: float, _instigator: Node = null) -> void:
	if in_play() and invulnerability_remaining <= 0.0 and damage > 0.0:
		_take_hit(damage)


## UE: TakeHit
func _take_hit(damage: float) -> void:
	health = maxi(health - maxi(roundi(damage), 1), 0)
	invulnerability_remaining = invulnerability_duration
	SoundUtil.play_at(self, hurt_sound, global_position)
	_drop_orbs()
	_check_death()


## UE: DropOrbs. Getting hit knocks every carried orb loose.
func _drop_orbs() -> void:
	if carried_orbs <= 0:
		return
	var game_mode := CosmoGameMode.find(self)
	if game_mode:
		game_mode.scatter_orbs(global_position, carried_orbs)
	carried_orbs = 0
	SoundUtil.play_at(self, drop_orbs_sound, global_position)


func _check_death() -> void:
	if health <= 0:
		_die()


## UE: Die. Topple over and tell the game mode.
func _die() -> void:
	dead = true
	ray_gun_time_remaining = 0.0
	ray_gun_mesh.visible = false
	stop_movement_immediately()
	disable_movement()
	mesh.visible = true
	# Topple: UE adds Roll 90 to the mesh's base rotation, which tips the model (it faces the
	# mesh's +Y there) onto its face, pivoting at the feet. In Godot that is -90 degrees
	# about the body's right axis. (The Blueprint's Make Rotator pins are crossed, which
	# only turns the mesh sideways; this follows the documented behavior instead.)
	mesh.transform = Transform3D(Basis(Vector3.RIGHT, deg_to_rad(-90.0)) * mesh_base_transform.basis, mesh_base_transform.origin)
	var game_mode := CosmoGameMode.find(self)
	if game_mode:
		game_mode.handle_player_death(self)


# Ray gun
# ------------------------------------------------

## UE: DoFire
func do_fire() -> void:
	if in_play() and ray_gun_time_remaining > 0.0 and fire_cooldown_remaining <= 0.0 and has_controller:
		_fire_shot()


## UE: FireShot. A straight trace would climb away from the curved ground, so the shot
## follows a great circle around the planet at muzzle height, swept one segment at a time.
func _fire_shot() -> void:
	var count := maxi(ceili(fire_range / 1.5), 1)
	fire_cooldown_remaining = fire_interval

	# Aim where the camera looks, flattened onto the surface, and turn to face it
	var up := planetary_gravity.get_current_surface_up()
	_compute_shot_aim(up)
	global_basis = PlanetMath.basis_from_forward_up(_shot_aim, up)

	_shot_center = planetary_gravity.get_planet_center()
	_shot_muzzle = ray_gun_holder.global_position + _shot_aim * (ray_gun_held_size * 0.5)
	var radial := _shot_muzzle - _shot_center
	_arc_radius = maxf(radial.length(), 1.0)
	_arc_up = radial / _arc_radius
	_arc_forward = PlanetMath.flatten_direction(_shot_aim, _arc_up)

	_beam_points = PackedVector3Array([_shot_muzzle])
	_hit_actor = null
	_shot_done = false
	for index: int in range(1, count + 1):
		_trace_segment(index, count)

	_shot_hit()
	SoundUtil.play_at(self, shoot_sound, _shot_muzzle)
	_spawn_beam()


## UE: ComputeShotAim
func _compute_shot_aim(up: Vector3) -> void:
	var flat := PlanetMath.project_on_plane(-control_basis.z, up)
	if flat.length() < PlanetMath.NEARLY_ZERO:
		_shot_aim = get_forward_vector()
	else:
		_shot_aim = flat.normalized()


## UE: TraceSegment. Sphere sweep (radius 20 cm) from the last beam point to point
## index on the arc; walls, the ground, the rocket and aliens stop it (RayGun channel).
func _trace_segment(index: int, count: int) -> void:
	if _shot_done:
		return
	var angle := fire_range * float(index) / float(count) / _arc_radius
	var point := _shot_center + (_arc_up * cos(angle) + _arc_forward * sin(angle)) * _arc_radius
	var start := _beam_points[_beam_points.size() - 1]

	var hit := _sphere_trace(start, point, 0.2)
	if hit.is_empty():
		_beam_points.append(point)
	else:
		_beam_points.append(hit["position"])
		_hit_actor = hit["collider"]
		_shot_done = true


## Sphere sweep like UE's Sphere Trace By Channel; returns the sphere center at impact and the collider.
func _sphere_trace(start: Vector3, end: Vector3, sweep_radius: float) -> Dictionary:
	var space := get_world_3d().direct_space_state
	var sphere := SphereShape3D.new()
	sphere.radius = sweep_radius
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = sphere
	query.transform = Transform3D(Basis.IDENTITY, start)
	query.motion = end - start
	query.collision_mask = Layers.RAY_GUN_MASK
	query.exclude = [get_rid()]
	var fractions := space.cast_motion(query)
	if fractions.is_empty() or fractions[1] >= 1.0:
		return {}
	var impact := start + (end - start) * fractions[1]
	query.transform = Transform3D(Basis.IDENTITY, impact)
	query.motion = Vector3.ZERO
	var info := space.get_rest_info(query)
	return {"position": start + (end - start) * fractions[0], "collider": instance_from_id(info["collider_id"]) if info.has("collider_id") else null}


## UE: ShotHit. Only aliens take damage.
func _shot_hit() -> void:
	var alien := _hit_actor as Alien
	if alien:
		alien.take_damage(fire_damage, self)


## UE: SpawnBeam
func _spawn_beam() -> void:
	var beam := RAY_GUN_BEAM_SCENE.instantiate() as RayGunBeam
	get_tree().current_scene.add_child(beam)
	beam.set_points(_beam_points, beam_color)


# BPI_CosmoPlayer
# ------------------------------------------------

## False once the player has died or boarded the rocket (UE: InPlay)
func in_play() -> bool:
	return not dead and not in_rocket


func player_is_in_play() -> bool:
	return in_play()


func player_is_dead() -> bool:
	return dead


func player_get_health() -> int:
	return health


func player_get_max_health() -> int:
	return max_health


func player_get_carried_orbs() -> int:
	return carried_orbs


func player_add_orbs(count: int) -> void:
	carried_orbs += maxi(count, 0)


## Hands over every carried orb (to the rocket)
func player_take_all_orbs() -> int:
	var taken := carried_orbs
	carried_orbs = 0
	return taken


func player_has_ray_gun() -> bool:
	return ray_gun_time_remaining > 0.0


func player_get_ray_gun_time_remaining() -> float:
	return ray_gun_time_remaining


## Full charge of the current ray gun, for the HUD's countdown bar
func player_get_ray_gun_duration() -> float:
	return ray_gun_duration


func player_equip_ray_gun(duration: float) -> void:
	ray_gun_time_remaining = maxf(ray_gun_time_remaining, duration)
	ray_gun_duration = ray_gun_time_remaining
	ray_gun_mesh.visible = true


## Hides the player inside the rocket for the launch
func player_board_rocket() -> void:
	in_rocket = true
	ray_gun_time_remaining = 0.0
	stop_movement_immediately()
	disable_movement()
	collision_shape.set_deferred("disabled", true)
	mesh.visible = false
	ray_gun_holder.visible = false
	footstep_audio.stop()
