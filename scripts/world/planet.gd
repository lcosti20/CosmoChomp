class_name Planet
extends StaticBody3D
## The planet the maze wraps around (port of BP_Planet, tagged "Planet").
##
## A solid sphere that characters stand on. Gravity components and the maze find it
## through the "Planet" group.

## Surface radius (UE: the engine sphere scaled to 3000 cm)
@export var radius: float = 30.0:
	set(value):
		radius = value
		if is_node_ready():
			_apply_radius()

@onready var surface_mesh: MeshInstance3D = $SurfaceMesh
@onready var surface_collision: CollisionShape3D = $SurfaceCollision


func _ready() -> void:
	add_to_group("Planet")
	_apply_radius()


func _apply_radius() -> void:
	var sphere_mesh := surface_mesh.mesh as SphereMesh
	if sphere_mesh:
		sphere_mesh.radius = radius
		sphere_mesh.height = radius * 2.0
	var sphere_shape := surface_collision.shape as SphereShape3D
	if sphere_shape:
		sphere_shape.radius = radius


## Radius of what characters stand on (UE: the blocking component's bounds; the larger
## overlap-only GravityWell sphere is ignored).
func get_surface_radius() -> float:
	var sphere_shape := surface_collision.shape as SphereShape3D
	return sphere_shape.radius * global_basis.get_scale().x if sphere_shape else radius
