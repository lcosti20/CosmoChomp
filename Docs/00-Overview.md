# Converting Cosmo Chomp from C++ to Blueprints

This guide walks through rebuilding every piece of Cosmo Chomp's custom C++ as Blueprints, from the easiest piece to the hardest. Each step has its own file:

| Step | Doc | Replaces | Difficulty |
|---|---|---|---|
| 1 | [Shared helpers and mesh fitting](01-Shared-Helpers-and-Mesh-Fitting.md) | `CosmoChompMeshFitting.h` | Easy |
| 2 | [Pause menu and player controller](02-Pause-Menu-and-Player-Controller.md) | `CosmoChompPauseMenu`, `CosmoChompPlayerController` | Easy |
| 3 | [HUD: stats, banner and ray gun timer](03-HUD.md) | `CosmoChompHUD` (everything except the mini map) | Easy |
| 4 | [Energy orb](04-Energy-Orb.md) | `EnergyOrb` | Easy |
| 5 | [Ray gun pickup and beam](05-Ray-Gun-Pickup-and-Beam.md) | `RayGunPickup` (`ARayGunPickup`, `ARayGunBeam`) | Easy–Medium |
| 6 | [Rocket](06-Rocket.md) | `CosmoRocket` | Medium |
| 7 | [Planetary gravity component](07-Planetary-Gravity-Component.md) | `PlanetaryGravityComponent` | Medium |
| 8 | [Game mode](08-Game-Mode.md) | `CosmoChompGameMode` | Medium |
| 9 | [Alien monster](09-Alien-Monster.md) | `AlienMonster` | Medium–Hard |
| 10 | [Player character](10-Player-Character.md) | `CosmoChompCharacter` | Hard |
| 11 | [Hex maze and planet grid](11-Hex-Maze-and-Planet-Grid.md) | `HexMaze`, `HexPlanetGrid` | Hard |
| 12 | [Mini map](12-Mini-Map.md) | `CosmoChompHUD` (mini map) | Hard |
| 13 | [Cutover: removing the C++](13-Cutover-Removing-CPP.md) | `Source/`, project settings | Medium |

The order runs from easiest to hardest, with one exception: the mini map (step 12) comes after the maze (step 11). It's a little easier than the maze, but it needs the maze's grid data, which the C++ maze doesn't expose to Blueprints.

All file paths in this guide are relative to the project root. C++ references point at `Source/CosmoChomp/`.

---

## How the conversion works

### Build side by side, cut over at the end

The C++ classes depend on each other in a loop:
- The character calls the game mode.
- The game mode spawns the orbs, rocket, aliens and ray guns.
- Those classes all call the character.

So you can't swap one class at a time and keep the shipping game working. Instead:

1. **Leave the shipping level alone.** `Lvl_CosmoChomp` keeps running on the C++ classes for the whole conversion, so the team can keep playtesting.
2. **Build the new Blueprints in a new folder,** `/Game/Blueprints/`, and test them in a copy of the level, `Lvl_BlueprintTest`.
3. **Switch the shipping level over in one go at the end,** then delete the C++ (step 13).

### Interfaces let new Blueprints talk to old C++

Most new Blueprints need to talk to the player and the maze, but those are converted near the end (steps 10 and 11). To keep earlier pieces testable, they talk through **Blueprint Interfaces** instead of casting to a specific class:

- **`BPI_CosmoPlayer`** is implemented first by the existing `BP_ThirdPersonCharacter`, which is a Blueprint child of the C++ character. Each function just forwards to the matching C++ function. In step 10 the new Blueprint astronaut implements the same interface, and nothing that calls it has to change.
- **`BPI_Maze`** works the same way. It's implemented first by `BP_MazeAdapter`, a Blueprint child of the C++ `HexMaze`, and later by the all-Blueprint `BP_HexMaze`.

### The dependency picture

| Class | Calls into | Called by |
|---|---|---|
| Mesh fitting | — | Character, Alien, Rocket, Ray gun pickup |
| Pause menu | Player controller | Player controller |
| Player controller | Pause menu | Pause menu |
| HUD | Game mode, Character, Rocket, Maze, Orbs, Aliens, Ray gun | Game mode (HUD class) |
| Energy orb | Character | Game mode, HUD |
| Ray gun pickup | Character | Game mode, HUD |
| Ray gun beam | — | Character |
| Rocket | Character | Game mode, HUD |
| Gravity component | — | Character, Alien |
| Game mode | Maze, Rocket, Orb, Alien, Ray gun, HUD | Character, HUD |
| Alien | Maze, Character, Gravity component | Game mode, HUD, Character (shooting) |
| Character | Game mode, Alien, Beam, Gravity component | Orb, Ray gun, Rocket, Alien, Game mode, HUD |
| Maze and grid | — | Game mode, Alien, HUD |

