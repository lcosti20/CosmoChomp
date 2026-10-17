# Cosmo Chomp

A Godot 4.7 sci-fi maze game about collecting energy orbs, returning them to your rocket, dodging aliens, and escaping from a hostile hex-maze planet.

## Running

Open `project.godot` in Godot 4.7 and press Play. The game starts on the main menu (`scenes/levels/main_menu.tscn`); Start loads the round (`scenes/levels/cosmo_chomp.tscn`), which you can also run directly if you want to jump straight into gameplay.

Controls: WASD / arrows / left stick to move, mouse / right stick to look, Space / gamepad A to jump, left mouse / F / right trigger to fire while holding the ray gun, Escape / P / Start to pause. Controller and keyboard inputs follow the same flow.

On phones and tablets (Android, or a mobile browser) on-screen controls appear: touch anywhere on the left half for a move stick, drag on the right half to look, and tap JUMP, FIRE (shown while the ray gun is equipped), and PAUSE.

## Layout

```text
CosmoChomp/
├── assets/
│   ├── models/          Source models (FBX/OBJ/DAE + textures) and models/glb/, the converted GLBs the game uses
│   ├── sounds/          MP3s (imported directly; music loops)
│   └── materials/       Glow shader, space sky, environment, ray gun material
├── scenes/
│   ├── levels/          main_menu.tscn, cosmo_chomp.tscn
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
│   └── library/         PlanetMath, physics layers, glow material, sound helper
├── tools/               Asset conversion and test scripts
└── project.godot        Godot project definition
```

## Design-to-code map

| Concept | Godot file |
|---|---|
| Game mode | `scripts/core/cosmo_game_mode.gd` (node in the level, group `game_mode`) |
| Menu game mode | `scripts/core/menu_game_mode.gd` |
| Player controller / pause menu UI | `scripts/core/cosmo_player_controller.gd`, `scripts/ui/pause_menu.gd` |
| Game instance (global) | `scripts/core/cosmo_game_instance.gd` (autoload `GameInstance`, owns the music) |
| Player / astronaut | `scripts/core/astronaut.gd` (interface methods: `player_*`) |
| Planetary gravity | `scripts/core/planetary_gravity.gd` (child node of each character) |
| Character movement | `scripts/core/planet_character.gd` (CharacterBody3D base: walking/braking math, gravity toward the planet) |
| Alien | `scripts/gameplay/alien.gd` |
| Rocket, energy orb, ray gun pickup/beam | `scripts/gameplay/*.gd` |
| Hex maze / maze interface | `scripts/maze/hex_maze.gd` |
| HUD / mini-map | `scripts/ui/cosmo_hud.gd` (canvas drawing, azimuthal equidistant projection) |
| Main menu UI | `scripts/ui/main_menu.gd` |
| Planet math utilities | `scripts/library/planet_math.gd` |
| Events / messaging | signals (`killed`, `launched`, `collected`, ...) |
| Groups / actor queries | node groups (`Planet`, `maze`, `player`, `aliens`, `energy_orbs`, ...) |

## Conventions and implementation notes

- Units are meters. Original distances, speeds and accelerations were converted to Godot units (e.g., planet radius 3000 cm → 30 m, walk speed 500 cm/s → 5 m/s).
- Coordinate conventions follow Godot: -Z is forward, +X is right, and +Y is up. Surface framing, heading, and control rotation are built around the planet-local frame rather than a flat world basis.
- Planet surface math is handled by re-flattening the heading, constructing control rotation from yaw/pitch around the local frame, tracing the ray gun across a great circle, and projecting the minimap in azimuthal equidistant space.
- Collision behavior is implemented using physics layers (`scripts/library/layers.gd`): the ray gun trace mask stops on walls, the planet, the rocket and aliens, and passes through the player while aiming.
- Walls are MultiMesh instances with box colliders to keep the spherical maze performant while preserving gameplay feel.
- Sounds are intentionally played unspatialized at full volume to match the original game's audio presentation.
- The astronaut tips over when killed to match the intended movement and death behavior.
- The right stick pitch behavior is preserved for parity; `invert_stick_pitch` is available on the astronaut to invert it if desired.
- Godot's CharacterBody3D can step off a curved floor when walking straight; the player controller snaps back to the floor each frame to keep movement stable on the planet surface.

## Assets

`tools/build_models.py` (Blender 5.x) converts source models into `assets/models/glb/`: it links textures, fixes mirrored geometry, scales exports, and writes GLBs used by the game.

```bash
blender -b --factory-startup --python tools/build_models.py
# or just some of them: ... --python tools/build_models.py -- astronaut rocket
```

## Testing

```bash
# Rules: maze, orbs, deposit, damage, aliens, ray gun, death, restart, rocket launch
godot --headless --path . --script tools/smoke_test.gd
# Screenshots of a running level (optionally holding forward / firing)
godot --path . --script tools/capture_level.gd -- res://scenes/levels/cosmo_chomp.tscn <out_dir> 1 3 walk fire
# Touch controls: stick, look, jump, fire, pause
godot --path . --script tools/touch_test.gd
```

## Exporting

`export_presets.cfg` has presets for Windows, Linux, Web and Android; builds go to `build/` (not committed). They export everything except `tools/` and the raw source models, which the game only uses during development.

One-time setup:
- Export templates: Editor → Manage Export Templates → Download and Install (4.7.2).
- Android: in Editor Settings → Export → Android, set the Java SDK path (JDK 17) and the Android SDK path. The SDK needs platform-tools and build-tools. Godot creates a debug keystore on the first run.

```bash
godot --headless --path . --export-release "Windows Desktop"
godot --headless --path . --export-release "Linux"
godot --headless --path . --export-release "Web"
godot --headless --path . --export-debug "Android"     # signed with the debug keystore
adb install -r build/android/CosmoChomp.apk
```

Notes:
- Web builds are single-threaded; serve `build/web/` over HTTP (itch.io, GitHub Pages, or `python -m http.server`).
- Android builds target arm64 phones plus x86_64 emulators. The game runs in landscape mode and full screen.
- The UI is laid out for 1920×1080 and scales with the window (`canvas_items` stretch), so it fits browser windows and phone screens.

## Credits

Larry Costigan — Game Designer & Developer (GAME 266 - Core Principles: Game Tech, Prof. James Taylor). Third-party credits are on the in-game Credits screen and in each asset folder's `CREDITS.txt` file.
