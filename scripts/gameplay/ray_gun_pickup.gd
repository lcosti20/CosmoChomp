class_name RayGunPickup
extends Area3D
## A ray gun lying in the maze, marked by a tall pink beacon (port of BP_RayGunPickup).
##
## Picking it up lets the player shoot for duration seconds.

signal collected

@export var duration: float = 10.0
## Displayed gun size (UE: 110 cm)
@export var display_size: float = 1.1
## UE: 1600 cm
@export var beacon_height: float = 16.0
## UE: 14 cm
@export var beacon_width: float = 0.14
@export var beacon_color: Color = Color(1.0, 0.1, 0.6)
## Spin speed of the displayed gun, degrees per second (UE: RotatingMovement yaw 120)
@export var spin_rate: float = 120.0
@export var pickup_sound: AudioStream = preload("res://assets/sounds/whoosh/universfield-cartoon-rocket-whoosh-229207.mp3")

const GUN_MATERIAL := preload("res://assets/materials/ray_gun.tres")

@onready var pivot: Node3D = $Pivot
@onready var gun_mesh: Node3D = $Pivot/GunMesh
@onready var beacon: MeshInstance3D = $Beacon
@onready var glow: OmniLight3D = $Glow


func _ready() -> void:
	add_to_group("ray_gun_pickups")
	PlanetMath.apply_material(gun_mesh, GUN_MATERIAL)
	PlanetMath.fit_static_mesh(gun_mesh, display_size, false)

	# Engine cylinder: 1 m tall and across, pivot at its center
	beacon.position = Vector3(0.0, beacon_height * 0.5, 0.0)
	beacon.scale = Vector3(beacon_width, beacon_height, beacon_width)
	beacon.material_override = GlowMaterial.make(beacon_color, 6.0)
	glow.light_color = beacon_color

	body_entered.connect(_on_body_entered)


func _process(delta: float) -> void:
	# The spinner turns this pivot, leaving the gun's fitting offset intact
	pivot.rotate_y(-deg_to_rad(spin_rate) * delta)


func _on_body_entered(body: Node3D) -> void:
	if is_queued_for_deletion() or not body.has_method("player_is_in_play"):
		return
	if not body.player_is_in_play():
		return

	body.player_equip_ray_gun(duration)
	SoundUtil.play_at(self, pickup_sound, global_position)
	collected.emit()
	queue_free()