---

## One-time setup (do this before step 1)

### 1. Folders

Create these folders in the Content Browser:

```
/Game/Blueprints/Core        game mode, player controller, character, HUD
/Game/Blueprints/Gameplay    orb, rocket, ray gun, alien
/Game/Blueprints/Maze        maze, grid struct
/Game/Blueprints/UI          pause menu
/Game/Blueprints/Interfaces  BPI_CosmoPlayer, BPI_Maze
/Game/Blueprints/Library     BPFL_PlanetMath
```

### 2. Test level

1. Duplicate `/Game/Maps/Lvl_CosmoChomp` as `/Game/Maps/Lvl_BlueprintTest`.
2. Create `BP_TestGameMode` in `/Game/Blueprints/Core`, parented to the existing `BP_ThirdPersonGameMode`. You'll point its class slots at new Blueprints as they're finished.
3. Open `Lvl_BlueprintTest`. In **World Settings**, set **GameMode Override** to `BP_TestGameMode`.

### 3. `BPI_CosmoPlayer`: the player interface

Create a Blueprint Interface `/Game/Blueprints/Interfaces/BPI_CosmoPlayer` with these functions:

| Function | Inputs | Output |
|---|---|---|
| `IsInPlay` | — | `bool` |
| `IsDead` | — | `bool` |
| `GetHealth` | — | `int` |
| `GetMaxHealth` | — | `int` |
| `GetCarriedOrbs` | — | `int` |
| `AddOrbs` | `Count` (int) | — |
| `TakeAllOrbs` | — | `int` |
| `HasRayGun` | — | `bool` |
| `GetRayGunTimeRemaining` | — | `float` |
| `GetRayGunDuration` | — | `float` |
| `EquipRayGun` | `Duration` (float) | — |
| `BoardRocket` | — | — |

Then open `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`:
1. **Class Settings → Implemented Interfaces → Add** `BPI_CosmoPlayer`.
2. Implement each function by calling the C++ function of the same name. They're all exposed to Blueprints already. For example, `AddOrbs` → **Add Orbs** (target: self).

Adding the interface doesn't change how the shipping game plays.

### 4. `BPI_Maze`: the maze interface

Create `/Game/Blueprints/Interfaces/BPI_Maze` with these functions, mirroring the C++ `AHexMaze` API:

| Function | Inputs | Output |
|---|---|---|
| `BuildGrid` | — | — |
| `GenerateMaze` | `StartTile` (int) | — |
| `GetNumTiles` | — | `int` |
| `GetPlanetCenter` | — | `Vector` |
| `GetPlanetRadius` | — | `float` |
| `GetTileSpacing` | — | `float` |
| `GetTileUp` | `Tile` (int) | `Vector` |
| `GetTileLocation` | `Tile` (int), `Height` (float) | `Vector` |
| `ProjectToSurface` | `WorldLocation` (Vector), `Height` (float) | `Vector` |
| `GetSurfaceRotation` | `WorldLocation` (Vector), `Forward` (Vector) | `Rotator` |
| `FindNearestTile` | `WorldLocation` (Vector) | `int` |
| `GetNeighbors` | `Tile` (int) | `int[]` |
| `GetOpenNeighbors` | `Tile` (int) | `int[]` |
| `IsPassage` | `A` (int), `B` (int) | `bool` |
| `ComputeDistances` | `FromTile` (int) | `int[]` |
| `FindPath` | `FromTile` (int), `ToTile` (int) | `int[]` |

Then:
1. Create `BP_MazeAdapter` in `/Game/Blueprints/Maze`, parented to the C++ class **HexMaze**.
2. Add `BPI_Maze` to it, and implement each function by calling the C++ function of the same name.
3. In `Lvl_BlueprintTest`, delete the `HexMaze` actor and place a `BP_MazeAdapter` at the origin `(0, 0, 0)` in its place. Copy over any properties the team changed on the original, such as **Wall Meshes** and **Post Mesh**.

