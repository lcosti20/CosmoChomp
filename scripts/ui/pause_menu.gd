class_name PauseMenu
extends Control
## "Paused" overlay with Continue and Quit (port of WBP_PauseMenu).
##
## Escape, P and gamepad Start reach the player controller's pause toggle; gamepad B
## (face button right) also closes the menu (UE: NativeOnKeyDown).

signal continue_pressed
signal quit_pressed

@onready var continue_button: Button = %ContinueButton
@onready var quit_button: Button = %QuitButton


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	continue_button.pressed.connect(continue_pressed.emit)
	quit_button.pressed.connect(quit_pressed.emit)
	# A browser tab can't close itself
	quit_button.visible = not OS.has_feature("web")


func focus_first_button() -> void:
	continue_button.grab_focus()


func _input(event: InputEvent) -> void:
	if not is_visible_in_tree():
		return
	var joypad := event as InputEventJoypadButton
	if joypad and joypad.pressed and joypad.button_index == JOY_BUTTON_B:
		continue_pressed.emit()
		get_viewport().set_input_as_handled()
