class_name TouchControls
extends Control
## On-screen controls for phones and tablets (no UE counterpart; the original only had
## keyboard, mouse and gamepad).
##
## A touch on the left half of the screen places a move stick under the finger; a drag
## on the right half looks around; Jump, Fire and Pause are round buttons. The stick
## and buttons drive the same input actions as the keyboard and gamepad, so the player
## code doesn't know the difference.
##
## While the game runs, touches don't also become mouse events: the left mouse button
## fires and mouse motion looks, so every tap would shoot. The pause menu needs those
## mouse events for its buttons, so they come back while the game is paused.

## Show the controls even without a touchscreen (for testing on desktop)
@export var always_show: bool = false
## Degrees per pixel of look drag on a 1080-pixel-tall screen
@export var look_sensitivity: float = 0.15
@export var stick_radius: float = 130.0
@export var button_radius: float = 95.0
@export var pause_button_radius: float = 50.0
## Dark fill with a light rim, so the controls read against both the white ground and space
@export var base_color: Color = Color(0.02, 0.04, 0.1, 0.45)
@export var active_color: Color = Color(0.25, 0.3, 0.45, 0.65)
@export var fire_color: Color = Color(0.6, 0.05, 0.35, 0.6)
@export var rim_color: Color = Color(1.0, 1.0, 1.0, 0.6)
@export var label_color: Color = Color(1.0, 1.0, 1.0, 0.9)

enum Role { STICK, LOOK, JUMP, FIRE, PAUSE }

const MOVE_ACTIONS := [&"move_left", &"move_right", &"move_back", &"move_forward"]

## Finger index -> Role
var _fingers: Dictionary = {}
var _stick_origin: Vector2 = Vector2.ZERO
var _stick_offset: Vector2 = Vector2.ZERO
var _font: Font = null


## Phones, tablets and mobile browsers. A touchscreen laptop keeps mouse and keyboard.
static func is_touch_device() -> bool:
	return DisplayServer.is_touchscreen_available() and (OS.has_feature("mobile") or OS.has_feature("web_android") or OS.has_feature("web_ios"))


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	# Keeps running while paused to hand touch-to-mouse back to the pause menu
	process_mode = Node.PROCESS_MODE_ALWAYS
	_font = ThemeDB.fallback_font
	visible = always_show or is_touch_device()
	if not visible:
		set_process(false)
		set_process_input(false)


func _exit_tree() -> void:
	_release_all()
	Input.emulate_mouse_from_touch = true


func _process(_delta: float) -> void:
	var paused := get_tree().paused
	Input.emulate_mouse_from_touch = paused
	if paused and not _fingers.is_empty():
		_release_all()
	queue_redraw()


func _input(event: InputEvent) -> void:
	if get_tree().paused:
		return
	var touch := event as InputEventScreenTouch
	if touch:
		if touch.pressed:
			_touch_down(touch.index, touch.position)
		else:
			_touch_up(touch.index)
		get_viewport().set_input_as_handled()
		return
	var drag := event as InputEventScreenDrag
	if drag:
		_touch_drag(drag.index, drag.position, drag.relative)
		get_viewport().set_input_as_handled()


func _touch_down(index: int, at: Vector2) -> void:
	var role := _role_at(at)
	if role == -1:
		return
	_fingers[index] = role
	match role:
		Role.STICK:
			_stick_origin = at
			_stick_offset = Vector2.ZERO
		Role.JUMP:
			_send_action(&"jump", true)
		Role.FIRE:
			_send_action(&"fire", true)
		Role.PAUSE:
			_send_action(&"pause", true)
			_send_action(&"pause", false)


func _touch_up(index: int) -> void:
	if not _fingers.has(index):
		return
	var role: int = _fingers[index]
	_fingers.erase(index)
	match role:
		Role.STICK:
			_stick_offset = Vector2.ZERO
			_apply_stick()
		Role.JUMP:
			_send_action(&"jump", false)
		Role.FIRE:
			_send_action(&"fire", false)


func _touch_drag(index: int, at: Vector2, relative: Vector2) -> void:
	if not _fingers.has(index):
		return
	match _fingers[index]:
		Role.STICK:
			_stick_offset = (at - _stick_origin).limit_length(stick_radius * _ui_scale())
			_apply_stick()
		Role.LOOK:
			var player := get_tree().get_first_node_in_group("player") as Astronaut
			if player:
				# Drag up looks up, like the mouse
				var degrees := relative * look_sensitivity * (1080.0 / maxf(size.y, 1.0))
				player.do_look(degrees.x, -degrees.y)


