extends SceneTree
## Runs a level for a few seconds and saves screenshots, for checking the port visually.
## Run: godot --path . --script tools/capture_level.gd -- <scene> <out_dir> [seconds...]
## Optional: "walk" makes the player hold forward between captures; "fire" equips and fires the ray gun.

var _scene_path: String = "res://scenes/levels/cosmo_chomp.tscn"
var _out_dir: String = "user://captures"
var _times: Array[float] = [1.0, 3.0]
var _walk: bool = false
var _fire: bool = false


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		_scene_path = args[0]
	if args.size() > 1:
		_out_dir = args[1]
	var times: Array[float] = []
	for index: int in range(2, args.size()):
		if args[index] == "walk":
			_walk = true
		elif args[index] == "fire":
			_fire = true
		else:
			times.append(float(args[index]))
	if not times.is_empty():
		_times = times
	DirAccess.make_dir_recursive_absolute(_out_dir)
	change_scene_to_file(_scene_path)
	_run.call_deferred()


var _floor_frames: int = 0
var _air_frames: int = 0


func _count_floor() -> void:
	var player := get_first_node_in_group("player") as CharacterBody3D
	if player:
		if player.is_on_floor():
			_floor_frames += 1
		else:
			_air_frames += 1


func _run() -> void:
	physics_frame.connect(_count_floor)
	var elapsed := 0.0
	for index: int in _times.size():
		var wait := _times[index] - elapsed
		if _walk:
			Input.action_press("move_forward")
		await create_timer(maxf(wait, 0.0)).timeout
		elapsed = _times[index]
		if _fire:
			_fire_ray_gun()
		await process_frame
		await process_frame
		var image := root.get_texture().get_image()
		var path := _out_dir.path_join("capture_%02d.png" % index)
		image.save_png(path)
		print("CAPTURE ", path, " at ", elapsed)
		_print_state()
	Input.action_release("move_forward")
	quit()


func _fire_ray_gun() -> void:
	var player := get_first_node_in_group("player")
	if player:
		player.player_equip_ray_gun(10.0)
		player.fire_cooldown_remaining = 0.0
		player.do_fire()


func _print_state() -> void:
	var player := get_first_node_in_group("player") as Node3D
	if player:
		print("  player pos ", player.global_position, " |pos| ", player.global_position.length(), " on_floor ", (player as CharacterBody3D).is_on_floor(), " health ", player.player_get_health(), " orbs ", player.player_get_carried_orbs(), " vel ", (player as CharacterBody3D).velocity, " input ", Input.get_vector("move_left", "move_right", "move_back", "move_forward"), " movement ", player.get("movement_enabled"), " up ", (player as CharacterBody3D).up_direction)
	print("  floor frames ", _floor_frames, " air frames ", _air_frames)
	print("  orbs ", get_nodes_in_group("energy_orbs").size(), " aliens ", get_nodes_in_group("aliens").size(), " ray guns ", get_nodes_in_group("ray_gun_pickups").size())
	for alien: Node3D in get_nodes_in_group("aliens"):
		print("    alien ", alien.global_position.length(), " state ", alien.get("state"))
