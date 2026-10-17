@tool
class_name HexMaze
extends Node3D
## Wraps the "Planet"-group node in a randomly generated maze of hexagonal tiles
## (port of BP_HexMaze, which implements BPI_Maze).
##
## The planet surface is split into a hex grid sized from target_tile_size (a Goldberg
## polyhedron: a subdivided icosahedron, all hexagons plus the 12 pentagons a sphere
## needs), a maze is carved through it, and wall meshes are stretched along every
## closed tile edge. Also answers tile/path queries for gameplay (spawning, alien
## navigation). Generation is driven by the game mode at the start of each round; use
## "Preview In Editor" in the inspector to see a layout without playing.

## Angle subtended by one icosahedron edge on its circumscribed sphere
const ICOSAHEDRON_EDGE_ANGLE := 1.1071487177940904
const MAX_NEIGHBORS := 6

const WALLS_SCENE := preload("res://assets/models/glb/walls.glb")

@export_group("Planet")
## If true, center on the nearest "Planet"-group node and read the radius from it
@export var auto_detect_planet: bool = true
## Planet radius used when auto_detect_planet is off (or no planet is found)
@export_range(1.0, 1000.0) var planet_radius: float = 30.0

@export_group("Layout")
## Desired distance between neighboring tile centers; the closest matching subdivision is used
@export_range(1.0, 100.0) var target_tile_size: float = 6.5
## Chance to knock a dead end through to a neighbor, which adds loops so the maze can be escaped around
@export_range(0.0, 1.0) var braid_chance: float = 0.85
## Random seed for the layout. 0 picks a new layout every round.
@export var maze_seed: int = 0

@export_group("Walls")
## Wall pieces (node names in walls.glb), picked at random per edge and stretched to fit it
@export var wall_pieces: PackedStringArray = PackedStringArray(["Wall_04", "Wall_05"])
## Post placed on tile corners where walls meet, hiding the joints between wall pieces
@export var post_piece: String = "Wall_03"
@export_range(0.1, 20.0) var wall_height: float = 2.2
@export_range(0.01, 5.0) var wall_thickness: float = 0.4
@export_range(0.01, 5.0) var post_width: float = 0.75
## How far walls extend below the surface so curvature never leaves a gap at their base
@export_range(0.0, 5.0) var wall_sink: float = 0.4

@export_tool_button("Preview In Editor") var preview_in_editor_button: Callable = preview_in_editor
@export_tool_button("Clear Preview") var clear_preview_button: Callable = clear_preview

# Grid (stored on the unit sphere; scale by the radius to place things)
var tile_centers: PackedVector3Array = PackedVector3Array()
## Neighbors per tile, sorted by angle around the tile
var tile_neighbors: Array[PackedInt32Array] = []
## Corner indices per tile, sorted by angle around the tile
var tile_corners: Array[PackedInt32Array] = []
## Unit directions of every tile corner
var corners: PackedVector3Array = PackedVector3Array()
## Edge key -> the two corners bounding the edge between two adjacent tiles
var edge_corners: Dictionary = {}
## Edge keys with no wall
var passages: Dictionary = {}

var planet_center: Vector3 = Vector3.ZERO
var radius: float = 30.0
var frequency: int = 1

var _walls_root: Node3D = null


func _ready() -> void:
	add_to_group("maze")
	if Engine.is_editor_hint():
		return
	# Drop any editor preview; the game mode generates a fresh layout for the round
	clear_walls()


# Generation
# ------------------------------------------------

## Builds the hex grid for the current planet. Called automatically by generate_maze if needed.
func build_grid() -> void:
	planet_center = global_position
	radius = planet_radius

	if auto_detect_planet:
		var nearest: Node3D = null
		for planet: Node in get_tree().get_nodes_in_group("Planet"):
			var planet_3d := planet as Node3D
			if planet_3d == null:
				continue
			if nearest == null or planet_3d.global_position.distance_squared_to(global_position) < nearest.global_position.distance_squared_to(global_position):
				nearest = planet_3d

		# Measure only what characters stand on; the planet also has a larger gravity well
		var surface_radius := 0.0
		if nearest and nearest.has_method("get_surface_radius"):
			surface_radius = nearest.get_surface_radius()

		if surface_radius > 0.0:
			planet_center = nearest.global_position
			radius = surface_radius
		else:
			push_warning("HexMaze: no node in the 'Planet' group found, using planet_radius %.1f" % planet_radius)

	frequency = clampi(roundi(radius * tile_spacing_radians(1) / target_tile_size), 1, 24)
	_build_planet_grid(frequency)
	passages.clear()

	print("HexMaze: radius %.1f, frequency %d, %d tiles, spacing %.2f" % [radius, frequency, tile_centers.size(), get_tile_spacing()])


