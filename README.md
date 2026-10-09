# Burn Phase 🚀

Burn Phase is a third-person action-adventure prototype that blends arcade platforming, vehicle traversal, and orbital combat across twin dying worlds. Built in Unreal Engine 5, the project explores spherical gravity, anti-gravity travel, and large-scale traversal in a compact vertical-slice experience.

## Overview

Eons after the extinction of biological life, two shell-worlds orbit twin miniature black holes and are separated by a narrow but deadly void. You play as Bob-3267, a newly rebooted maintenance unit, as he awakens amid escalating conflict between the Red and Blue factions.

The prototype focuses on a short story-driven experience with fast movement, environmental traversal, and dynamic combat across multiple gameplay layers:

- walking and platforming across spherical shell-worlds
- vehicle-based movement across land, atmosphere, and space
- weapon switching between melee and ranged combat
- faction-driven narrative choices and dialogue
- a compact 10-week studio prototype aimed at testing core systems

## Core Features

- Spherical gravity traversal: explore a 360-degree world with gravity-driven movement and platforming
- Dual combat modes: melee-focused movement and aim-assisted ranged attacks
- Anti-gravity lifts: launch across the gap between worlds and use momentum to traverse large distances
- Multi-vehicle traversal:
  - land vehicles such as hovercrafts and mechs
  - atmospheric vehicles such as jets and helicopters
  - spacecraft using orbital velocity and spaceflight physics
- Space frigate boarding: close-quarters combat and traversal inside orbital structures
- Branching narrative: faction-based choices and lighthearted dialogue with replayable paths

## Technical Stack

- Engine: Unreal Engine 5
- Primary tooling: C++ and Blueprints
- Focus areas: custom gravity systems, orbital movement, vehicle logic, combat flow, and level traversal
- Platform: PC with gamepad and keyboard/mouse support

## Repository Structure

```text
BurnPhase/
├── Config/                     # Unreal project configuration files
├── Content/                    # Game assets, blueprints, maps, materials, and audio
│   ├── Audio/                  # Sound effects and dialogue audio
│   ├── Blueprints/             # Core gameplay logic and systems
│   ├── Maps/                   # Level files and greybox test environments
│   ├── Materials/              # Materials, shaders, and VFX setup
│   └── Meshes/                 # 3D assets for characters, props, and vehicles
├── Source/                     # C++ source code for the project module
│   └── BurnPhase/              # Main gameplay implementation
├── BurnPhase.uproject          # Unreal Engine project file
├── .gitignore                 # Git ignore rules for Unreal projects
├── .gitattributes             # Git attributes configuration
├── .vsconfig                  # Visual Studio configuration for UE projects
├── README.md                  # Project documentation
└── LICENSE                    # If present, repository licensing details
```

## Getting Started

### Prerequisites

- Unreal Engine 5.x installed through the Epic Games Launcher
- Visual Studio 2022 with the C++ Game Development workload

### Setup

1. Clone the repository:

```bash
git clone https://github.com/lcosti20/burnphase.git
```

2. Open `BurnPhase.uproject` in Unreal Engine.
3. If prompted, generate project files and allow Unreal Engine to configure the project.
4. Open the generated solution in Visual Studio and build the project in Development Editor mode.
5. Launch the project from Unreal Editor and open the main map to begin testing.

## Controls

The prototype is designed for controller-first play, with a support-focused control scheme:

- Movement: analog stick / WASD
- Melee mode: close-range combat and traversal-focused action
- Aim mode: precision targeting for ranged weapons
- Right trigger: toggle to aim / use ranged combat tools

## Notes

This repository is a prototype and may evolve as gameplay systems and content are expanded during development. The focus is on proving the core concept: a gravity-driven, multi-vehicle action game set across two linked worlds.
