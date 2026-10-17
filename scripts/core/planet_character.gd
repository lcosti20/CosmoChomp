class_name PlanetCharacter
extends CharacterBody3D
## Shared movement for characters that walk on a planet (the parts of UE's Character +
## CharacterMovementComponent the Blueprints rely on).
##
## Gravity points at the planet center (set every frame by the PlanetaryGravity child),
## so "horizontal" means the plane tangent to the surface under the character. Walking
## uses UE's CalcVelocity rules: input accelerates toward MaxWalkSpeed with ground
## friction turning the velocity, and no input brakes with BrakingDecelerationWalking.
## Units are meters (UE centimeters / 100).

## UE: Max Walk Speed (500 cm/s)
@export var max_walk_speed: float = 5.0
## UE: Min Analog Walk Speed (20 cm/s)
@export var min_analog_walk_speed: float = 0.2
## UE: Max Acceleration (2048 cm/s^2)
@export var max_acceleration: float = 20.48
## UE: Braking Deceleration Walking (2000 cm/s^2)
@export var braking_deceleration_walking: float = 20.0
## UE: Braking Deceleration Falling (1500 cm/s^2)
@export var braking_deceleration_falling: float = 15.0
## UE: Ground Friction
@export var ground_friction: float = 8.0
## UE: Braking Friction Factor (applied to ground friction while braking)
@export var braking_friction_factor: float = 2.0
## UE: Air Control
@export var air_control: float = 0.35
## UE: Jump Z Velocity (500 cm/s)
@export var jump_velocity: float = 5.0
## UE: world gravity (980 cm/s^2)
@export var gravity: float = 9.8

@onready var planetary_gravity: PlanetaryGravity = $PlanetaryGravity

var movement_enabled: bool = true
## Movement input gathered this frame (UE: AddMovementInput / ConsumeInputVector)
var _pending_input: Vector3 = Vector3.ZERO
var _jump_requested: bool = false


func _ready() -> void:
	floor_max_angle = deg_to_rad(44.765) # UE: Walkable Floor Angle
	floor_snap_length = 0.45 # UE: MaxStepHeight-ish snap so curvature never lifts us off
	floor_stop_on_slope = true
	motion_mode = CharacterBody3D.MOTION_MODE_GROUNDED


## UE: Pawn > Add Movement Input
func add_movement_input(world_direction: Vector3, scale: float = 1.0) -> void:
	_pending_input += world_direction * scale


## UE: Character > Jump
func jump() -> void:
	_jump_requested = true


## UE: Character > Stop Jumping (jumps have no hold time, so nothing to cancel)
func stop_jumping() -> void:
	_jump_requested = false


## UE: Character Movement > Stop Movement Immediately
func stop_movement_immediately() -> void:
	velocity = Vector3.ZERO
	_pending_input = Vector3.ZERO


## UE: Character Movement > Disable Movement
func disable_movement() -> void:
	movement_enabled = false
	_pending_input = Vector3.ZERO


## UE: Character Movement > Is Moving On Ground
func is_moving_on_ground() -> bool:
	return movement_enabled and is_on_floor()


## Applies gathered input, gravity and collisions for one frame (UE: CharacterMovement tick).
func update_movement(delta: float) -> void:
	var input := _pending_input
	_pending_input = Vector3.ZERO
	if not movement_enabled:
		return

	var up := up_direction
	var vertical_speed := velocity.dot(up)
	# Gravity direction changes as we walk around; keep the speed, re-flattened onto the new tangent plane
	var planar := PlanetMath.project_on_plane(velocity, up)
	var planar_speed := planar.length()
	planar = planar.normalized() * planar_speed if planar_speed > 0.0 else Vector3.ZERO

	input = PlanetMath.project_on_plane(input, up)
	var analog := minf(input.length(), 1.0)
	var acceleration := input.normalized() * analog * max_acceleration if analog > 0.0 else Vector3.ZERO

	var walking := is_on_floor()
	var jumped := false
	if walking:
		planar = _calc_velocity(planar, acceleration, analog, delta, ground_friction, braking_deceleration_walking)
		vertical_speed = 0.0
		if _jump_requested:
			vertical_speed = jump_velocity
			_jump_requested = false
			jumped = true
	else:
		planar = _calc_velocity(planar, acceleration * air_control, analog, delta, 0.0, braking_deceleration_falling)
		vertical_speed -= gravity * delta
		_jump_requested = false

	velocity = planar + up * vertical_speed
	move_and_slide()
	# Walking straight ahead leaves the curved ground by a hair each frame; stay on it
	# (UE: CharacterMovement keeps walking characters on the floor)
	if walking and not jumped and not is_on_floor():
		apply_floor_snap()


## UE: UCharacterMovementComponent::CalcVelocity for the tangent-plane part of the velocity.
func _calc_velocity(v: Vector3, acceleration: Vector3, analog: float, delta: float, friction: float, braking_deceleration: float) -> Vector3:
	var max_speed := maxf(max_walk_speed * analog, min_analog_walk_speed) if analog > 0.0 else max_walk_speed

	if acceleration.is_zero_approx():
		return _apply_braking(v, delta, friction * braking_friction_factor, braking_deceleration)

	var accel_dir := acceleration.normalized()
	var speed := v.length()
	if speed > max_speed:
		# Over the limit (e.g. after a speed change): only friction may slow us down
		v = _apply_braking(v, delta, friction, 0.0)
	else:
		# Friction turns the velocity toward the input direction
		v = v - (v - accel_dir * speed) * minf(delta * friction, 1.0)
	v += acceleration * delta
	# Input can't push past the speed limit, or past the current speed if already over it
	return v.limit_length(maxf(max_speed, speed) if speed > max_speed else max_speed)


## UE: ApplyVelocityBraking (substepped like UE so it's framerate independent).
func _apply_braking(v: Vector3, delta: float, friction: float, braking_deceleration: float) -> Vector3:
	if v.is_zero_approx() or (friction <= 0.0 and braking_deceleration <= 0.0):
		return v
	const MAX_STEP := 1.0 / 33.0
	var old := v
	var remaining := delta
	while remaining >= 1e-6:
		var dt := minf(remaining, MAX_STEP)
		remaining -= dt
		var reverse := -v.normalized() * braking_deceleration
		v += (-friction * v + reverse) * dt
		# Don't reverse direction
		if v.dot(old) <= 0.0:
			return Vector3.ZERO
	# Stop completely when nearly stopped (UE: BRAKE_TO_STOP_VELOCITY 10 cm/s)
	if v.length_squared() < 0.01:
		return Vector3.ZERO
	return v


## UE: Get Actor Forward Vector (Godot's forward is -Z).
func get_forward_vector() -> Vector3:
	return -global_basis.z.normalized()
