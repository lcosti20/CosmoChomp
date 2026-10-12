"""
Unreal Editor Python script that imports the staged Cosmo Chomp assets and builds their materials.
Run through build_assets.py (UnrealEditor-Cmd -run=pythonscript).

Safe to re-run: meshes, textures and sounds are re-imported in place and nothing is deleted or
renamed (the C++ classes reference these paths, which keeps them loaded). Existing materials are
kept as they are; delete one in the editor to have it rebuilt.

Resulting layout (the C++ classes load these paths as defaults):
  /Game/Meshes/Astronaut   SK_Astronaut, SK_Astronaut_Anim (idle), M_Astronaut, M_Astronaut_Visor
  /Game/Meshes/Alien       SK_Alien, SK_AlienArmature_<Take> animations, M_Alien
  /Game/Meshes/Rocket      SM_Rocket
  /Game/Meshes/RayGun      SM_RayGun, M_RayGun
  /Game/Meshes/Walls       SM_Wall_01 .. SM_Wall_06, M_Wall_01 .. M_Wall_06
  /Game/Materials          M_Glow (unlit emissive, "Color" + "Intensity" parameters)
  /Game/Audio              S_* sound waves
  /Game/Input/Actions      IA_Fire (mapped in IMC_Default)
"""
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ASSETS_DIR = os.path.join(PROJECT_DIR, "Assets")
STAGING_DIR = os.path.join(PROJECT_DIR, "Intermediate", "AssetStaging")
MODELS_DIR = os.path.join(ASSETS_DIR, "models")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"[CosmoChompImport] {msg}")


# ---------------------------------------------------------------------------
# Generic import helpers
# ---------------------------------------------------------------------------

def make_pipeline(static=False, skeletal=False, animations=False, materials=False, box_collision=False):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", True)

    common = pipeline.get_editor_property("common_meshes_properties")
    common.set_editor_property("recompute_normals", False)
    common.set_editor_property("force_all_mesh_as_type",
                               unreal.InterchangeForceMeshType.IFMT_STATIC_MESH if static else unreal.InterchangeForceMeshType.IFMT_NONE)

    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", static)
    mesh.set_editor_property("import_skeletal_meshes", skeletal)
    mesh.set_editor_property("build_nanite", False)
    mesh.set_editor_property("create_physics_asset", False)
    mesh.set_editor_property("import_morph_targets", False)
    if box_collision:
        # Walls are stretched along hex edges and need simple collision for characters and traces
        mesh.set_editor_property("collision", True)
        mesh.set_editor_property("force_collision_primitive_generation", True)
        mesh.set_editor_property("fallback_collision_type", unreal.InterchangeMeshCollision.BOX)

    anim = pipeline.get_editor_property("animation_pipeline")
    anim.set_editor_property("import_animations", animations)

    mat = pipeline.get_editor_property("material_pipeline")
    mat.set_editor_property("import_materials", materials)
    mat.get_editor_property("texture_pipeline").set_editor_property("import_textures", False)
    return pipeline


def interchange_import(filename, dest_path, pipeline):
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.get_editor_property("override_pipelines").append(unreal.SoftObjectPath(pipeline.get_path_name()))

    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(filename)
    if not manager.import_asset(dest_path, source, params):
        raise RuntimeError(f"Interchange import failed: {filename}")
    manager.wait_until_all_tasks_done(False)
    log(f"imported {os.path.basename(filename)} -> {dest_path}")


