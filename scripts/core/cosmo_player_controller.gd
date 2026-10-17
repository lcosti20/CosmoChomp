class_name CosmoPlayerController
extends Node
## Handles the pause key and the pause menu (port of BP_CosmoPlayerController).
##
## Runs while the game is paused so the same key (Escape, P, gamepad Start) closes the
## menu again (UE: IA_Pause has Trigger When Paused checked).
##
## Platform additions: Android's Back button toggles the menu, and the game pauses when a
## phone app goes to the background. In a browser, Escape never reaches the game: the
## browser uses it to release the mouse, so losing the mouse capture opens the menu.

const PAUSE_MENU_SCENE := preload("res://scenes/ui/pause_menu.tscn")

## Z-order of the menu's canvas layer (UE: Add to Viewport Z-order 100)
@export var pause_menu_layer: int = 100

var pause_menu: PauseMenu = null
var _pause_layer: CanvasLayer = null
## True once the browser has granted the mouse capture
var _had_mouse_capture: bool = false


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_process(OS.has_feature("web") and not TouchControls.is_touch_device())


## Web only: watch for the browser releasing the mouse. The capture is granted a few
## frames after it's requested, so only a capture that was held and then lost counts.
func _process(_delta: float) -> void:
	var captured := Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	if captured:
		_had_mouse_capture = true
	elif _had_mouse_capture:
		_had_mouse_capture = false
		if not get_tree().paused:
			open_pause_menu()


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_GO_BACK_REQUEST:
		toggle_pause_menu()
	elif what == NOTIFICATION_APPLICATION_PAUSED and OS.has_feature("mobile"):
		open_pause_menu()


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
	_had_mouse_capture = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func quit_game() -> void:
	get_tree().quit()
