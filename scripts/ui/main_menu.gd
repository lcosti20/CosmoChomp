class_name MainMenu
extends Control
## Start menu with Start, Credits and Quit (port of WBP_MainMenu).
##
## Start loads the game level, Credits swaps the buttons for a scrolling credits panel
## with a Back button, Quit closes the game. If the Credits section of the README
## changes, update CreditsBody in scenes/ui/main_menu.tscn to match.

## UE: Open Level (Lvl_CosmoChomp)
@export_file("*.tscn") var game_level: String = "res://scenes/levels/cosmo_chomp.tscn"

@onready var main_panel: Control = %MainPanel
@onready var credits_panel: Control = %CreditsPanel
@onready var start_button: Button = %StartButton
@onready var credits_button: Button = %CreditsButton
@onready var quit_button: Button = %QuitButton
@onready var back_button: Button = %BackButton


func _ready() -> void:
	start_button.pressed.connect(start_game)
	credits_button.pressed.connect(show_credits)
	quit_button.pressed.connect(quit_to_desktop)
	back_button.pressed.connect(show_menu)
	# A browser tab can't close itself
	quit_button.visible = not OS.has_feature("web")
	# UE: Set Input Mode UI Only + Show Mouse Cursor
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	show_menu()


func _unhandled_input(event: InputEvent) -> void:
	# Back out of the credits with Escape or gamepad B
	if credits_panel.visible and (event.is_action_pressed("ui_cancel") or event.is_action_pressed("pause")):
		show_menu()
		get_viewport().set_input_as_handled()


func _notification(what: int) -> void:
	# Android Back button
	if what == NOTIFICATION_WM_GO_BACK_REQUEST:
		if credits_panel.visible:
			show_menu()
		else:
			quit_to_desktop()


func start_game() -> void:
	# UE: Set Input Mode Game Only before Open Level
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	get_tree().change_scene_to_file(game_level)


func show_credits() -> void:
	main_panel.visible = false
	credits_panel.visible = true
	back_button.grab_focus()


## UE: ShowMenu / HideCredits
func show_menu() -> void:
	credits_panel.visible = false
	main_panel.visible = true
	start_button.grab_focus()


func quit_to_desktop() -> void:
	get_tree().quit()
