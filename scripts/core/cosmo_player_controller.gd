class_name CosmoPlayerController
extends Node
## Handles the pause key and the pause menu (port of BP_CosmoPlayerController).
##
## Runs while the game is paused so the same key (Escape, P, gamepad Start) closes the
## menu again (UE: IA_Pause has Trigger When Paused checked).

const PAUSE_MENU_SCENE := preload("res://scenes/ui/pause_menu.tscn")

## Z-order of the menu's canvas layer (UE: Add to Viewport Z-order 100)
@export var pause_menu_layer: int = 100

var pause_menu: PauseMenu = null
var _pause_layer: CanvasLayer = null


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("pause") and not event.is_echo():
		toggle_pause_menu()
		get_viewport().set_input_as_handled()


func toggle_pause_menu() -> void:
	if pause_menu and pause_menu.is_visible_in_tree():
		close_pause_menu()
	else:
		open_pause_menu()


func open_pause_menu() -> void:
	if pause_menu and pause_menu.is_visible_in_tree():
		return
	if pause_menu == null:
		_pause_layer = CanvasLayer.new()
		_pause_layer.layer = pause_menu_layer
		add_child(_pause_layer)
		pause_menu = PAUSE_MENU_SCENE.instantiate() as PauseMenu
		pause_menu.continue_pressed.connect(close_pause_menu)
		pause_menu.quit_pressed.connect(quit_game)
		_pause_layer.add_child(pause_menu)
	pause_menu.show()
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	pause_menu.focus_first_button()


func close_pause_menu() -> void:
	if pause_menu:
		pause_menu.hide()
	get_tree().paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func quit_game() -> void:
	get_tree().quit()