## Carves a new maze (fully opening start_tile into a plaza) and rebuilds the walls.
func generate_maze(start_tile: int = 0) -> void:
	if tile_centers.is_empty():
		build_grid()
	if start_tile < 0 or start_tile >= tile_centers.size():
		start_tile = 0

	var stream := RandomNumberGenerator.new()
	if maze_seed != 0:
		stream.seed = maze_seed
	else:
		stream.randomize()

	passages.clear()
	_carve_passages(start_tile, stream)
	_braid_dead_ends(stream)

	# Open the start tile up completely so there's room for the rocket and the player
	for neighbor: int in tile_neighbors[start_tile]:
		passages[edge_key(start_tile, neighbor)] = true

	_build_walls(stream)


func clear_walls() -> void:
	if _walls_root and is_instance_valid(_walls_root):
		_walls_root.queue_free()
	_walls_root = null
	# Previews made in the editor are not owned by the scene, so they are never saved
	for child: Node in get_children():
		if child.name.begins_with("Walls"):
			child.queue_free()


func preview_in_editor() -> void:
	build_grid()
	generate_maze(find_nearest_tile(planet_center + Vector3.UP * radius))


func clear_preview() -> void:
	clear_walls()


func _carve_passages(start_tile: int, stream: RandomNumberGenerator) -> void:
	# Randomized depth-first search: produces long, winding corridors
	var visited := PackedByteArray()
	visited.resize(tile_centers.size())
	visited.fill(0)

	var stack: Array[int] = [start_tile]
	visited[start_tile] = 1

	var candidates: Array[int] = []
	while not stack.is_empty():
		var current: int = stack.back()

		candidates.clear()
		for neighbor: int in tile_neighbors[current]:
			if visited[neighbor] == 0:
				candidates.append(neighbor)

		if candidates.is_empty():
			stack.pop_back()
			continue

		var next: int = candidates[stream.randi_range(0, candidates.size() - 1)]
		passages[edge_key(current, next)] = true
		visited[next] = 1
		stack.push_back(next)


func _braid_dead_ends(stream: RandomNumberGenerator) -> void:
	# A perfect maze has exactly one route between tiles, which makes aliens impossible
	# to escape. Knocking out dead ends adds the loops a Pac-Man style chase needs.
	var order: Array[int] = []
	for tile: int in tile_centers.size():
		order.append(tile)
	for index: int in range(order.size() - 1, 0, -1):
		var other := stream.randi_range(0, index)
		var swap := order[index]
		order[index] = order[other]
		order[other] = swap

	var closed: Array[int] = []
	var closed_dead_ends: Array[int] = []
	for tile: int in order:
		if get_open_neighbors(tile).size() != 1 or stream.randf() >= braid_chance:
			continue

		closed.clear()
		closed_dead_ends.clear()
		for neighbor: int in tile_neighbors[tile]:
			if not is_passage(tile, neighbor):
				closed.append(neighbor)
				if get_open_neighbors(neighbor).size() == 1:
					closed_dead_ends.append(neighbor)

		# Prefer joining two dead ends, which removes both with a single wall
		var choices := closed if closed_dead_ends.is_empty() else closed_dead_ends
		if not choices.is_empty():
			passages[edge_key(tile, choices[stream.randi_range(0, choices.size() - 1)])] = true