def task_import(filename, dest_path, dest_name):
    """Plain AssetImportTask, used for textures and sounds."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    if not paths:
        raise RuntimeError(f"Import failed: {filename}")
    return unreal.load_asset(paths[0])


def require(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Expected asset was not imported: {path}")
    return asset


# ---------------------------------------------------------------------------
# Textures and materials
# ---------------------------------------------------------------------------

def import_texture(filename, dest_path, name, kind="color"):
    tex = task_import(filename, dest_path, name)
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", False)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    return tex


SAMPLER = {
    "color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    "mask": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
}


def existing_material(dest_path, name):
    # Clearing an existing material's graph crashes in commandlets, so existing materials are reused as-is
    full = f"{dest_path}/{name}"
    return unreal.load_asset(full) if eal.does_asset_exist(full) else None


def new_material(dest_path, name):
    return asset_tools.create_asset(name, dest_path, unreal.Material, unreal.MaterialFactoryNew())


def texture_node(material, texture, kind, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", SAMPLER[kind])
    return node


def make_pbr_material(dest_path, name, base, normal=None, roughness=None, metallic=None, emissive=None):
    """Builds a simple textured material: each provided texture feeds its matching material input."""
    material = existing_material(dest_path, name)
    if material:
        return material
    material = new_material(dest_path, name)
    y = -300
    node = texture_node(material, base, "color", -400, y)
    mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal:
        y += 250
        node = texture_node(material, normal, "normal", -400, y)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if roughness:
        y += 250
        node = texture_node(material, roughness, "mask", -400, y)
        mel.connect_material_property(node, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if metallic:
        y += 250
        node = texture_node(material, metallic, "mask", -400, y)
        mel.connect_material_property(node, "R", unreal.MaterialProperty.MP_METALLIC)
    if emissive:
        y += 250
        node = texture_node(material, emissive, "color", -700, y)
        boost = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, y)
        boost.set_editor_property("const_b", 4.0)
        mel.connect_material_expressions(node, "RGB", boost, "A")
        mel.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    return material


def make_constant_material(dest_path, name, base_color, metallic, roughness):
    material = existing_material(dest_path, name)
    if material:
        return material
    material = new_material(dest_path, name)
    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 0)
    color.set_editor_property("constant", base_color)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, value, y in ((unreal.MaterialProperty.MP_METALLIC, metallic, 200),
                           (unreal.MaterialProperty.MP_ROUGHNESS, roughness, 400)):
        node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, y)
        node.set_editor_property("r", value)
        mel.connect_material_property(node, "", prop)
    mel.recompile_material(material)
    return material


def make_glow_material():
    """Unlit emissive material for energy orbs and ray beams; tinted per-actor via dynamic instances."""
    existing = existing_material("/Game/Materials", "M_Glow")
    if existing:
        set_usage(existing, instanced=True)
        return
    material = new_material("/Game/Materials", "M_Glow")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.1, 1.0, 0.6, 1.0))
    intensity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 250)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 8.0)
    mult = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(color, "", mult, "A")
    mel.connect_material_expressions(intensity, "", mult, "B")
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Ray gun beams are drawn as instanced cylinders
    set_usage(material, instanced=True)


def set_usage(material, skeletal=False, instanced=False):
    """Usage flags must be set ahead of time for cooked/game use, or the default material is used instead."""
    if skeletal:
        material.set_editor_property("used_with_skeletal_mesh", True)
    if instanced:
        material.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(material)
    return material


def assign_material(mesh, material, slot_filter=None):
    """Assigns material to every slot, or only slots whose name contains slot_filter."""
    if isinstance(mesh, unreal.SkeletalMesh):
        # Struct array elements come back as copies, so rebuild the array and write it back
        materials = []
        for slot in mesh.get_editor_property("materials"):
            slot = slot.copy()
            if not slot_filter or slot_filter in str(slot.get_editor_property("material_slot_name")).lower():
                slot.set_editor_property("material_interface", material)
            materials.append(slot)
        mesh.set_editor_property("materials", materials)
    else:
        for i in range(mesh.get_num_sections(0)):
            mesh.set_material(i, material)


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------

def import_astronaut():
    dest = "/Game/Meshes/Astronaut"
    interchange_import(os.path.join(STAGING_DIR, "SK_Astronaut.fbx"), dest,
                       make_pipeline(skeletal=True, animations=True))
    mesh = require(f"{dest}/SK_Astronaut")

    tex_dir = os.path.join(MODELS_DIR, "cute-astronaut", "textures")
    base = import_texture(os.path.join(tex_dir, "Atronaut_4096.png"), dest, "T_Astronaut_D")
    assign_material(mesh, set_usage(make_pbr_material(dest, "M_Astronaut", base), skeletal=True))

    # The visor slot ("visier") is untextured in the source: a dark, mirrored gold
    visor = make_constant_material(dest, "M_Astronaut_Visor", unreal.LinearColor(0.25, 0.2, 0.03, 1.0), 1.0, 0.12)
    set_usage(visor, skeletal=True)
    assign_material(mesh, visor, "visier")


def import_alien():
    dest = "/Game/Meshes/Alien"
    # FBX takes: Hello / Idle / Jump / Look up / Running attack / Talk / Walk -> SK_AlienArmature_<Take>
    interchange_import(os.path.join(STAGING_DIR, "SK_Alien.fbx"), dest,
                       make_pipeline(skeletal=True, animations=True))
    mesh = require(f"{dest}/SK_Alien")

    tex_dir = os.path.join(MODELS_DIR, "one-eyed-monster-with-animations", "textures")
    base = import_texture(os.path.join(tex_dir, "texture_pbr_20250901.png"), dest, "T_Alien_D")
    normal = import_texture(os.path.join(tex_dir, "texture_pbr_20250901_normal.png"), dest, "T_Alien_N", "normal")
    rough = import_texture(os.path.join(tex_dir, "texture_pbr_20250901_roughness.png"), dest, "T_Alien_R", "mask")
    metal = import_texture(os.path.join(tex_dir, "texture_pbr_20250901_metallic.png"), dest, "T_Alien_M", "mask")
    assign_material(mesh, set_usage(make_pbr_material(dest, "M_Alien", base, normal, rough, metal), skeletal=True))


def import_rocket():
    dest = "/Game/Meshes/Rocket"
    # The rocket has no textures; keep the flat colors from its OBJ material library
    interchange_import(os.path.join(STAGING_DIR, "SM_Rocket.fbx"), dest, make_pipeline(static=True, materials=True))
    require(f"{dest}/SM_Rocket")


def import_raygun():
    dest = "/Game/Meshes/RayGun"
    interchange_import(os.path.join(STAGING_DIR, "SM_RayGun.fbx"), dest, make_pipeline(static=True))
    mesh = require(f"{dest}/SM_RayGun")

    tex_dir = os.path.join(MODELS_DIR, "simple-ray-gun", "textures")
    base = import_texture(os.path.join(tex_dir, "lambert1_albedo.jpeg"), dest, "T_RayGun_D")
    normal = import_texture(os.path.join(tex_dir, "lambert1_normal.png"), dest, "T_RayGun_N", "normal")
    rough = import_texture(os.path.join(tex_dir, "lambert1_roughness.jpeg"), dest, "T_RayGun_R", "mask")
    metal = import_texture(os.path.join(tex_dir, "lambert1_metallic.jpeg"), dest, "T_RayGun_M", "mask")
    emissive = import_texture(os.path.join(tex_dir, "lambert1_emissive.jpeg"), dest, "T_RayGun_E")
    assign_material(mesh, make_pbr_material(dest, "M_RayGun", base, normal, rough, metal, emissive))


# Wall piece -> (base color, normal, roughness) texture files
WALL_TEXTURES = {
    "01": ("01_(1).png", "01_(2).png", "01_(3).png"),
    "02": ("02.png", "02.1.png", "02.2.png"),
    "03": ("03_(1).png", "03_(2).png", "03_(3).png"),
    "04": ("04.png", "04.1.png", "04.2.png"),
    "05": ("05_(1).png", "05_(2).png", "05_(3).png"),
    "06": ("06_(1).png", "06_(2).png", "06_(3).png"),
}


def import_walls():
    dest = "/Game/Meshes/Walls"
    tex_dir = os.path.join(MODELS_DIR, "wall-ready-for-construction", "textures")
    for piece, (base_file, normal_file, rough_file) in WALL_TEXTURES.items():
        name = f"SM_Wall_{piece}"
        interchange_import(os.path.join(STAGING_DIR, name + ".fbx"), dest, make_pipeline(static=True, box_collision=True))
        mesh = require(f"{dest}/{name}")

        base = import_texture(os.path.join(tex_dir, base_file), dest, f"T_Wall_{piece}_D")
        normal = import_texture(os.path.join(tex_dir, normal_file), dest, f"T_Wall_{piece}_N", "normal")
        rough = import_texture(os.path.join(tex_dir, rough_file), dest, f"T_Wall_{piece}_R", "mask")
        # The maze draws walls as instanced static meshes
        assign_material(mesh, set_usage(make_pbr_material(dest, f"M_Wall_{piece}", base, normal, rough), instanced=True))


# ---------------------------------------------------------------------------
# Audio and input
# ---------------------------------------------------------------------------

def import_sounds():
    dest = "/Game/Audio"
    sound_dir = os.path.join(STAGING_DIR, "sounds")
    for wav in sorted(os.listdir(sound_dir)):
        sound = task_import(os.path.join(sound_dir, wav), dest, os.path.splitext(wav)[0])
        if sound.get_name() == "S_Footsteps":
            sound.set_editor_property("looping", True)
        log(f"sound {sound.get_name()} ({sound.get_editor_property('duration'):.2f}s)")


def create_fire_input():
    dest = "/Game/Input/Actions"
    path = f"{dest}/IA_Fire"
    if eal.does_asset_exist(path):
        action = unreal.load_asset(path)
    else:
        action = asset_tools.create_asset("IA_Fire", dest, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)

    imc = unreal.load_asset("/Game/Input/IMC_Default")
    imc.unmap_all_keys_from_action(action)
    for key_name in ("LeftMouseButton", "Gamepad_RightTrigger", "F"):
        key = unreal.Key()
        key.import_text(f'(KeyName="{key_name}")')
        imc.map_key(action, key)
    # MapKey doesn't mark the package dirty, so force the saves
    eal.save_loaded_asset(action, False)
    eal.save_loaded_asset(imc, False)
    mapped = [str(m.get_editor_property("key").get_editor_property("key_name")) for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
              if m.get_editor_property("action") == action]
    log(f"IA_Fire mapped to {mapped}")


def main():
    for folder in ("/Game/Meshes/Walls", "/Game/Meshes/Rocket", "/Game/Meshes/RayGun",
                   "/Game/Meshes/Astronaut", "/Game/Meshes/Alien", "/Game/Materials", "/Game/Audio"):
        eal.make_directory(folder)

    import_walls()
    import_rocket()
    import_raygun()
    import_astronaut()
    import_alien()
    make_glow_material()
    import_sounds()
    create_fire_input()
    for folder in ("/Game/Meshes", "/Game/Materials", "/Game/Audio"):
        eal.save_directory(folder, only_if_is_dirty=False, recursive=True)
    log("done")


main()
