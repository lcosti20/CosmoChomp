extends SceneTree
## Drives the on-screen touch controls with synthetic touches and checks the player reacts.
## Run: godot --path . --script tools/touch_test.gd -- [screenshot.png]

var _failures: int = 0


func _initialize() -> void:
	change_scene_to_file("res://scenes/levels/cosmo_chomp.tscn")
	_run.call_deferred()


func _check(passed: bool, label: String) -> void:
	print(("PASS " if passed else "FAIL ") + label)
	if not passed:
		_failures += 1


## Touch events arrive in window pixels; the controls lay out in the stretched UI's coordinates
func _touch(index: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = root.get_final_transform() * at
	event.pressed = pressed
	Input.parse_input_event(event)


func _drag(index: int, at: Vector2, relative: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = root.get_final_transform() * at
	event.relative = root.get_final_transform().basis_xform(relative)
	Input.parse_input_event(event)


func _run() -> void:
	await create_timer(4.5).timeout
	var touch := current_scene.get_node("HUDLayer/TouchControls") as TouchControls
	var player := get_first_node_in_group("player") as Astronaut
	touch.always_show = true
	touch.visible = true
	touch.set_process(true)
	touch.set_process_input(true)
	await process_frame
	var size := touch.size
	player.invulnerability_duration = 1000.0
	# Checked first: a ray gun pickup lies near the spawn point
	_check(not touch._fire_available(), "no FIRE button without a ray gun")

	# Move stick: press on the left half and push up
	var start := player.global_position
	var origin := Vector2(size.x * 0.2, size.y * 0.7)
	_touch(0, origin, true)
	_drag(0, origin + Vector2(0, -200), Vector2(0, -200))
	await create_timer(1.0).timeout
	_check(Input.get_action_strength("move_forward") > 0.9, "stick pushes move_forward")
	_check(player.global_position.distance_to(start) > 1.0, "player walks  (%.2f m)" % player.global_position.distance_to(start))
	_check(not Input.emulate_mouse_from_touch, "touches don't become mouse clicks while playing")

	# Look: drag on the right half while still holding the stick
	var yaw := player.look_yaw
	_touch(1, Vector2(size.x * 0.75, size.y * 0.5), true)
	await process_frame
	_drag(1, Vector2(size.x * 0.75 + 300, size.y * 0.5), Vector2(300, 0))
	await process_frame
	_touch(1, Vector2(size.x * 0.75 + 300, size.y * 0.5), false)
	await process_frame
	_check(player.look_yaw > yaw + 10.0, "right-side drag turns the camera  (%.1f deg)" % (player.look_yaw - yaw))
	_touch(0, origin, false)
	await process_frame
	_check(Input.get_action_strength("move_forward") == 0.0, "lifting the finger stops moving")

	# Jump
	await create_timer(0.8).timeout
	_touch(2, touch._jump_center(), true)
	await create_timer(0.15).timeout
	_check(not player.is_on_floor(), "JUMP button jumps")
	_touch(2, touch._jump_center(), false)

	# Fire
	player.player_equip_ray_gun(10.0)
	await create_timer(1.0).timeout
	var beams_before := current_scene.find_children("*", "RayGunBeam", true, false).size()
	_touch(3, touch._fire_center(), true)
	await create_timer(0.1).timeout
	_check(player.fire_cooldown_remaining > 0.0, "FIRE button shoots")
	if OS.get_cmdline_user_args().size() > 0:
		await process_frame
		root.get_texture().get_image().save_png(OS.get_cmdline_user_args()[0])
	_touch(3, touch._fire_center(), false)
	await process_frame
	_check(not Input.is_action_pressed("fire"), "releasing FIRE stops shooting")

	# Pause button opens the menu, and the menu gets mouse clicks back
	_touch(4, touch._pause_center(), true)
	_touch(4, touch._pause_center(), false)
	await process_frame
	await process_frame
	_check(paused, "pause button pauses")
	_check(Input.emulate_mouse_from_touch, "pause menu receives taps as clicks")
	print("TOUCH TEST DONE, failures: %d (beams before fire: %d)" % [_failures, beams_before])
	quit(1 if _failures > 0 else 0)
