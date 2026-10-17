extends SceneTree
## Plays through the core rules of a round without input and prints PASS/FAIL lines.
## Run: godot --headless --path . --script tools/smoke_test.gd

var _failures: int = 0


func _initialize() -> void:
	change_scene_to_file("res://scenes/levels/cosmo_chomp.tscn")
	_run.call_deferred()


func _check(label: String, ok: bool, detail: String = "") -> void:
	print(("PASS " if ok else "FAIL ") + label + ("" if detail.is_empty() else "  (" + detail + ")"))
	if not ok:
		_failures += 1


func _wait(seconds: float) -> void:
	await create_timer(seconds).timeout


func _run() -> void:
	await _wait(0.5)
	var game_mode := get_first_node_in_group("game_mode") as CosmoGameMode
	var player := get_first_node_in_group("player") as Astronaut
	var maze := game_mode.get_maze()
	_check("maze generated", maze.get_num_tiles() == 252, "%d tiles" % maze.get_num_tiles())
	_check("orbs spawned (one per tile but the start)", get_nodes_in_group("energy_orbs").size() == maze.get_num_tiles() - 1)
	_check("rocket requires half the orbs", game_mode.get_rocket().required_orbs == ceili((maze.get_num_tiles() - 1) * 0.5))

	# Every tile reachable through the maze
	var distances := maze.compute_distances(game_mode.start_tile)
	_check("maze is connected", not distances.has(-1))

	# Collect an orb by standing on it
	var orb := get_first_node_in_group("energy_orbs") as EnergyOrb
	player.global_position = orb.global_position
	await _wait(0.3)
	_check("orb collected", player.player_get_carried_orbs() == 1, "carrying %d" % player.player_get_carried_orbs())

	# Deposit at the rocket
	var rocket := game_mode.get_rocket()
	player.global_position = maze.project_to_surface(rocket.global_position + rocket.global_basis.x * 2.5, 1.0)
	await _wait(0.3)
	_check("orbs deposited at rocket", rocket.get_stored_orbs() == 1 and player.player_get_carried_orbs() == 0, "stored %d" % rocket.get_stored_orbs())

	# Getting hurt scatters carried orbs and costs health
	player.player_add_orbs(3)
	var orbs_before := get_nodes_in_group("energy_orbs").size()
	player.take_damage(1.0)
	await _wait(0.1)
	_check("damage costs one health", player.player_get_health() == 2)
	_check("carried orbs scattered", player.player_get_carried_orbs() == 0 and get_nodes_in_group("energy_orbs").size() == orbs_before + 3)
	player.take_damage(1.0)
	_check("invulnerable after a hit", player.player_get_health() == 2)

	# Aliens appear after the grace period
	await _wait(4.0)
	var aliens := get_nodes_in_group("aliens")
	_check("aliens spawned", aliens.size() == 4, "%d" % aliens.size())

	# Shoot an alien standing in front of the player
	# Back at the start, look away from the rocket: the start tile's exits are always open
	var alien := aliens[0] as Alien
	var start_location := maze.project_to_surface(game_mode.get_rocket().global_position, 0.0)
	player.global_position = maze.project_to_surface(start_location + game_mode._compute_outward() * maze.get_tile_spacing() * 0.42, 0.9)
	player.look_yaw = 180.0
	player.look_pitch = 0.0
	await physics_frame
	await physics_frame
	var up := player.planetary_gravity.get_current_surface_up()
	var ahead := PlanetMath.flatten_direction(-player.control_basis.z, up)
	alien.global_position = maze.project_to_surface(player.global_position + ahead * 3.0, 0.9)
	alien.current_tile = -1
	player.invulnerability_remaining = 0.0
	player.player_equip_ray_gun(10.0)
	# Let a physics step move the teleported alien's collider
	await physics_frame
	await physics_frame
	player.do_fire()
	await _wait(0.1)
	_check("ray gun kills alien", alien.is_dead() or not is_instance_valid(alien), "shot hit %s" % player._hit_actor)

	# An alien next to the player attacks
	await _wait(1.0)
	player.ray_gun_time_remaining = 0.0
	player.invulnerability_remaining = 0.0
	var attacker := get_nodes_in_group("aliens")[1] as Alien
	attacker.global_position = maze.project_to_surface(player.global_position + ahead * 1.2, 0.9)
	attacker.current_tile = -1
	var health_before := player.player_get_health()
	await _wait(1.5)
	_check("alien attack hurts the player", player.player_get_health() < health_before, "health %d -> %d" % [health_before, player.player_get_health()])

	# Dying ends the round
	player.invulnerability_remaining = 0.0
	player.take_damage(5.0)
	await _wait(0.1)
	_check("player dies", player.player_is_dead() and not player.player_is_in_play())
	_check("round over banner", game_mode.get_banner_text() == "The aliens got you!")

	# The level restarts after the delay; then refuel the rocket to win
	await _wait(game_mode.restart_delay + 1.0)
	game_mode = get_first_node_in_group("game_mode") as CosmoGameMode
	player = get_first_node_in_group("player") as Astronaut
	_check("round restarted", player != null and player.player_is_in_play())
	rocket = game_mode.get_rocket()
	maze = game_mode.get_maze()
	rocket.required_orbs = 2
	player.player_add_orbs(2)
	player.global_position = maze.project_to_surface(rocket.global_position + rocket.global_basis.x * 2.5, 1.0)
	await _wait(0.5)
	_check("rocket launches when refueled", rocket.has_launched and not player.player_is_in_play())
	_check("escape banner", game_mode.get_banner_text() == "Rocket refueled - you escaped the planet!")
	await _wait(2.0)
	_check("rocket flies away from the planet", rocket.global_position.length() > maze.get_planet_radius() + 5.0, "%.1f" % rocket.global_position.length())

	print("SMOKE TEST DONE, failures: %d" % _failures)
	quit(1 if _failures > 0 else 0)
