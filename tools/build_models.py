# Converts the raw source models in assets/models into Godot-ready GLB files in
# assets/models/glb. Godot port of the UE project's Tools/AssetPipeline step:
# links the textures, cuts the astronaut's single take into Idle/Walk clips and
# splits the wall model into its six pieces with bottom-centered pivots.
#
# Run (Blender 5.x):
#   blender -b --factory-startup --python tools/build_models.py
#
# The ray gun ships as Collada (model.dae); Godot imports that natively, so it
# is not converted here.

import os
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = os.path.join(ROOT, "assets", "models")
OUT = os.path.join(MODELS, "glb")


def reset():
	bpy.ops.wm.read_factory_settings(use_empty=True)


def image(path):
	return bpy.data.images.load(path, check_existing=True)


def principled(mat):
	mat.use_nodes = True
	return next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


def link_tex(mat, path, socket, non_color=False):
	nt = mat.node_tree
	bsdf = principled(mat)
	tex = nt.nodes.new("ShaderNodeTexImage")
	tex.image = image(path)
	if non_color:
		tex.image.colorspace_settings.name = "Non-Color"
	if socket == "Normal":
		nm = nt.nodes.new("ShaderNodeNormalMap")
		nt.links.new(tex.outputs["Color"], nm.inputs["Color"])
		nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
	else:
		nt.links.new(tex.outputs["Color"], bsdf.inputs[socket])


def export(name, **kw):
	os.makedirs(OUT, exist_ok=True)
	bpy.ops.export_scene.gltf(
		filepath=os.path.join(OUT, name + ".glb"),
		export_format="GLB",
		export_yup=True,
		export_apply=False,
		**kw)


def outward_facing(obj):
	# True if most face normals point away from the mesh center (the mesh isn't inside out)
	center = sum((v.co for v in obj.data.vertices), Vector()) / max(len(obj.data.vertices), 1)
	return sum(p.normal.dot(p.center - center) * p.area for p in obj.data.polygons) > 0


def astronaut():
	reset()
	base = os.path.join(MODELS, "cute-astronaut")
	bpy.ops.import_scene.fbx(filepath=os.path.join(base, "source", "Astronaut.fbx"))
	for mat in bpy.data.materials:
		if mat.name == "Astronaut":
			link_tex(mat, os.path.join(base, "textures", "Atronaut_4096.png"), "Base Color")
		elif mat.name == "visier":
			# The visor slot is untextured in the source: a dark, mirrored gold
			bsdf = principled(mat)
			bsdf.inputs["Base Color"].default_value = (0.25, 0.2, 0.03, 1.0)
			bsdf.inputs["Metallic"].default_value = 1.0
			bsdf.inputs["Roughness"].default_value = 0.12
	# The source mesh has a negative-determinant scale; bake it into the vertices
	mesh = next(o for o in bpy.context.scene.objects if o.type == "MESH")
	bpy.ops.object.select_all(action="DESELECT")
	mesh.select_set(True)
	bpy.context.view_layer.objects.active = mesh
	bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
	if not outward_facing(mesh):
		mesh.data.flip_normals()
	arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
	take = arm.animation_data.action
	arm.animation_data.action = None
	# The source has one take; frames 1-41 are the idle loop, 61-121 the walk.
	for clip, (start, end) in {"Idle": (1, 41), "Walk": (61, 121)}.items():
		track = arm.animation_data.nla_tracks.new()
		track.name = clip
		strip = track.strips.new(clip, int(start), take)
		strip.action_frame_start = start
		strip.action_frame_end = end
	export("astronaut", export_animation_mode="NLA_TRACKS")


def alien():
	reset()
	base = os.path.join(MODELS, "one-eyed-monster-with-animations")
	bpy.ops.import_scene.fbx(filepath=os.path.join(base, "source", "Claymation monster with animations.fbx"))
	tex = os.path.join(base, "textures")
	for mat in bpy.data.materials:
		link_tex(mat, os.path.join(tex, "texture_pbr_20250901.png"), "Base Color")
		link_tex(mat, os.path.join(tex, "texture_pbr_20250901_metallic.png"), "Metallic", True)
		link_tex(mat, os.path.join(tex, "texture_pbr_20250901_roughness.png"), "Roughness", True)
		link_tex(mat, os.path.join(tex, "texture_pbr_20250901_normal.png"), "Normal", True)
	# Drop the unnamed duplicate takes the FBX importer creates.
	for act in list(bpy.data.actions):
		if "|" not in act.name:
			bpy.data.actions.remove(act)
	export("alien", export_animation_mode="ACTIONS")


def bottom_center(obj):
	# Move the object's origin to the bottom center of its bounds and put it at the world origin.
	bpy.context.view_layer.update()
	pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
	mn = Vector([min(p[i] for p in pts) for i in range(3)])
	mx = Vector([max(p[i] for p in pts) for i in range(3)])
	pivot = Vector(((mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5, mn.z))
	bpy.ops.object.select_all(action="DESELECT")
	obj.select_set(True)
	bpy.context.view_layer.objects.active = obj
	bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
	obj.data.transform(Matrix.Translation(-pivot))
	obj.location = (0.0, 0.0, 0.0)


def rocket():
	reset()
	base = os.path.join(MODELS, "rocket")
	bpy.ops.wm.obj_import(filepath=os.path.join(base, "source", "rocket.obj"))
	for obj in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
		bottom_center(obj)
	export("rocket")


def walls():
	base = os.path.join(MODELS, "wall-ready-for-construction")
	reset()
	bpy.ops.import_scene.fbx(filepath=os.path.join(base, "source", "wall  ready for construction.fbx"))
	tex = os.path.join(base, "textures")
	# Wall piece -> (base color, normal, roughness)
	piece_tex = {
		"01": ("01_(1).png", "01_(2).png", "01_(3).png"),
		"02": ("02.png", "02.1.png", "02.2.png"),
		"03": ("03_(1).png", "03_(2).png", "03_(3).png"),
		"04": ("04.png", "04.1.png", "04.2.png"),
		"05": ("05_(1).png", "05_(2).png", "05_(3).png"),
		"06": ("06_(1).png", "06_(2).png", "06_(3).png"),
	}
	pieces = [o for o in bpy.context.scene.objects if o.type == "MESH"]
	for obj in pieces:
		# Keep the parent's rotation and 0.01 scale when unparenting
		world = obj.matrix_world.copy()
		obj.parent = None
		obj.matrix_world = world
	for obj in pieces:
		for mat in obj.data.materials:
			files = piece_tex.get(obj.name[:2])
			if files:
				link_tex(mat, os.path.join(tex, files[0]), "Base Color")
				link_tex(mat, os.path.join(tex, files[1]), "Normal", True)
				link_tex(mat, os.path.join(tex, files[2]), "Roughness", True)
	for obj in [o for o in bpy.context.scene.objects if o.type != "MESH"]:
		bpy.data.objects.remove(obj)
	for obj in pieces:
		bottom_center(obj)
		obj.name = "Wall_" + obj.name[:2]
	export("walls")


astronaut()
alien()
rocket()
walls()
print("BUILD_MODELS_DONE")
