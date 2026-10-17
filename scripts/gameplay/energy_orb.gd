class_name EnergyOrb
extends Area3D
## A glowing energy orb lying in the maze (port of BP_EnergyOrb).
##
## Walking into it adds value to the player's carried orbs. Orbs knocked loose when the
## player is hurt are briefly uncollectable (set_pickup_delay) so they scatter first.

@export var value: int = 1
## Orb diameter (UE: 36 cm)
@export var orb_size: float = 0.36
@export var glow_color: Color = Color(0.15, 1.0, 0.55)
## Seconds before the orb can be collected (UE: set right after spawning)
@export var initial_pickup_delay: float = 0.0
@export var collect_sound: AudioStream = null

@onready var orb_mesh: MeshInstance3D = $OrbMesh

var _pickup_timer: SceneTreeTimer = null


func _ready() -> void:
	add_to_group("energy_orbs")
	# Engine sphere is 1 m across (UE: 100 cm)
	orb_mesh.scale = Vector3.ONE * orb_size
	orb_mesh.material_override = GlowMaterial.make(glow_color)
	body_entered.connect(_on_body_entered)
	set_pickup_delay(initial_pickup_delay)


func _on_body_entered(body: Node3D) -> void:
	try_collect(body)


## Turns off pickup for [param seconds], then turns it back on.
func set_pickup_delay(seconds: float) -> void:
	set_deferred("monitoring", false)
	_pickup_timer = get_tree().create_timer(maxf(seconds, 0.01), false)
	_pickup_timer.timeout.connect(_enable_pickup.bind(_pickup_timer))


func _enable_pickup(timer: SceneTreeTimer) -> void:
	# A newer delay replaced this one
	if timer != _pickup_timer:
		return
	monitoring = true
	# Collects the orb right away if the player is already standing on it
	await get_tree().physics_frame
	if not is_inside_tree():
		return
	for body: Node3D in get_overlapping_bodies():
		try_collect(body)


func try_collect(other: Node) -> void:
	if is_queued_for_deletion() or not other.has_method("player_is_in_play"):
		return
	if not other.player_is_in_play():
		return
	other.player_add_orbs(value)
	if collect_sound:
		SoundUtil.play_at(self, collect_sound, global_position)
	queue_free()