func _build_walls(stream: RandomNumberGenerator) -> void:
	clear_walls()

	var piece_source := WALLS_SCENE.instantiate()
	var wall_meshes: Array[Mesh] = []
	for piece: String in wall_pieces:
		var mesh := _find_piece_mesh(piece_source, piece)
		if mesh:
			wall_meshes.append(mesh)
	var post_mesh := _find_piece_mesh(piece_source, post_piece)
	piece_source.free()

	if wall_meshes.is_empty():
		push_warning("HexMaze: no wall meshes set")
		return

	_walls_root = Node3D.new()
	_walls_root.name = "Walls"
	add_child(_walls_root)

	# Walls block characters and ray gun shots (UE: BlockAll instanced static meshes)
	var collision := StaticBody3D.new()
	collision.name = "WallCollision"
	collision.collision_layer = Layers.WORLD
	collision.collision_mask = 0
	_walls_root.add_child(collision)

	var wall_transforms: Array = []
	for mesh: Mesh in wall_meshes:
		wall_transforms.append([])

	var corner_has_wall := PackedByteArray()
	corner_has_wall.resize(corners.size())
	corner_has_wall.fill(0)

	var to_local := global_transform.affine_inverse()
	for tile: int in tile_centers.size():
		for neighbor: int in tile_neighbors[tile]:
			# Visit each edge once, and only the ones that are walls
			if neighbor < tile or is_passage(tile, neighbor):
				continue
			var edge := get_edge_corners(tile, neighbor)
			if edge.x < 0:
				continue
			corner_has_wall[edge.x] = 1
			corner_has_wall[edge.y] = 1

			var point_a := corners[edge.x] * radius
			var point_b := corners[edge.y] * radius
			var up := (point_a + point_b).normalized()
			var along := PlanetMath.flatten_direction(point_b - point_a, up)
			# Mesh X runs along the edge, Y is up and Z is the thickness
			var rot := Basis(along, up, along.cross(up))
			var base := planet_center + up * (radius - wall_sink)
			var size := Vector3(point_a.distance_to(point_b), wall_height + wall_sink, wall_thickness)

			var mesh_index := stream.randi_range(0, wall_meshes.size() - 1)
			var xform := to_local * _fit_mesh_transform(wall_meshes[mesh_index], base, rot, size)
			wall_transforms[mesh_index].append(xform)
			_add_box_collision(collision, to_local * Transform3D(rot, base), size)

	for index: int in wall_meshes.size():
		_add_multimesh(wall_meshes[index], wall_transforms[index])

	if post_mesh:
		var post_size := Vector3(post_width, wall_height + wall_sink + 0.15, post_width)
		var post_transforms: Array = []
		for corner: int in corners.size():
			if corner_has_wall[corner] == 0:
				continue
			var up := corners[corner]
			var rot := PlanetMath.basis_from_up(up)
			var base := planet_center + up * (radius - wall_sink)
			post_transforms.append(to_local * _fit_mesh_transform(post_mesh, base, rot, post_size))
			_add_box_collision(collision, to_local * Transform3D(rot, base), post_size)
		_add_multimesh(post_mesh, post_transforms)


func _find_piece_mesh(source: Node, piece: String) -> Mesh:
	var node := source.find_child(piece, true, false)
	if node == null:
		return null
	if node is MeshInstance3D:
		return (node as MeshInstance3D).mesh
	for mesh_instance: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
		return mesh_instance.mesh
	return null


func _add_multimesh(mesh: Mesh, transforms: Array) -> void:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	multimesh.instance_count = transforms.size()
	for index: int in transforms.size():
		multimesh.set_instance_transform(index, transforms[index])
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = multimesh
	_walls_root.add_child(instance)


## Box collider covering a bottom-centered piece of the given size (X length, Y height, Z thickness).
func _add_box_collision(body: StaticBody3D, base_transform: Transform3D, size: Vector3) -> void:
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	shape.transform = base_transform * Transform3D(Basis.IDENTITY, Vector3(0.0, size.y * 0.5, 0.0))
	body.add_child(shape)


## Transform that sits a mesh's bottom-center at base, stretched to size (X length, Y height, Z thickness).
static func _fit_mesh_transform(mesh: Mesh, base: Vector3, rot: Basis, size: Vector3) -> Transform3D:
	var bounds := mesh.get_aabb()
	var mesh_size := bounds.size.max(Vector3.ONE * PlanetMath.NEARLY_ZERO)
	var mesh_scale := size / mesh_size
	var center := bounds.get_center()
	var bottom_center := Vector3(center.x, bounds.position.y, center.z)
	return Transform3D(rot.scaled_local(mesh_scale), base - rot * (bottom_center * mesh_scale))


# Grid
# ------------------------------------------------

static func edge_key(a: int, b: int) -> int:
	return PlanetMath.make_edge_key(a, b)


## Average angular distance (radians) between neighboring tile centers for a given frequency.
static func tile_spacing_radians(grid_frequency: int) -> float:
	return ICOSAHEDRON_EDGE_ANGLE / maxi(1, grid_frequency)


## Returns the two corners shared by adjacent tiles a and b, or (-1, -1).
func get_edge_corners(a: int, b: int) -> Vector2i:
	return edge_corners.get(edge_key(a, b), Vector2i(-1, -1))


