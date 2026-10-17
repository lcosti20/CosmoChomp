# Cosmo Chomp (Godot 4)

Godot 4.7 port of the Unreal Engine 5 Blueprint project in `../CosmoChomp_UE`. Collect energy orbs on a hex-maze planet, bring them to your rocket, dodge the aliens and escape. This project is self-contained: every model, texture and sound it uses lives under `assets/`.

## Running

Open `project.godot` in Godot 4.7 and press Play. The game starts on the main menu (`scenes/levels/main_menu.tscn`); Start loads the round (`scenes/levels/cosmo_chomp.tscn`), which you can also run directly with F6.

Controls match the original: WASD / arrows / left stick to move, mouse / right stick to look, Space / gamepad A to jump, left mouse / F / right trigger to fire while holding the ray gun, Escape / P / Start to pause.

## Layout

```text
CosmoChomp_GE/
├── assets/
│   ├── models/          Source models (FBX/OBJ/DAE + textures) and models/glb/, the converted GLBs the game uses
│   ├── sounds/          MP3s (imported directly; music loops)
│   └── materials/       Glow shader (M_Glow), space sky (M_SpaceSky), environment, ray gun material
├── scenes/
│   ├── levels/          main_menu.tscn (Lvl_MainMenu), cosmo_chomp.tscn (Lvl_CosmoChomp)
│   ├── characters/      astronaut.tscn, alien.tscn
│   ├── gameplay/        rocket, energy orb, ray gun pickup, ray gun beam
│   ├── world/           planet.tscn
│   └── ui/              main_menu.tscn, pause_menu.tscn
├── scripts/
│   ├── core/            Game mode, player controller, game instance (autoload), astronaut, gravity, shared movement
│   ├── gameplay/        alien, rocket, energy orb, ray gun pickup and beam
│   ├── maze/            hex_maze.gd (hex grid on the sphere + maze generation and queries)
│   ├── ui/              HUD with mini map, main menu, pause menu
│   ├── world/           planet.gd
│   └── library/         PlanetMath (BPFL_PlanetMath), physics layers, glow material, sound helper
└── tools/               Asset conversion and test scripts (see below)
```

## Blueprint → GDScript map

| Unreal | Godot |
|---|---|
| `BP_CosmoGameMode` | `scripts/core/cosmo_game_mode.gd` (node in the level, group `game_mode`) |
| `BP_MenuGameMode` | `scripts/core/menu_game_mode.gd` |
| `BP_CosmoPlayerController`, `WBP_PauseMenu` | `scripts/core/cosmo_player_controller.gd`, `scripts/ui/pause_menu.gd` |
| `BP_CosmoGameInstance` | `scripts/core/cosmo_game_instance.gd` (autoload `GameInstance`, owns the music) |
| `BP_Astronaut` (+ `BPI_CosmoPlayer`) | `scripts/core/astronaut.gd`; the interface functions are `player_*` methods called duck-typed |
| `BPC_PlanetaryGravity` | `scripts/core/planetary_gravity.gd` (child node of each character) |
| CharacterMovementComponent | `scripts/core/planet_character.gd` (CharacterBody3D base: UE walking/braking math, gravity toward the planet) |
| `BP_Alien` | `scripts/gameplay/alien.gd` |
| `BP_Rocket`, `BP_EnergyOrb`, `BP_RayGunPickup`, `BP_RayGunBeam` | `scripts/gameplay/*.gd` |
| `BP_HexMaze` (+ `BPI_Maze`) | `scripts/maze/hex_maze.gd` |
| `BP_CosmoHUD` | `scripts/ui/cosmo_hud.gd` (canvas drawing, same mini map projection) |
| `WBP_MainMenu` | `scripts/ui/main_menu.gd` |
| `BPFL_PlanetMath` | `scripts/library/planet_math.gd` |
| Event dispatchers (`OnKilled`, `OnLaunched`, `OnCollected`, ...) | signals (`killed`, `launched`, `collected`, ...) |
| Actor tags / Get All Actors | node groups (`Planet`, `maze`, `player`, `aliens`, `energy_orbs`, ...) |

## Conventions and deliberate differences

- **Units:** meters. Every UE distance, speed and acceleration was divided by 100 (planet radius 3000 cm → 30 m, walk speed 500 cm/s → 5 m/s).
- **Axes:** UE is X forward / Y right / Z up; Godot is −Z forward / X right / Y up. `PlanetMath.basis_from_forward_up` replaces *Make Rot from XZ*, and UE's right vector `Cross(Up, Forward)` becomes `forward.cross(up)`. Look yaw is negated for the same reason, so positive yaw still turns right.
- **Planet surface math** (surface frame carried by re-flattening the heading, control rotation built from yaw/pitch around the frame, great-circle ray gun trace, azimuthal equidistant mini map) is the same math as the Blueprints.
- **Collision channels** become physics layers (`scripts/library/layers.gd`): the RayGun trace channel is the `RAY_GUN_MASK`, which stops on walls, the planet, the rocket and aliens and passes through pickups and trigger zones.
- **Walls** are MultiMesh instances with box colliders (UE: instanced static meshes).
- **Sounds** play unspatialized at full volume, like the UE sounds (they had no attenuation).
- **Death topple:** `BP_Astronaut.Die`'s Make Rotator has its pins crossed, which turns the mesh sideways instead of rolling it; the port tips the astronaut over as the docs and C++ describe.
- **Stick pitch:** `IMC_Default` doesn't negate the right stick's Y (only mouse Y is negated), so pushing the stick up looks down in the original. Kept for parity; set `invert_stick_pitch` on the Astronaut to false to change it.
- **Floor snapping:** Godot's CharacterBody3D steps off a curved floor when walking straight; `planet_character.gd` snaps back to the floor each frame, which UE's walking mode did implicitly.

## Assets

`tools/build_models.py` (Blender 5.x) converts the source models into `assets/models/glb/`, doing what the UE `Tools/AssetPipeline` did: links textures, fixes the astronaut's mirrored mesh and cuts its single take into `Idle` (frames 1–41) and `Walk` (61–121), splits the wall model into six pieces with bottom-centered pivots, and centers the rocket on its base. The ray gun's Collada file and all MP3s are imported by Godot directly.

```bash
blender -b --factory-startup --python tools/build_models.py
```

## Testing

```bash
# Rules: maze, orbs, deposit, damage, aliens, ray gun, death, restart, rocket launch
godot --headless --path . --script tools/smoke_test.gd
# Screenshots of a running level (optionally holding forward / firing)
godot --path . --script tools/capture_level.gd -- res://scenes/levels/cosmo_chomp.tscn <out_dir> 1 3 walk fire
```

## Credits

Larry Costigan — Game Designer & Developer (GAME 266 - Core Principles: Game Tech, Prof. James Taylor). Third-party credits are on the in-game Credits screen and in each asset folder's `CREDITS.txt`: models are CC BY 4.0, sounds and music are from Pixabay (the music's license certificate is in `assets/sounds/music/`).
