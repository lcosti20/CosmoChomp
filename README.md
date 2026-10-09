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
├── Source/                     # C++ source code for the project module
│   └── CosmoChomp/             # Main gameplay implementation
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
5. Launch the project from Unreal Editor and begin exploring the planet.

## Controls

- Movement: analog stick / WASD
- Collect energy orbs: move through the maze and pick up glowing power cells
- Fight back: use the ray gun when you find it to shoot alien monsters
- Escape: gather enough energy and return to your spaceship to leave the planet

## Credits

- Larry Costigan — Game Designer & Developer
- Course: GAME 266 - Core Principles: Game Tech
- Instructor: Prof. James Taylor

## Notes

Cosmo Chomp is focused on a compact, replayable prototype built around the fantasy of exploring a dangerous alien maze, collecting energy, and escaping before the planet's creatures overrun you. The game is designed to feel easy to learn, but strategically tense as you balance exploration, combat, and escape timing.