The C++ game mode finds the maze by class, and `BP_MazeAdapter` is a child of `HexMaze`, so the test level still plays normally.

### 5. A collision channel for the ray gun

The ray gun's shot has to stop at walls, aliens, the rocket and the ground, but pass through pickups and trigger zones. The C++ used the built-in Pawn channel for this. A Blueprint trace node can't use Pawn, because it's an *object* channel. So add a dedicated channel:

1. Go to **Project Settings → Engine → Collision → Trace Channels → New Trace Channel**.
2. Name it `RayGun`, with **Default Response: Block**.
3. Open `/Game/BP_Planet`, select **GravityWell**, and set its `RayGun` response to **Ignore**. Every other trigger you build in later steps uses "Ignore all, overlap Pawn", which covers the new channel automatically.

Do this first. Collision settings made before the channel existed won't include it.

---

## C++ → Blueprint cheat sheet

| C++ | Blueprint |
|---|---|
| `UPROPERTY(EditAnywhere)` | Variable with **Instance Editable** checked |
| `UPROPERTY(VisibleAnywhere)` component | Component added in the **Components** panel |
| `ConstructorHelpers::FObjectFinder` (asset loading) | Set the asset as the default in **Class Defaults** or the component's **Details** |
| Code in the constructor | Defaults in **Class Defaults** / component **Details**; anything computed goes in the **Construction Script** |
| `BeginPlay` / `Tick` override | **Event BeginPlay** / **Event Tick** |
| `TakeDamage` override | **Event AnyDamage** |
| `DECLARE_DYNAMIC_MULTICAST_DELEGATE` + `Broadcast` | **Event Dispatcher** + **Call** |
| `AddDynamic(this, &Class::Func)` | **Bind Event to …** with a Custom Event |
| `GetWorldTimerManager().SetTimer` | **Set Timer by Event** (keep the returned handle if you need to clear it) |
| `TActorIterator<T>` | **Get All Actors Of Class** (cache the result; don't call it every tick) |
| `Cast<T>(Actor)` | **Cast To T**, or an interface message call |
| `SpawnActorDeferred` + setting a property | **Spawn Actor from Class** with the variable marked **Expose on Spawn** |
| `FVector::VectorPlaneProject(V, N)` | **Project Vector on to Plane** |
| `FRotationMatrix::MakeFromXZ(X, Z)` | **Make Rot from XZ** |
| `FMath::QInterpTo` | **Lerp (Rotator)** with **Shortest Path** checked, alpha = `Clamp(DeltaSeconds × Speed, 0, 1)` |
| `FQuat` rotation about an axis | **Rotate Vector Around Axis** (degrees) |
| `FRandomStream` | **Random Stream** struct + **Random Integer from Stream** / **Random Float from Stream** |
| `TSet<uint64>` edge keys | **Set** of `int`, with key `Min(A,B) × 10000 + Max(A,B)` (works for up to 10,000 tiles) |
| `INDEX_NONE` | `-1` |
| `UE_LOG` | **Print String** (untick *Print to Screen* for log-only) |

### Performance rules of thumb

Blueprint code runs roughly 10× slower than C++. Cosmo Chomp's hot spots are:
- the maze grid generation (step 11)
- the per-frame mini map (step 12)
- the aliens' route searches (step 9)

Each of those docs explains how to keep it fast. In general:
- Don't call **Get All Actors Of Class** in Tick. Cache references, or let the game mode keep arrays of spawned actors.
- Use **Set Timer by Event** for anything that doesn't need to run every frame.
- Watch the **Maximum Loop Iteration Count** (Project Settings → Engine → General Settings, default 1,000,000). A Blueprint that exceeds it stops with an "Infinite loop detected" error. Grid generation is the only step that comes close.
- Profile with `stat game` and Unreal Insights after steps 9, 11 and 12.

If the team finds the maze generation or mini map too slow in Blueprints, the usual compromise is to keep just those two algorithms in a small C++ Blueprint Function Library, and do everything else in Blueprints.

---

## Testing each step

Every doc ends with a **Test** section and a **Done when** checklist. Test in `Lvl_BlueprintTest`. Play-In-Editor uses **Shift+Escape** to stop (see `Config/DefaultEditorSettings.ini`), so Escape reaches the pause menu.

Keep the shipping level, `Lvl_CosmoChomp`, untouched until step 13.
