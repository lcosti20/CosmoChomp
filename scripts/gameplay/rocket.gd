class_name Rocket
extends Node3D
## The player's crashed rocket (port of BP_Rocket).
##
## Standing near it deposits every carried orb as fuel. Once required_orbs have been
## delivered, the player boards and it blasts off along its local up axis.

signal fuel_changed(stored_orbs: int, required_orbs: int)
signal launched

## Set by the game mode from the number of orbs in the maze
@export var required_orbs: int = 50
## Rocket height (UE: 520 cm)
@export var rocket_height: float = 5.2
@export var launch_duration: float = 6.0
## UE: 900 cm/s^2
@export var launch_acceleration: float = 9.0
@export var deposit_sound: AudioStream = preload("res://assets/sounds/whoosh/universfield-cartoon-rocket-whoosh-229207.mp3")
@export var launch_sound: AudioStream = preload("res://assets/sounds/rocket-launch/49053354-rocket-launch-306441.mp3")

@onready var rocket_mesh: Node3D = $RocketMesh
@onready var rocket_body: StaticBody3D = $RocketBody
@onready var flame_mesh: MeshInstance3D = $FlameMesh
@onready var deposit_zone: Area3D = $DepositZone

var stored_orbs: int = 0
var has_launched: bool = false
var launch_time: float = 0.0
var launch_speed: float = 0.0


func _ready() -> void:
	add_to_group("rocket")
	PlanetMath.fit_static_mesh(rocket_mesh, rocket_height, true)
	_fit_body_collision()
	flame_mesh.material_override = GlowMaterial.make(Color(1.0, 0.45, 0.05), 20.0)
	flame_mesh.visible = false


## Blocking collision hull around the fitted rocket model (UE: BlockAll on the mesh)
func _fit_body_collision() -> void:
	for mesh_instance: MeshInstance3D in rocket_mesh.find_children("*", "MeshInstance3D", true, false):
		if mesh_instance.mesh == null:
			continue
		var shape := CollisionShape3D.new()
		shape.shape = mesh_instance.mesh.create_convex_shape(true, true)
		rocket_body.add_child(shape)
		shape.global_transform = mesh_instance.global_transform


func _physics_process(delta: float) -> void:
	if not has_launched:
		# Deposit continuously, so a player waiting by the rocket hands over orbs as soon as they have any
		for body: Node3D in deposit_zone.get_overlapping_bodies():
			_deposit_from(body)
		return

	# Blast off along the local up axis (away from the planet)
	launch_time += delta
	launch_speed += launch_acceleration * delta
	global_position += global_basis.y.normalized() * launch_speed * delta
	flame_mesh.scale = Vector3(1.2, 2.0 + 0.4 * sin(launch_time * 40.0), 1.2)

	if launch_time >= launch_duration:
		set_physics_process(false)
		visible = false


func get_stored_orbs() -> int:
	return stored_orbs


func _deposit_from(player: Node) -> void:
	if has_launched or not player.has_method("player_is_in_play"):
		return
	if not player.player_is_in_play() or player.player_get_carried_orbs() <= 0:
		return

	stored_orbs += player.player_take_all_orbs()
	fuel_changed.emit(stored_orbs, required_orbs)
	SoundUtil.play_at(self, deposit_sound, global_position)

	if stored_orbs >= required_orbs:
		_launch(player)


func _launch(player: Node) -> void:
	has_launched = true
	player.player_board_rocket()

	rocket_body.process_mode = Node.PROCESS_MODE_DISABLED
	flame_mesh.visible = true
	SoundUtil.play_at(self, launch_sound, global_position)
	launched.emit()