## Rebuilds the grid. grid_frequency is the number of subdivisions per icosahedron edge;
## the grid has 10 * frequency^2 + 2 tiles. Every geodesic vertex becomes a tile center
## and every geodesic triangle becomes a tile corner.
func _build_planet_grid(grid_frequency: int) -> void:
	grid_frequency = maxi(1, grid_frequency)
	tile_centers = PackedVector3Array()
	tile_neighbors.clear()
	tile_corners.clear()
	corners = PackedVector3Array()
	edge_corners.clear()

	var phi := (1.0 + sqrt(5.0)) * 0.5
	var ico_verts: Array[Vector3] = [
		Vector3(-1, phi, 0), Vector3(1, phi, 0), Vector3(-1, -phi, 0), Vector3(1, -phi, 0),
		Vector3(0, -1, phi), Vector3(0, 1, phi), Vector3(0, -1, -phi), Vector3(0, 1, -phi),
		Vector3(phi, 0, -1), Vector3(phi, 0, 1), Vector3(-phi, 0, -1), Vector3(-phi, 0, 1),
	]
	var ico_faces: Array[Vector3i] = [
		Vector3i(0, 11, 5), Vector3i(0, 5, 1), Vector3i(0, 1, 7), Vector3i(0, 7, 10), Vector3i(0, 10, 11),
		Vector3i(1, 5, 9), Vector3i(5, 11, 4), Vector3i(11, 10, 2), Vector3i(10, 7, 6), Vector3i(7, 1, 8),
		Vector3i(3, 9, 4), Vector3i(3, 4, 2), Vector3i(3, 2, 6), Vector3i(3, 6, 8), Vector3i(3, 8, 9),
		Vector3i(4, 9, 5), Vector3i(2, 4, 11), Vector3i(6, 2, 10), Vector3i(8, 6, 7), Vector3i(9, 8, 1),
	]

	# Geodesic subdivision: vertices become tiles, triangles become corners
	var vertices := PackedVector3Array()
	var triangles: Array[Vector3i] = []
	var face_grid := PackedInt32Array()
	face_grid.resize((grid_frequency + 1) * (grid_frequency + 1))

	for face: Vector3i in ico_faces:
		var a := ico_verts[face.x].normalized()
		var b := ico_verts[face.y].normalized()
		var c := ico_verts[face.z].normalized()

		face_grid.fill(-1)
		for i: int in grid_frequency + 1:
			for j: int in grid_frequency - i + 1:
				var u := float(i) / grid_frequency
				var v := float(j) / grid_frequency
				var point := (a * (1.0 - u - v) + b * u + c * v).normalized()
				face_grid[i * (grid_frequency + 1) + j] = _find_or_add_vertex(vertices, point)

		for i: int in grid_frequency:
			for j: int in grid_frequency - i:
				var g00 := face_grid[i * (grid_frequency + 1) + j]
				var g10 := face_grid[(i + 1) * (grid_frequency + 1) + j]
				var g01 := face_grid[i * (grid_frequency + 1) + j + 1]
				triangles.append(Vector3i(g00, g10, g01))
				if i + j < grid_frequency - 1:
					var g11 := face_grid[(i + 1) * (grid_frequency + 1) + j + 1]
					triangles.append(Vector3i(g10, g11, g01))

	# Tiles, corners and adjacency
	tile_centers = vertices
	for index: int in vertices.size():
		tile_neighbors.append(PackedInt32Array())
		tile_corners.append(PackedInt32Array())

	var edge_triangles: Dictionary = {}
	for tri_index: int in triangles.size():
		var tri := triangles[tri_index]
		corners.append((vertices[tri.x] + vertices[tri.y] + vertices[tri.z]).normalized())

		var tri_verts: Array[int] = [tri.x, tri.y, tri.z]
		for side: int in 3:
			var v0 := tri_verts[side]
			var v1 := tri_verts[(side + 1) % 3]
			if not tile_corners[v0].has(tri_index):
				tile_corners[v0].append(tri_index)
			if not tile_neighbors[v0].has(v1):
				tile_neighbors[v0].append(v1)
			if not tile_neighbors[v1].has(v0):
				tile_neighbors[v1].append(v0)
			var key := edge_key(v0, v1)
			var list: PackedInt32Array = edge_triangles.get(key, PackedInt32Array())
			if not list.has(tri_index):
				list.append(tri_index)
			edge_triangles[key] = list

	for key: int in edge_triangles:
		var list: PackedInt32Array = edge_triangles[key]
		if list.size() == 2:
			edge_corners[key] = Vector2i(list[0], list[1])

	# Sort neighbors and corners by angle around each tile so they wind consistently
	for tile: int in tile_centers.size():
		var center := tile_centers[tile]
		var axes := PlanetMath.tangent_basis(center)
		var angle_of := func(point: Vector3) -> float:
			var offset := point - center
			return atan2(offset.dot(axes[1]), offset.dot(axes[0]))
		tile_neighbors[tile] = _sorted_by_angle(tile_neighbors[tile], vertices, angle_of)
		tile_corners[tile] = _sorted_by_angle(tile_corners[tile], corners, angle_of)


