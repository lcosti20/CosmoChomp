"""
Blender (5.x) pre-processing for Cosmo Chomp source models.

Run headless (build_assets.py does this for you):
    blender -b --python blender_prep.py -- <ProjectDir> <StagingDir>

- Splits "wall ready for construction.fbx" into one FBX per wall piece, with each
  piece's pivot at its bottom-center so the maze generator can stretch it along a
  hex edge.
- Converts the rocket (OBJ) and ray gun (glTF, converted from Collada) into FBX with the pivot at the
  bottom-center (rocket) or center (ray gun) and the longest axis of the gun on +X.
"""
import os
import sys
import zipfile
import tempfile

import bpy
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
PROJECT_DIR, STAGING_DIR = argv[0], argv[1]
MODELS = os.path.join(PROJECT_DIR, "Assets", "models")
os.makedirs(STAGING_DIR, exist_ok=True)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def mesh_objects():
    return [o for o in bpy.data.objects if o.type == "MESH"]


def world_bounds(objs):
    pts = [o.matrix_world @ mathutils.Vector(c) for o in objs for c in o.bound_box]
    mn = mathutils.Vector([min(p[i] for p in pts) for i in range(3)])
    mx = mathutils.Vector([max(p[i] for p in pts) for i in range(3)])
    return mn, mx


def join_and_apply(objs, name):
    """Joins objs into a single mesh with all transforms applied and no parent."""
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    if len(objs) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = name
    return obj


def recenter(obj, pivot="bottom"):
    """Moves mesh data so the pivot sits at the bottom-center (or center) of its bounds."""
    mn, mx = world_bounds([obj])
    center = (mn + mx) * 0.5
    offset = mathutils.Vector((center.x, center.y, mn.z if pivot == "bottom" else center.z))
    obj.data.transform(mathutils.Matrix.Translation(-offset))
    obj.location = (0, 0, 0)


def export_fbx(obj, filename):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(STAGING_DIR, filename)
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        bake_space_transform=True,
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        bake_anim=False,
    )
    mn, mx = world_bounds([obj])
    print(f"[prep] wrote {filename} size={tuple(round(v, 3) for v in (mx - mn))}")


def prep_walls():
    reset_scene()
    src = os.path.join(MODELS, "wall-ready-for-construction", "source", "wall  ready for construction.fbx")
    bpy.ops.import_scene.fbx(filepath=src)
    for obj in list(mesh_objects()):
        piece = join_and_apply([obj], f"SM_Wall_{obj.name}")
        recenter(piece, "bottom")
        export_fbx(piece, f"{piece.name}.fbx")
        bpy.data.objects.remove(piece)


def prep_rocket():
    reset_scene()
    with tempfile.TemporaryDirectory() as tmp:
        with zipfile.ZipFile(os.path.join(MODELS, "rocket", "source", "rocket.zip")) as z:
            z.extractall(tmp)
        bpy.ops.wm.obj_import(filepath=os.path.join(tmp, "rocket.obj"))
        rocket = join_and_apply(mesh_objects(), "SM_Rocket")
        recenter(rocket, "bottom")
        export_fbx(rocket, "SM_Rocket.fbx")


def prep_raygun():
    # Blender 5 dropped Collada import, so build_assets.py converts model.dae -> raygun.glb first
    reset_scene()
    bpy.ops.import_scene.gltf(filepath=os.path.join(STAGING_DIR, "raygun.glb"))
    gun = join_and_apply(mesh_objects(), "SM_RayGun")
    # Put the barrel (longest horizontal axis) on X so code can treat +X as "forward"
    mn, mx = world_bounds([gun])
    size = mx - mn
    if size.y > size.x:
        gun.data.transform(mathutils.Matrix.Rotation(-1.5707963, 4, "Z"))
    # The source model's muzzle points down -X; flip it so the muzzle is +X
    gun.data.transform(mathutils.Matrix.Rotation(3.1415927, 4, "Z"))
    recenter(gun, "center")
    export_fbx(gun, "SM_RayGun.fbx")


prep_walls()
prep_rocket()
prep_raygun()
print("[prep] done")
