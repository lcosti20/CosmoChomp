extends SceneTree
## Watches the aliens for a while: they should wander through the maze, and chase a
## player a few tiles away. Prints distance travelled, states and steps to the player.
## Run: godot --headless --path . --script tools/alien_test.gd


func _initialize() -> void:
	change_scene_to_file("res://scenes/levels/cosmo_chomp.tscn")
	_run.call_deferred()


func _run() -> void:
	await create_timer(4.5).timeout
	var game_mode := get_first_node_in_group("game_mode") as CosmoGameMode
	var player := get_first_node_in_group("player") as Astronaut
	var maze := game_mode.get_maze()
	player.invulnerability_duration = 1000.0
	var aliens := get_nodes_in_group("aliens")
	var start: Array[Vector3] = []
	for alien: Alien in aliens:
		start.append(alien.global_position)
	await create_timer(5.0).timeout
	for index: int in aliens.size():
		var alien := aliens[index] as Alien
		print("alien %d moved %.1f m, state %d, tile %d" % [index, alien.global_position.distance_to(start[index]), alien.state, alien.current_tile])

	# Put one alien a few tiles from the player and watch it close in
	var chaser := aliens[0] as Alien
	var distances := maze.compute_distances(maze.find_nearest_tile(player.global_position))
	var tile := distances.find(3)
	chaser.global_position = maze.get_tile_location(tile, 1.0)
	chaser.current_tile = -1
	for step: int in 8:
		await create_timer(0.75).timeout
		var steps := game_mode.get_player_distances()[chaser.current_tile] if chaser.current_tile >= 0 else -1
		print("chase t=%.2f state %d steps %d dist %.1f health %d" % [(step + 1) * 0.75, chaser.state, steps, chaser.global_position.distance_to(player.global_position), player.player_get_health()])
	quit()