static func _find_or_add_vertex(vertices: PackedVector3Array, point: Vector3) -> int:
	# Vertices on shared icosahedron edges are generated once per adjacent face,
	# so merge points that land on the same spot.
	for index: int in vertices.size():
		if vertices[index].distance_squared_to(point) < 1e-10:
			return index
	vertices.append(point)
	return vertices.size() - 1


static func _sorted_by_angle(indices: PackedInt32Array, points: PackedVector3Array, angle_of: Callable) -> PackedInt32Array:
	var list: Array = Array(indices)
	list.sort_custom(func(l: int, r: int) -> bool: return angle_of.call(points[l]) < angle_of.call(points[r]))
	return PackedInt32Array(list)


# Queries (BPI_Maze)
# ------------------------------------------------

func get_num_tiles() -> int:
	return tile_centers.size()


func get_planet_center() -> Vector3:
	return planet_center


func get_planet_radius() -> float:
	return radius


## World distance between neighboring tile centers.
func get_tile_spacing() -> float:
	return radius * tile_spacing_radians(frequency)


## Unit "up" at the tile center.
func get_tile_up(tile: int) -> Vector3:
	return tile_centers[tile] if tile >= 0 and tile < tile_centers.size() else Vector3.UP


## World location at the tile center, height above the planet surface.
func get_tile_location(tile: int, height: float = 0.0) -> Vector3:
	return planet_center + get_tile_up(tile) * (radius + height)


## World location height above the surface point under world_location.
func project_to_surface(world_location: Vector3, height: float = 0.0) -> Vector3:
	return planet_center + PlanetMath.surface_up(world_location, planet_center) * (radius + height)


## Basis with Y along the surface normal at world_location and -Z as close to forward as possible.
func get_surface_rotation(world_location: Vector3, forward: Vector3) -> Basis:
	var up := (world_location - planet_center).normalized()
	return PlanetMath.surface_rotation(up, forward)


func find_nearest_tile(world_location: Vector3) -> int:
	var direction := (world_location - planet_center).normalized()
	var best := -1
	var best_dot := -2.0
	for tile: int in tile_centers.size():
		var dot := tile_centers[tile].dot(direction)
		if dot > best_dot:
			best_dot = dot
			best = tile
	return best


func get_neighbors(tile: int) -> PackedInt32Array:
	return tile_neighbors[tile] if tile >= 0 and tile < tile_neighbors.size() else PackedInt32Array()


## Neighbors reachable from tile without crossing a wall.
func get_open_neighbors(tile: int) -> PackedInt32Array:
	var result := PackedInt32Array()
	for neighbor: int in get_neighbors(tile):
		if is_passage(tile, neighbor):
			result.append(neighbor)
	return result


## True if there is no wall between adjacent tiles a and b.
func is_passage(a: int, b: int) -> bool:
	return passages.has(edge_key(a, b))


## Steps through the maze from from_tile to every tile (-1 if unreachable).
func compute_distances(from_tile: int) -> PackedInt32Array:
	var distances := PackedInt32Array()
	distances.resize(tile_centers.size())
	distances.fill(-1)
	if from_tile < 0 or from_tile >= tile_centers.size():
		return distances

	# Breadth-first search; every step through the maze costs the same
	var queue := PackedInt32Array([from_tile])
	distances[from_tile] = 0
	var head := 0
	while head < queue.size():
		var current := queue[head]
		head += 1
		for neighbor: int in tile_neighbors[current]:
			if distances[neighbor] == -1 and is_passage(current, neighbor):
				distances[neighbor] = distances[current] + 1
				queue.append(neighbor)
	return distances


## Shortest route through the maze, including both ends; empty if unreachable.
func find_path(from_tile: int, to_tile: int) -> PackedInt32Array:
	var path := PackedInt32Array()
	var distances := compute_distances(to_tile)
	if from_tile < 0 or from_tile >= distances.size() or distances[from_tile] == -1:
		return path

	# Walk downhill on the distance field from the destination
	var current := from_tile
	path.append(current)
	while current != to_tile:
		for neighbor: int in tile_neighbors[current]:
			if is_passage(current, neighbor) and distances[neighbor] == distances[current] - 1:
				current = neighbor
				break
		path.append(current)
	return path
