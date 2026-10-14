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
- Primary tooling: Blueprints (no C++ module; the game was converted from C++, see `Docs/`)
- Focus areas: procedural generation, maze traversal, enemy AI, combat interactions, player movement, and world design
- Platform: PC with keyboard/mouse and gamepad support

## Repository Structure

```text
CosmoChomp/
├── Config/                     # Unreal project configuration files
├── Content/                    # Game assets, blueprints, maps, materials, and audio
│   ├── Audio/                  # Sound effects, dialogue audio and the music loop (S_Music)
│   ├── Blueprints/             # All gameplay logic
│   │   ├── Core/               # Game mode, player controller, astronaut, HUD, planetary gravity
│   │   ├── Gameplay/           # Rocket, energy orb, alien, ray gun pickup and beam
│   │   ├── Maze/               # BP_HexMaze: hex grid on the sphere and maze generation
│   │   ├── UI/                 # Start menu, pause menu, mini map material
│   │   ├── Interfaces/         # BPI_CosmoPlayer, BPI_Maze
│   │   └── Library/            # BPFL_PlanetMath
│   ├── Maps/                   # Level files and test environments
│   ├── Materials/              # Materials, shaders, and VFX setup
│   └── Meshes/                 # 3D assets for characters, props, and environment
├── Assets/                     # Raw source models, textures and sounds (imported by Tools/AssetPipeline)
├── Docs/                       # How the original C++ was converted to Blueprints (start at 00-Overview.md)
├── Tools/AssetPipeline/        # Scripts that convert and import Assets/ into Content/
├── CosmoChomp.uproject         # Unreal Engine project file
├── .gitignore                 # Git ignore rules for Unreal projects
├── .gitattributes             # Git attributes configuration
├── .vsconfig                  # Visual Studio configuration (only needed if C++ is ever re-added)
├── README.md                  # Project documentation
├── LICENSE                    # Project licensing details
└── .github/                   # GitHub configuration and workflows
```

## Getting Started

### Prerequisites

- Unreal Engine 5.8 installed through the Epic Games Launcher (no Visual Studio or compiler needed)

### Setup

1. Clone the repository:

```bash
git clone https://github.com/lcosti20/CosmoChomp.git
```

2. Open `CosmoChomp.uproject` in Unreal Engine.
3. Press Play. The editor opens on the game level, `Content/Maps/Lvl_CosmoChomp`, so Play drops straight into a round; `Lvl_BlueprintTest` is a sandbox copy used during the conversion.

A packaged build starts on the start menu, `Content/Maps/Lvl_MainMenu` (the game default map). `WBP_MainMenu` offers Start (loads `Lvl_CosmoChomp`), Credits (the credits below) and Quit; `BP_MenuGameMode` shows it with no pawn, viewed through the level's `MenuCamera`. If you change the Credits section of this README, update the `CreditsBody` text in `WBP_MainMenu` to match.

Background music (`S_Music`, a looping sound wave) starts with the first level and keeps playing across level loads, round restarts included. `BP_CosmoGameInstance` (the project's Game Instance class) spawns it once with *Persist Across Level Transition* set and keeps the audio component; both game modes call its `StartMusic` on BeginPlay, which does nothing if the music is already playing. Change the volume on the `SpawnSound2D` node in `StartMusic` (currently 0.5).

## Controls

- Movement: analog stick / WASD
- Collect energy orbs: move through the maze and pick up glowing power cells
- Deposit orbs: walk up to the rocket; everything you carry is loaded as fuel
- Fight back: when you're holding the ray gun, fire a laser with left mouse / F / right trigger; a countdown bar at the bottom of the screen shows how long the gun lasts
- Escape: gather enough energy and return to your spaceship to leave the planet
- Mini map (top right): centered on you and turned to face where the camera looks. Lines are tiles and walls, dots are energy orbs, the red triangle is the rocket, the green sideways triangle is a ray gun, and yellow squares are aliens. The rocket and ray gun stay on the rim when out of range, pointing the way. It uses an azimuthal equidistant projection, so distance from the center is true distance over the planet; zoom with `MiniMapRangeDegrees` on `BP_CosmoHUD` (180 shows the whole planet)
- Pause menu: Escape / P / gamepad Start. In Play-In-Editor, this project moves "Stop" to Shift+Escape (`Config/DefaultEditorSettings.ini`) so Escape reaches the game

## How It Works

A round runs in any level that contains a `BP_HexMaze` actor and a `Planet`-tagged actor (`BP_Planet`). `BP_CosmoGameMode` sets everything up when play starts:

1. **Hex tile maze** (`BP_HexMaze`): the grid tiles the sphere as a Goldberg polyhedron (a subdivided icosahedron: all hexagons plus the 12 pentagons a sphere requires). It sizes the grid from the planet radius and `TargetTileSize`, carves a maze with a randomized depth-first search, then knocks out most dead ends (`BraidChance`) so there are loops to escape aliens through. Walls are the pieces split out of the wall model, stretched along each closed tile edge, with posts covering the corners. Use **Preview In Editor** on the `BP_HexMaze` actor to see a layout without playing; set `Seed` to repeat one.
2. **Rocket** (`BP_Rocket`): lands on the tile nearest the PlayerStart; the player spawns beside it. Standing near it deposits carried orbs. Once `OrbsRequiredFraction` of the maze's orbs are delivered, the player boards and it launches.
3. **Energy orbs** (`BP_EnergyOrb`): one per tile. Getting hurt drops every carried orb around you (briefly uncollectable), so bank them often.
4. **Aliens** (`BP_Alien`): appear after a short grace period, far from the player. They navigate the maze's tile graph (navmesh doesn't work on a sphere): wander, chase when you're within a few tiles, and claw at close range. Three hits and the round restarts.
5. **Ray gun** (`BP_RayGunPickup`): one starts beside the rocket (toggle with `RayGunAtStart` on the game mode), and later ones spawn on random distant tiles, marked by a tall pink beacon. There's at most one in the maze at a time. Picking it up lets you shoot for 10 seconds, and aliens flee from you while you're armed. Shots follow the curve of the planet. A new gun appears some time after the last runs out.

Tuning lives on the `BP_HexMaze` actor (tile size, braiding, wall sizes) and on `BP_CosmoGameMode`'s variables (orb requirement, alien count, spawn and respawn delays).

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

Music: ["Video Game Pixel Chiptune Music"](https://pixabay.com/music/synthwave-video-game-pixel-chiptune-music-583271/) by [alex-morgan](https://pixabay.com/users/alex-morgan-54692529/) on Pixabay, used under the Pixabay Content License. The track is registered with YouTube Content ID (license certificate in `Assets/sounds/music/`).

## Notes

Cosmo Chomp is focused on a compact, replayable prototype built around the fantasy of exploring a dangerous alien maze, collecting energy, and escaping before the planet's creatures overrun you. The game is designed to feel easy to learn, but strategically tense as you balance exploration, combat, and escape timing.
