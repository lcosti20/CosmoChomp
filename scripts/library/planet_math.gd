class_name PlanetMath
extends RefCounted
## Shared vector helpers for living on a spherical planet (port of BPFL_PlanetMath).
##
## Unreal's frame is X forward / Y right / Z up. Godot's is -Z forward / X right / Y up,
## so "Make Rot from XZ (Forward, Up)" becomes [method basis_from_forward_up] and
## UE's right vector Cross(Up, Forward) becomes forward.cross(up).

const NEARLY_ZERO := 0.0001


## Order-independent key for the edge between two tiles.
static func make_edge_key(a: int, b: int) -> int:
	return mini(a, b) * 10000 + maxi(a, b)


## Two unit tangents perpendicular to [param up] (UE: FindBestAxisVectors / TangentBasis).
static func tangent_basis(up: Vector3) -> Array[Vector3]:
	var reference := Vector3.UP if absf(up.y) < 0.9 else Vector3.RIGHT
	var tangent := reference.cross(up).normalized()
	var bitangent := up.cross(tangent)
	return [tangent, bitangent]


## Unit "up" at [param location]; world up if it sits on the planet center.
static func surface_up(location: Vector3, planet_center: Vector3) -> Vector3:
	var offset := location - planet_center
	if offset.is_zero_approx() or offset.length() < NEARLY_ZERO:
		return Vector3.UP
	return offset.normalized()


## Basis with Y along [param up] and -Z as close to [param forward] as possible.
static func surface_rotation(up: Vector3, forward: Vector3) -> Basis:
	var tangent := flatten_direction(forward, up)
	if tangent.length() < NEARLY_ZERO:
		tangent = flatten_direction(Vector3.FORWARD, up)
		if tangent.length() < NEARLY_ZERO:
			return basis_from_forward_up(Vector3.RIGHT, up)
	return basis_from_forward_up(tangent, up)


## Removes the part of [param direction] along [param up] and normalizes the rest
## (zero if nothing is left).
static func flatten_direction(direction: Vector3, up: Vector3) -> Vector3:
	return project_on_plane(direction, up).normalized()


## UE: Project Vector on to Plane.
static func project_on_plane(v: Vector3, normal: Vector3) -> Vector3:
	var n := normal.normalized()
	return v - n * v.dot(n)


## UE: Make Rot from XZ. Forward is kept exactly; up is adjusted to be perpendicular.
static func basis_from_forward_up(forward: Vector3, up: Vector3) -> Basis:
	var f := forward.normalized()
	if f.is_zero_approx():
		f = Vector3.FORWARD
	var u := up.normalized()
	var right := f.cross(u)
	if right.length() < NEARLY_ZERO:
		# Forward is parallel to up: pick any perpendicular "up"
		u = tangent_basis(f)[0]
		right = f.cross(u)
	right = right.normalized()
	u = right.cross(f).normalized()
	return Basis(right, u, -f)


## UE: Make Rot from Z. Any basis whose Y axis is [param up].
static func basis_from_up(up: Vector3) -> Basis:
	var u := up.normalized()
	return basis_from_forward_up(tangent_basis(u)[1], u)


## Combined AABB of every mesh under [param node], in [param node]'s local space.
static func local_mesh_aabb(node: Node3D) -> AABB:
	var result := AABB()
	var found := false
	for mesh_instance: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
		if mesh_instance.mesh == null:
			continue
		var to_local := node.global_transform.affine_inverse() * _rendered_mesh_transform(mesh_instance)
		var box := to_local * mesh_instance.mesh.get_aabb()
		result = box if not found else result.merge(box)
		found = true
	return result


## Where a mesh's vertices actually render. A skinned mesh ignores its own node transform:
## its vertices go through the skeleton's rest pose and the skin's bind pose instead (the
## alien's mesh node sits under a 0.33-scaled armature but renders at full size).
static func _rendered_mesh_transform(mesh_instance: MeshInstance3D) -> Transform3D:
	var skeleton := mesh_instance.get_node_or_null(mesh_instance.skeleton) as Skeleton3D
	var skin := mesh_instance.skin
	if skeleton == null or skin == null or skin.get_bind_count() == 0:
		return mesh_instance.global_transform
	var bone := skin.get_bind_bone(0)
	if bone < 0:
		bone = skeleton.find_bone(skin.get_bind_name(0))
	if bone < 0:
		return mesh_instance.global_transform
	return skeleton.global_transform * skeleton.get_bone_global_rest(bone) * skin.get_bind_pose(0)


## Scales [param model] so its largest dimension equals [param size]. If [param stand_on_base],
## its bottom-center sits at the parent's origin; otherwise its center does
## (UE: BPFL_PlanetMath.FitStaticMesh).
static func fit_static_mesh(model: Node3D, size: float, stand_on_base: bool) -> void:
	if model == null or not model.is_inside_tree():
		return
	model.scale = Vector3.ONE
	model.position = Vector3.ZERO
	var bounds := local_mesh_aabb(model)
	var fit_scale := size / maxf(maxf(bounds.size.x, bounds.size.y), maxf(bounds.size.z, NEARLY_ZERO))
	var pivot := bounds.get_center()
	if stand_on_base:
		pivot.y = bounds.position.y
	model.scale = Vector3.ONE * fit_scale
	model.position = model.quaternion * (-pivot * fit_scale)


## Scales a character model to [param height] and stands it on the bottom of a capsule
## whose half height is [param half_height], rotated by [param yaw_degrees]
## (UE: FitSkeletalMeshToCapsule, baked as the Mesh component transform in the Blueprints).
static func fit_model_to_capsule(model: Node3D, half_height: float, height: float, yaw_degrees: float) -> void:
	if model == null or not model.is_inside_tree():
		return
	model.transform = Transform3D.IDENTITY
	var bounds := local_mesh_aabb(model)
	var fit_scale := height / maxf(bounds.size.y, NEARLY_ZERO)
	model.basis = Basis(Vector3.UP, deg_to_rad(yaw_degrees)).scaled(Vector3.ONE * fit_scale)
	model.position = Vector3(0.0, -half_height - bounds.position.y * fit_scale, 0.0)


## Overrides the material of every mesh under [param node] (the ray gun's Collada file
## doesn't carry its textures through the import).
static func apply_material(node: Node, material: Material) -> void:
	for mesh_instance: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
		mesh_instance.material_override = material
