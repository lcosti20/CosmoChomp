# Burn Phase 🚀🤖

**Burn Phase** is a third-person action-adventure prototype combining arcade platforming, multi-vehicle mechanics, and orbital physics combat across twin dying worlds. Developed in Unreal Engine as part of the GAME 405 Interactive Design and Game Dev Studio I course at the Savannah College of Art and Design (SCAD).

---

## 🌌 Overview & Premise

Eons after the extinction of biological life and the death of the stars, two tidally locked shell-worlds orbit twin miniature black holes separated by $1200\text{m}$ of open space. The **Red Faction** and **Blue Faction** have waged an endless, forgotten war for millions of years—so long that neither side retains any memory logs of why it started.

You play as **Bob-3267**, a newly rebooted maintenance unit who awakens amidst the crossfire. Across a ~15-minute story vertical slice, players navigate $360^\circ$ spherical gravity, pilot land and space vehicles, board orbiting frigates, and ultimately choose which faction to align with (or trick both into an arbitrary truce).

---

## 🎮 Key Features

* **Spherical Point Gravity:** Complete $360^\circ$ walking, platforming, and combat on spherical shell-worlds ($80\text{--}100\text{m}$ radius).
* **Dual Combat Modes:** Dynamic third-person targeting—toggle between **Melee Mode** for platforming/slashing and **Aim Mode** (Right Trigger) for laser rifles and rocket launchers.
* **Interplanetary Anti-Gravity Lifts:** Planet-facing acceleration pads that launch players, vehicles, and enemy NPCs across the $1200\text{m}$ void between worlds.
* **Multi-Vehicle Traversal:**
  * **Land:** Agile hovercrafts and heavy combat mechs.
  * **Atmospheric Flight:** Jets and helicopters bounded by planetary gas layers.
  * **Spaceflight & Orbital Physics:** Spacecraft featuring Keplerian orbital velocity mechanics.
* **Space Frigate Boarding:** Orbiting capital ships with localized interior artificial gravity for close-quarters boarding action.
* **Branching Story:** Lighthearted, witty dialogue system featuring faction choice paths between the Red and Blue Factions.

---

## 🛠️ Technical Details & Stack

* **Engine:** Unreal Engine 5
* **Primary Language:** C++ / Blueprints
* **Gravity Implementation:** Custom $N$-body / point-based gravity vector calculation per tick.
* **Platform:** PC (Gamepad & Keyboard/Mouse support)

---

## 📂 Repository Structure

```text
├── Config/                  # Project configuration files
├── Content/                 # Game assets, Blueprints, Materials, and Maps
│   ├── Audio/               # SFX and dialogue audio
│   ├── Blueprints/          # Core game logic, character controllers, and vehicle Blueprints
│   ├── Maps/                # Main level (ShellWorlds_Master.umap) and greybox test tracks
│   ├── Materials/           # Shaders, master materials, and visual effects
│   └── Meshes/              # 3D models for Bob-3267, vehicles, terrain, and props
├── Source/                  # C++ source code files
│   └── BurnPhase/           # Core game module source files
├── BurnPhase.uproject       # Unreal Engine project file
└── README.md                # Project documentation

```

---

## 🚀 Getting Started

### Prerequisites

* **Unreal Engine 5.x** installed via Epic Games Launcher.
* **Visual Studio 2022** (with C++ Game Development workload installed).

### Installation & Setup

1. Clone the repository:
```bash
git clone [https://github.com/lcosti20/burnphase.git](https://github.com/lcosti20/burnphase.git)

```


2. Right-click `BurnPhase.uproject` and select **Generate Visual Studio project files**.
3. Open `BurnPhase.sln` in Visual Studio and build the solution in `Development Editor` mode.
4. Launch `BurnPhase.uproject` to open the project in Unreal Editor.
5. Open `Content/Maps/ShellWorlds_Master.umap` and press **Play in Editor (PIE)**.

---

## 📅 Development Roadmap (10-Week Studio)

* [x] **Sprint 1: Project Pitch & Vision** — Initial concept, spherical gravity proof of concept.
* [x] **Sprint 2: Preproduction & Greybox** — Level diagram flow, Melee/Aim camera mechanics.
* [ ] **Sprint 3: Technical Framework** — Orbital physics, anti-gravity lifts, character controller, base UI framework.
* [ ] **Sprint 4: Production Alpha** — Vehicle possession logic, space frigate interior gravity, dialogue system, Test Candidate 01.
* [ ] **Sprint 5: Production Beta & Polish** — VFX/SFX pass, UI refinement, bug fixing, Test Candidate 02.
* [ ] **Sprint 6: Final Release & Trailer** — Gameplay trailer, final build submission, postmortem.

---

## 🌐 Project Documentation

For complete development logs, sprint breakdown screenshots, and course deliverables, visit the official outcome site:
👉 **[Larry's GAME 405 Course Outcome Site](https://sites.google.com/view/larrys-game-405-project/home)**

---

## 👤 Author

* **Larry Costigan** — Game Designer & Developer (SCAD)
* **Course:** GAME 405 - Interactive Design and Game Dev Studio I (Prof. Wan Chiu)