## Which control a new touch at [param at] belongs to, or -1 if it's taken.
func _role_at(at: Vector2) -> int:
	var ui_scale := _ui_scale()
	if at.distance_to(_pause_center()) <= pause_button_radius * ui_scale * 1.3:
		return Role.PAUSE
	if at.distance_to(_jump_center()) <= button_radius * ui_scale:
		return Role.JUMP
	if _fire_available() and at.distance_to(_fire_center()) <= button_radius * ui_scale:
		return Role.FIRE
	var wanted := Role.STICK if at.x < size.x * 0.5 else Role.LOOK
	return -1 if _fingers.values().has(wanted) else wanted


## Splits the stick into the four move actions, with analog strength (like a gamepad stick).
func _apply_stick() -> void:
	var value := _stick_offset / (stick_radius * _ui_scale())
	var strengths := [maxf(-value.x, 0.0), maxf(value.x, 0.0), maxf(value.y, 0.0), maxf(-value.y, 0.0)]
	for i: int in MOVE_ACTIONS.size():
		if strengths[i] > 0.0:
			Input.action_press(MOVE_ACTIONS[i], strengths[i])
		else:
			Input.action_release(MOVE_ACTIONS[i])


## Sends the action through the input pipeline, so code waiting for the pressed event
## (jump, pause) sees it as well as code that polls (fire).
func _send_action(action: StringName, pressed: bool) -> void:
	var event := InputEventAction.new()
	event.action = action
	event.pressed = pressed
	event.strength = 1.0 if pressed else 0.0
	Input.parse_input_event(event)


func _release_all() -> void:
	for index: int in _fingers.keys():
		_touch_up(index)
	_fingers.clear()


func _fire_available() -> bool:
	var player := get_tree().get_first_node_in_group("player") as Astronaut
	return player != null and player.ray_gun_time_remaining > 0.0


# Layout and drawing
# ------------------------------------------------

func _ui_scale() -> float:
	return maxf(minf(size.x, size.y) / 1080.0, 0.5)


func _jump_center() -> Vector2:
	var ui_scale := _ui_scale()
	return Vector2(size.x - 160.0 * ui_scale, size.y - 160.0 * ui_scale)


func _fire_center() -> Vector2:
	var ui_scale := _ui_scale()
	return Vector2(size.x - 380.0 * ui_scale, size.y - 210.0 * ui_scale)


## Top center, above the banners and clear of the stats and the mini map
func _pause_center() -> Vector2:
	return Vector2(size.x * 0.5, 75.0 * _ui_scale())


func _draw() -> void:
	var ui_scale := _ui_scale()
	if _fingers.values().has(Role.STICK):
		draw_circle(_stick_origin, stick_radius * ui_scale, base_color)
		draw_arc(_stick_origin, stick_radius * ui_scale, 0.0, TAU, 48, rim_color, 3.0 * ui_scale, true)
		draw_circle(_stick_origin + _stick_offset, stick_radius * ui_scale * 0.45, active_color)
		draw_arc(_stick_origin + _stick_offset, stick_radius * ui_scale * 0.45, 0.0, TAU, 32, rim_color, 3.0 * ui_scale, true)
	_draw_button(_jump_center(), button_radius * ui_scale, "JUMP", base_color, Role.JUMP)
	if _fire_available():
		_draw_button(_fire_center(), button_radius * ui_scale, "FIRE", fire_color, Role.FIRE)
	_draw_button(_pause_center(), pause_button_radius * ui_scale, "II", base_color, Role.PAUSE)


func _draw_button(center: Vector2, radius: float, label: String, color: Color, role: Role) -> void:
	draw_circle(center, radius, active_color if _fingers.values().has(role) else color)
	draw_arc(center, radius, 0.0, TAU, 48, rim_color, maxf(radius * 0.035, 2.0), true)
	var font_size := int(radius * 0.42)
	var text_size := _font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
	draw_string(_font, center + Vector2(-text_size.x * 0.5, text_size.y * 0.3), label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, label_color)
