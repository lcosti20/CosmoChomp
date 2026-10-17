class_name MenuGameMode
extends Node
## Game mode for the start menu level (port of BP_MenuGameMode): no pawn, the level is
## seen through its MenuCamera, and the background music starts here.

@export var menu_camera_path: NodePath
## The astronaut standing by the rocket (UE: a SkeletalMeshActor playing SK_AstronautIdle)
@export var menu_astronaut_path: NodePath
@export var menu_astronaut_animation: StringName = &"Idle"


func _ready() -> void:
	var camera := get_node_or_null(menu_camera_path) as Camera3D
	if camera:
		camera.make_current()
	_play_astronaut_idle()
	var game_instance := get_node_or_null("/root/GameInstance")
	if game_instance:
		game_instance.start_music()


func _play_astronaut_idle() -> void:
	var astronaut := get_node_or_null(menu_astronaut_path)
	var animation_player := astronaut.find_child("AnimationPlayer", true, false) as AnimationPlayer if astronaut else null
	if animation_player == null or not animation_player.has_animation(menu_astronaut_animation):
		return
	animation_player.get_animation(menu_astronaut_animation).loop_mode = Animation.LOOP_LINEAR
	animation_player.play(menu_astronaut_animation)
