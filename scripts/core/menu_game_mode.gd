class_name MenuGameMode
extends Node
## Game mode for the start menu level (port of BP_MenuGameMode): no pawn, the level is
## seen through its MenuCamera, and the background music starts here.

@export var menu_camera_path: NodePath


func _ready() -> void:
	var camera := get_node_or_null(menu_camera_path) as Camera3D
	if camera:
		camera.make_current()
	var game_instance := get_node_or_null("/root/GameInstance")
	if game_instance:
		game_instance.start_music()
