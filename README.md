# Cosmo Chomp 🚀

Cosmo Chomp is a puzzle/action prototype inspired by classic maze-chasers like Pac-Man and the playful space exploration energy of the Galactic Adventure expansion in Spore. You play as a space explorer who has landed on an alien world and must collect energy orbs to refuel your spacecraft before the planet's dangers overwhelm you.

## Overview

After a rough landing, your ship is stranded on a mysterious planet covered in a randomly generated maze of hexagonal tiles. The surface is a spherical world, twisting in every direction, and the path ahead is filled with glowing energy orbs that power your escape. But the maze is not empty: alien monsters lurk in the shadows, roaming the planet and attacking anything that wanders too close.

To survive and escape, you must:

- explore the planet's maze-like terrain
- collect enough energy orbs to recharge your ship
- avoid or defeat alien monsters roaming the area
- find a temporary ray gun to defend yourself
- return to your spacecraft and escape the planet once your fuel is restored

## Core Gameplay

- Hexagonal tiled spherical world: navigate a maze built across a curved alien planet surface
- Randomly generated levels: each run creates a fresh maze layout and new enemy patterns
- Energy orb collection: gather glowing orbs to power your spaceship
- Monster encounters: alien creatures patrol the maze and attack on sight
- Ray gun pickup: collect a temporary weapon that lets you shoot monsters for a limited time
- Escape sequence: once enough energy is gathered, head back to your ship to leave the planet

## Key Features

- Maze-based exploration with a classic arcade feel
- Spherical planet traversal with spatial awareness and route planning
- Procedural map generation for replayability
- Survival and action tension from roaming alien enemies
- Temporary power-up combat using a ray gun
- Escape-driven progression that turns collection into a satisfying objective loop

## Technical Stack

- Engine: Unreal Engine 5
- Primary tooling: C++ and Blueprints
- Focus areas: procedural generation, maze traversal, enemy AI, combat interactions, player movement, and world design
- Platform: PC with keyboard/mouse and gamepad support

## Repository Structure

```text
CosmoChomp/
├── Config/                     # Unreal project configuration files
├── Content/                    # Game assets, blueprints, maps, materials, and audio
│   ├── Audio/                  # Sound effects and dialogue audio
│   ├── Blueprints/             # Core gameplay logic and systems
│   ├── Maps/                   # Level files and test environments
│   ├── Materials/              # Materials, shaders, and VFX setup
│   └── Meshes/                 # 3D assets for characters, props, and environment
├── Assets/                     # Raw source models, textures and sounds (imported by Tools/AssetPipeline)
├── Source/                     # C++ source code for the project module
│   └── CosmoChomp/             # Main gameplay implementation
│       ├── Maze/               # Hex grid on the sphere and maze generation
│       └── Gameplay/           # Rocket, energy orbs, aliens, ray gun
├── Docs/                       # Guides, incl. converting the C++ to Blueprints (start at 00-Overview.md)
├── Tools/AssetPipeline/        # Scripts that convert and import Assets/ into Content/
├── CosmoChomp.uproject         # Unreal Engine project file
├── .gitignore                 # Git ignore rules for Unreal projects
├── .gitattributes             # Git attributes configuration
├── .vsconfig                  # Visual Studio configuration for UE projects
├── README.md                  # Project documentation
├── LICENSE                    # Project licensing details
└── .github/                   # GitHub configuration and workflows
```

## Getting Started

### Prerequisites

- Unreal Engine 5.x installed through the Epic Games Launcher
- Visual Studio 2022 with the C++ Game Development workload

### Setup

1. Clone the repository:

```bash
git clone https://github.com/lcosti20/CosmoChomp.git
```

2. Open `CosmoChomp.uproject` in Unreal Engine.
3. If prompted, generate project files and allow Unreal Engine to configure the project.
4. Open the generated solution in Visual Studio and build the project in Development Editor mode.
5. Launch the project from Unreal Editor and begin exploring the planet. The game level is `Content/Maps/Lvl_CosmoChomp` (the default map); `Lvl_ThirdPerson` is still the original planet test level.

## Controls

- Movement: analog stick / WASD
- Collect energy orbs: move through the maze and pick up glowing power cells
- Deposit orbs: walk up to the rocket; everything you carry is loaded as fuel
- Fight back: when you're holding the ray gun, fire a laser with left mouse / F / right trigger; a countdown bar at the bottom of the screen shows how long the gun lasts
- Escape: gather enough energy and return to your spaceship to leave the planet
- Mini map (top right): centered on you and turned to face where the camera looks. Lines are tiles and walls, dots are energy orbs, the red triangle is the rocket, the green sideways triangle is a ray gun, and yellow squares are aliens. The rocket and ray gun stay on the rim when out of range, pointing the way. It uses an azimuthal equidistant projection, so distance from the center is true distance over the planet; zoom with `MiniMapRangeDegrees` on `ACosmoChompHUD` (180 shows the whole planet)
- Pause menu: Escape / P / gamepad Start. In Play-In-Editor, this project moves "Stop" to Shift+Escape (`Config/DefaultEditorSettings.ini`) so Escape reaches the game

