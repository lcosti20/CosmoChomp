class_name RayGunBeam
extends Node3D
## A ray gun shot drawn along the curve of the planet (port of BP_RayGunBeam).
##
## Each pair of consecutive points becomes a thin glowing cylinder; the beam thins
## out and disappears over lifetime seconds.

## UE: 16 cm
@export var beam_width: float = 0.16
@export var lifetime: float = 0.3
## Peak brightness of the muzzle and impact flashes (Godot light energy)
@export var flash_energy: float = 1.5

@onready var muzzle_flash: OmniLight3D = $MuzzleFlash
@onready var impact_flash: OmniLight3D = $ImpactFlash

var age: float = 0.0
var _segments: Array[MeshInstance3D] = []
var _cylinder: CylinderMesh = null


func _ready() -> void:
	# Engine cylinder: 1 m tall along Y, 1 m across, pivot at its center
	_cylinder = CylinderMesh.new()
	_cylinder.top_radius = 0.5
	_cylinder.bottom_radius = 0.5
	_cylinder.height = 1.0
	_cylinder.radial_segments = 8
	_cylinder.rings = 1


func set_points(points: PackedVector3Array, color: Color) -> void:
	var material := GlowMaterial.make(color, 25.0)

	if points.size() > 0:
		muzzle_flash.global_position = points[0]
		impact_flash.global_position = points[points.size() - 1]
	for flash: OmniLight3D in [muzzle_flash, impact_flash]:
		flash.light_color = color
		flash.light_energy = flash_energy

	for index: int in range(1, points.size()):
		var start := points[index - 1]
		var end := points[index]
		var length := start.distance_to(end)
		if length < 0.0001:
			continue
		var segment := MeshInstance3D.new()
		segment.mesh = _cylinder
		segment.material_override = material
		segment.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(segment)
		var axis_y := (end - start) / length
		var segment_basis := PlanetMath.basis_from_up(axis_y)
		segment.global_transform = Transform3D(segment_basis.scaled_local(Vector3(beam_width, length, beam_width)), (start + end) * 0.5)
		_segments.append(segment)


func _process(delta: float) -> void:
	# Thin out, then disappear
	age += delta
	var alpha := 1.0 - age / lifetime
	if alpha <= 0.0:
		queue_free()
		return
	muzzle_flash.light_energy = flash_energy * alpha
	impact_flash.light_energy = flash_energy * alpha
	for segment: MeshInstance3D in _segments:
		var length := segment.basis.y.length()
		var segment_basis := segment.basis.orthonormalized()
		segment.basis = segment_basis.scaled_local(Vector3(beam_width * alpha, length, beam_width * alpha))
