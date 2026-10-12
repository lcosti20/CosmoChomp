"""
Creates /Game/Maps/Lvl_CosmoChomp: the planet, lighting, a PlayerStart and the HexMaze actor.
Re-running it requires deleting the level first. Everything else (maze walls, rocket, orbs, aliens, ray gun) is spawned by BP_CosmoGameMode at runtime.

Needs the full editor (level editing subsystems aren't available in commandlets):
    UnrealEditor.exe CosmoChomp.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import unreal

LEVEL_PATH = "/Game/Maps/Lvl_CosmoChomp"
PLANET_RADIUS = 3000.0
SKY_RADIUS = 200000.0

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(cls, location=(0, 0, 0), rotation=(0, 0, 0), label=None):
    actor = actor_subsystem.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(*rotation))
    # Spawning can land at the editor's default drop point; pin the transform explicitly
    actor.set_actor_location_and_rotation(unreal.Vector(*location), unreal.Rotator(*rotation), False, True)
    if label:
        actor.set_actor_label(label)
    return actor


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        raise RuntimeError(f"{LEVEL_PATH} already exists; delete it first to regenerate it")

    folder, name = LEVEL_PATH.rsplit("/", 1)
    unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.World, unreal.WorldFactory())
    unreal.EditorAssetLibrary.save_asset(LEVEL_PATH)
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)

    # Never edit whatever other map happens to be open (e.g. the editor startup map)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_outermost().get_name() != LEVEL_PATH:
        raise RuntimeError(f"Expected {LEVEL_PATH} to be open, got {world.get_outermost().get_name() if world else None}")

    # Planet: BP_Planet (tagged "Planet") scaled to the desired radius
    planet_class = unreal.load_asset("/Game/BP_Planet").generated_class()
    planet = spawn(planet_class, label="Planet")
    # Size by the sphere mesh; the actor bounds also include BP_Planet's larger overlap-only GravityWell
    sphere = planet.get_component_by_class(unreal.StaticMeshComponent)
    radius = sphere.static_mesh.get_bounds().box_extent.x * sphere.get_world_scale().x
    scale = PLANET_RADIUS / max(radius, 1.0)
    planet.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    unreal.log(f"[CosmoChompLevel] planet radius {radius:.0f} -> {PLANET_RADIUS:.0f} (scale {scale:.3f})")

    # The game mode builds the maze around the planet at runtime
    maze_class = unreal.load_asset("/Game/Blueprints/Maze/BP_HexMaze").generated_class()
    maze = spawn(maze_class, label="HexMaze")

    # The player lands on the tile nearest to this
    spawn(unreal.PlayerStart, (0, 0, PLANET_RADIUS + 200), label="PlayerStart")

    # Lighting: a sun, plus ambient from a fixed cubemap so the night side of the planet stays
    # readable (capturing the black space sky would leave it pitch dark)
    sun = spawn(unreal.DirectionalLight, (0, 0, PLANET_RADIUS * 2), (0, -50, 30), label="Sun")
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_component.set_editor_property("intensity", 8.0)

    sky_light = spawn(unreal.SkyLight, (0, 0, PLANET_RADIUS * 2), label="SkyLight")
    sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky_component.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
    sky_component.set_editor_property("intensity", 1.0)

    # Space: a huge sphere with a black, starry, unlit material (no atmosphere)
    sky_sphere = spawn(unreal.StaticMeshActor, label="SpaceSky")
    sky_mesh = sky_sphere.static_mesh_component
    sky_mesh.set_static_mesh(unreal.load_asset("/Engine/EngineSky/SM_SkySphere"))
    sky_mesh.set_material(0, unreal.load_asset("/Game/Materials/M_SpaceSky"))
    sky_mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sky_mesh.set_editor_property("cast_shadow", False)
    sky_radius = sky_mesh.static_mesh.get_bounds().sphere_radius
    sky_sphere.set_actor_scale3d(unreal.Vector(1, 1, 1) * (SKY_RADIUS / max(sky_radius, 1.0)))

    post = spawn(unreal.PostProcessVolume, label="PostProcess")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    settings.set_editor_property("override_bloom_intensity", True)
    settings.set_editor_property("bloom_intensity", 1.2)
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 1.0)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 2.0)
    post.set_editor_property("settings", settings)

    level_subsystem.save_current_level()
    for actor in actor_subsystem.get_all_level_actors():
        unreal.log(f"[CosmoChompLevel] {actor.get_actor_label()} at {actor.get_actor_location()}")
    unreal.log("[CosmoChompLevel] saved " + LEVEL_PATH)


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