## How It Works

A round runs in any level that contains a `HexMaze` actor and a `Planet`-tagged actor (`BP_Planet`). `ACosmoChompGameMode` sets everything up when play starts:

1. **Hex tile maze** (`Maze/`): `FHexPlanetGrid` tiles the sphere as a Goldberg polyhedron (a subdivided icosahedron: all hexagons plus the 12 pentagons a sphere requires). `AHexMaze` sizes the grid from the planet radius and `TargetTileSize`, carves a maze with a randomized depth-first search, then knocks out most dead ends (`BraidChance`) so there are loops to escape aliens through. Walls are the pieces split out of the wall model, stretched along each closed tile edge, with posts covering the corners. Use **Preview In Editor** on the `HexMaze` actor to see a layout without playing; set `Seed` to repeat one.
2. **Rocket** (`ACosmoRocket`): lands on the tile nearest the PlayerStart; the player spawns beside it. Standing near it deposits carried orbs. Once `OrbsRequiredFraction` of the maze's orbs are delivered, the player boards and it launches.
3. **Energy orbs** (`AEnergyOrb`): one per tile. Getting hurt drops every carried orb around you (briefly uncollectable), so bank them often.
4. **Aliens** (`AAlienMonster`): appear after a short grace period, far from the player. They navigate the maze's tile graph (navmesh doesn't work on a sphere): wander, chase when you're within a few tiles, and claw at close range. Three hits and the round restarts.
5. **Ray gun** (`ARayGunPickup`): one starts beside the rocket (toggle with `bRayGunAtStart` on the game mode), and later ones spawn on random distant tiles, marked by a tall pink beacon. There's at most one in the maze at a time. Picking it up lets you shoot for 10 seconds, and aliens flee from you while you're armed. Shots follow the curve of the planet. A new gun appears some time after the last runs out.

Tuning lives on the `HexMaze` actor (tile size, braiding, wall sizes) and on the game mode's **Cosmo Chomp** category (orb requirement, alien count, spawn and respawn delays).

## Asset Pipeline

The raw files in `Assets/` are imported by `Tools/AssetPipeline`; the results are already in `Content/` (`Meshes/`, `Materials/`, `Audio/`). To re-import after changing a source asset, close the editor and run:

```bash
pip install trimesh pycollada
python Tools/AssetPipeline/build_assets.py
```

This needs ffmpeg on the `PATH`, Blender 5.x and UE 5.8 (override their paths with the `BLENDER_EXE` / `UE_EDITOR_CMD` environment variables). It converts the MP3 sounds to WAV, converts the ray gun from Collada, fixes the astronaut's mirrored mesh and cuts its single animation into `Idle` (frames 1–41) and `Walk` (61–121) takes, splits the wall model into its six pieces with bottom-centered pivots (`SM_Wall_01`..`06`), then imports everything and builds materials. Re-running updates assets in place; delete a material in the editor if you want it rebuilt. `ue_create_level.py` is the script that created `Lvl_CosmoChomp`.

## Credits

- Larry Costigan — Game Designer & Developer
- Course: GAME 266 - Core Principles: Game Tech
- Instructor: Prof. James Taylor

### Third-party assets

Models (all licensed under [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/)):

- ["Cute Astronaut"](https://skfb.ly/otLzp) by haefu
- ["One-eyed Monster with animations"](https://skfb.ly/pDXFI) by Jungle Jim
- ["Rocket"](https://skfb.ly/6UxWR) by Rafa Andrade
- ["Simple Ray Gun"](https://skfb.ly/6XyDY) by Playblast
- ["Wall Ready For Construction"](https://skfb.ly/oRTDy) by Mehdi Shahsavan

Sound effects from Pixabay: [distant bang](https://pixabay.com/sound-effects/distant-bang-472364/), [scorpion claw attack](https://pixabay.com/sound-effects/film-special-effects-scorpion-claw-attack-4-482507/), [footsteps on hard floor](https://pixabay.com/sound-effects/film-special-effects-footsteps-on-hard-floor-356919/), [sci-fi gun shoot](https://pixabay.com/sound-effects/film-special-effects-scifi-gun-shoot-1-266417/), [ough](https://pixabay.com/sound-effects/people-ough-47202/), [rocket launch](https://pixabay.com/sound-effects/film-special-effects-rocket-launch-306441/), [slime](https://pixabay.com/sound-effects/film-special-effects-slime-2-30099/), [cartoon rocket whoosh](https://pixabay.com/sound-effects/film-special-effects-cartoon-rocket-whoosh-229207/).

## Notes

Cosmo Chomp is focused on a compact, replayable prototype built around the fantasy of exploring a dangerous alien maze, collecting energy, and escaping before the planet's creatures overrun you. The game is designed to feel easy to learn, but strategically tense as you balance exploration, combat, and escape timing.
