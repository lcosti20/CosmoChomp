class_name PlanetaryGravity
extends Node
## Keeps its owner (the parent Node3D) stuck to the nearest planet and upright on its
## surface (port of BPC_PlanetaryGravity).
##
## Every physics frame it finds the nearest node in the "Planet" group, works out the
## surface "up" under the owner and carries a heading (FrameForward) along with it.
## A CharacterBody3D owner gets its up_direction set; it applies gravity itself along
## -up (UE: Character Movement > Set Gravity Direction).
##
## Add it as a child of the body, before the body's own script runs its physics step
## (UE: Add Tick Prerequisite Component); the owners call [method update_frame] first
## thing in _physics_process so the order is explicit.

signal surface_frame_updated(new_surface_up: Vector3, surface_rotation: Basis)

## Planets further away than this are ignored (UE: 10000 cm)
@export var gravity_search_radius: float = 100.0
@export var drive_movement_component_gravity: bool = true

var planet_center: Vector3 = Vector3.ZERO
var frame_forward: Vector3 = Vector3.FORWARD
var frame_up: Vector3 = Vector3.UP
var current_surface_up: Vector3 = Vector3.UP
var frame_initialized: bool = false
var new_up: Vector3 = Vector3.UP
var current_planet: Node3D = null

var _best_dist_sq: float = 0.0
var _best_planet: Node3D = null
var _cached_planets: Array[Node] = []


func _ready() -> void:
	# Planets are placed in the level and never spawned, so the lookup is cached
	_cached_planets = get_tree().get_nodes_in_group("Planet")


## Runs one frame of the gravity update (UE: Event Tick).
func update_frame() -> void:
	_find_nearest_planet()
	_compute_new_up()
	_update_frame()
	frame_up = new_up
	current_surface_up = new_up
	_update_gravity()
	surface_frame_updated.emit(new_up, PlanetMath.basis_from_forward_up(frame_forward, frame_up))


func get_owner_body() -> Node3D:
	return get_parent() as Node3D


func _find_nearest_planet() -> void:
	var body := get_owner_body()
	_best_dist_sq = gravity_search_radius * gravity_search_radius
	_best_planet = null
	for planet: Node in _cached_planets:
		_consider_planet(planet as Node3D, body.global_position)
	if is_instance_valid(_best_planet):
		current_planet = _best_planet
		planet_center = _best_planet.global_position


func _consider_planet(planet: Node3D, owner_location: Vector3) -> void:
	if not is_instance_valid(planet):
		return
	var dist_sq := owner_location.distance_squared_to(planet.global_position)
	if dist_sq <= _best_dist_sq:
		_best_dist_sq = dist_sq
		_best_planet = planet


func _compute_new_up() -> void:
	var offset := get_owner_body().global_position - planet_center
	if offset.length() < PlanetMath.NEARLY_ZERO:
		new_up = frame_up if frame_initialized else Vector3.UP
	else:
		new_up = offset.normalized()


func _update_frame() -> void:
	if frame_initialized:
		_advance_frame()
	else:
		_init_frame()


func _init_frame() -> void:
	frame_initialized = true
	var body_forward := -get_owner_body().global_basis.z
	var result := PlanetMath.flatten_direction(body_forward, new_up)
	if result.length() < PlanetMath.NEARLY_ZERO:
		frame_forward = PlanetMath.flatten_direction(Vector3.FORWARD, new_up)
	else:
		frame_forward = result


func _advance_frame() -> void:
	frame_forward = PlanetMath.flatten_direction(frame_forward, new_up)


func _update_gravity() -> void:
	if not drive_movement_component_gravity:
		return
	var body := get_owner_body() as CharacterBody3D
	if body:
		body.up_direction = new_up


## "Up" at the owner's current location; movement code calls this after the owner moved.
func get_current_surface_up() -> Vector3:
	var offset := get_owner_body().global_position - planet_center
	if offset.length() < PlanetMath.NEARLY_ZERO:
		return frame_up if frame_initialized else Vector3.UP
	return offset.normalized()


func get_surface_rotation() -> Basis:
	return PlanetMath.basis_from_forward_up(frame_forward, frame_up)


func get_planet_center() -> Vector3:
	return planet_center


func get_current_planet() -> Node3D:
	return current_planet


## Smoothly turns the owner so its up is the surface up and it faces [param desired_forward].
## Non-sweeping by design, so collisions can't knock the rotation off.
func orient_owner_to_surface(delta: float, desired_forward: Vector3, interp_speed: float) -> void:
	var body := get_owner_body()
	var target := Quaternion(PlanetMath.basis_from_forward_up(desired_forward, current_surface_up))
	var current := body.global_basis.get_rotation_quaternion()
	var rotation := current.slerp(target, clampf(delta * interp_speed, 0.0, 1.0))
	body.global_basis = Basis(rotation.normalized()).scaled(body.global_basis.get_scale())
