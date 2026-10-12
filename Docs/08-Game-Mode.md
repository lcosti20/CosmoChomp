# Step 8: Game mode

**Difficulty:** Medium (no hard math, but it runs the whole round: setup, spawning, timers and win/lose)
**Replaces:** `Source/CosmoChomp/CosmoChompGameMode.h/.cpp`
**Creates:** `/Game/Blueprints/Core/BP_CosmoGameMode` (parent: **Game Mode Base**)

After this step, `Lvl_BlueprintTest` runs a full round on Blueprints. The only C++ left is the player character, the aliens and the maze, which are still used through the interfaces.

## What the C++ does

**Round setup** (`StartPlay` → `SetupRound`):
1. **Find the maze.** If the level has no maze, skip everything. This is how other levels opt out.
2. **Build the grid.** Call `BuildGrid`.
3. **Pick the start tile:** the tile closest to the player pawn, or to the Player Start if there's no pawn yet. Then `GenerateMaze(StartTile)`, which opens the start tile up into a plaza.
4. **Spawn the rocket** on the start tile, and bind its `OnLaunched`.
5. **Spawn the orbs:** one on every tile except the start tile, at height `OrbHeight` (70). Then `Rocket.RequiredOrbs = Clamp(Ceil(OrbCount × OrbsRequiredFraction), 1, OrbCount)`.
6. **Place the player.** Every player stands beside the rocket and faces it (see `PlacePlayer` below).
7. **Schedule the aliens:** after `AlienSpawnDelay` (4 s), spawn `NumAliens` (4).
8. **First ray gun:** if `bRayGunAtStart`, place a gun beside the rocket now.
9. **Ray gun timer:** after `RayGunRespawnDelay` (15 s), try to spawn a ray gun. It skips if one is already in the maze.
10. **Opening banner:** "Collect energy orbs and bring them back to your rocket!" for 6 s.

**During the round:**
- **`RestartPlayer` override:** places any newly spawned pawn beside the rocket.
- **`ScatterOrbs(Location, Count)`:** called by the character when it's hurt. Spawns `Count` orbs at random spots on the player's tile and the tiles open to it, within 28% of a tile spacing of each tile's center. Each one has `InitialPickupDelay = 1.5`.
- **Alien spawning:**
  - **Where:** each alien spawns on a tile at least half the maze's maximum walking distance from the player.
  - **Respawns:** when an alien dies (`OnKilled`), another spawns after `AlienRespawnDelay` (8 s).
- **Ray gun spawning:**
  - **One at a time:** only one gun is in the maze at once. `SpawnRayGun` picks a tile at least 35% of the maximum distance from the player, spawns the gun 90 units above it, and shows "A ray gun has appeared - follow the pink beacon!" for 4 s.
  - **After a pickup** (`OnCollected`): the next gun spawns after `Duration + RayGunRespawnDelay`.
- **Ending the round:**
  - **Win:** `OnLaunched` → `EndRound(true)` → "Rocket refueled - you escaped the planet!"
  - **Lose:** `HandlePlayerDeath` → `EndRound(false)` → "The aliens got you!"
  - **Restart:** both show the banner for `RestartDelay + 1` seconds, then reload the level after `RestartDelay` (6 s).
- **`GetBannerText()`:** returns the banner until its end time, then an empty string.

## Class defaults

| Setting | Value |
|---|---|
| Default Pawn Class | `BP_ThirdPersonCharacter` for now (`BP_Astronaut` after step 10) |
| HUD Class | `BP_CosmoHUD` |
| Player Controller Class | `BP_CosmoPlayerController` |

## Variables

| Name | Type | Default |
|---|---|---|
| `RocketClass` | `BP_Rocket` class | `BP_Rocket` |
| `OrbClass` | `BP_EnergyOrb` class | `BP_EnergyOrb` |
| `AlienClass` | Actor class | `AlienMonster` (the C++ class) until step 9, then `BP_Alien` |
| `RayGunClass` | `BP_RayGunPickup` class | `BP_RayGunPickup` |
| `OrbsRequiredFraction` | float | 0.5 |
| `NumAliens` | int | 4 |
| `AlienSpawnDelay` | float | 4.0 |
| `AlienRespawnDelay` | float | 8.0 |
| `RayGunRespawnDelay` | float | 15.0 |
| `RayGunAtStart` | bool | true |
| `RestartDelay` | float | 6.0 |
| `OrbHeight` | float | 70.0 |
| `AlienHalfHeight` | float | 90.0 (the alien capsule's half height; **Get Class Defaults** can't read component values, so store it here) |
| `RayGunDuration` | float | 10.0 (keep it equal to `BP_RayGunPickup.Duration`, or read it with **Get Class Defaults**) |
| `Maze` | Actor (implements `BPI_Maze`) | — |
| `Rocket` | `BP_Rocket` reference | — |
| `ActiveRayGun` | `BP_RayGunPickup` reference | — |
| `StartTile` | int | −1 |
| `RoundReady`, `RoundOver` | bool | false |
| `Banner` | String | — |
| `BannerEndTime` | float | 0 |

## Functions

**`SpawnOnTile(Class, Tile, Height)` → Actor.** This replaces the C++ template.
1. `Location` = **Get Tile Location** (Tile, Height); `Up` = **Get Tile Up** (Tile).
2. **TangentBasis** (Up) → `T`, `B`. `Angle` = **Random Float in Range** (0, 2π).
3. `Rotation` = **Get Surface Rotation** (Location, `T × cos(Angle) + B × sin(Angle)`).
4. **Spawn Actor from Class** (Class, Location, Rotation, Collision Handling = **Always Spawn**) → return it.

Callers cast the result to the class they spawned.

**`PlacePlayer(Pawn)`:**
1. Return if `RoundReady` is false, or if Maze or Pawn isn't valid.
2. `Neighbors` = **Get Neighbors** (`StartTile`); `TileUp` = **Get Tile Up** (`StartTile`).
3. `Outward` = **FlattenDirection** (`GetTileUp(Neighbors[0]) − TileUp`, `TileUp`). If there are no neighbors, use `(1, 0, 0)`.
4. `HalfHeight` = the pawn's capsule **Get Scaled Capsule Half Height** (if the pawn is a Character), else 100.
5. `Location` = **Project To Surface** (`GetTileLocation(StartTile, 0) + Outward × GetTileSpacing × 0.42`, `HalfHeight + 20`).
6. Pawn → **Teleport** (Location, **Get Surface Rotation** (Location, −Outward)).

**`GetPlayerTile`** (pure): **Find Nearest Tile** (player pawn location) if the pawn and maze are valid; otherwise `StartTile`.

**`PickDistantTile(FromTile, MinFraction)` → int:**
1. `Distances` = **Compute Distances** (FromTile). `MaxDistance` = the largest entry.
2. `Candidates` = every tile index ≠ `StartTile` with `Distances[i] >= MaxDistance × MinFraction`.
3. Return a random candidate, or **Random Integer** (`GetNumTiles`) if there are none.

**`ShowBanner(Text, Duration)`:** `Banner = Text`; `BannerEndTime = Get Game Time in Seconds + Duration`.

**`GetBannerText`** (pure): `Get Game Time in Seconds < BannerEndTime ? Banner : ""`.

**`GetMaze`**, **`GetRocket`** (pure): return the variables. The HUD uses these.

## Events

**Event BeginPlay → Delay Until Next Tick → `SetupRound`.**
The C++ ran `SetupRound` from `StartPlay`, after every actor's BeginPlay. Without the one-tick delay, the maze's own BeginPlay could run *after* setup and clear the walls that were just built. That matters for `BP_HexMaze` in step 11, which clears editor previews on BeginPlay.

**`SetupRound`:**
1. **Get All Actors with Interface** (`BPI_Maze`) → first one → `Maze`. If there's none, **Print String** "no maze in level" and stop.
2. **Build Grid**.
3. Start location: player pawn location → else the location of **Find Player Start** → else `PlanetCenter + (0, 0, PlanetRadius)`. `StartTile` = **Find Nearest Tile** (start location). **Generate Maze** (`StartTile`).
4. `Rocket` = **SpawnOnTile** (`RocketClass`, StartTile, 0) → cast to `BP_Rocket` → **Bind Event to OnLaunched** → `OnRocketLaunched`.
5. **SpawnOrbs**: for every tile index ≠ `StartTile`, call **SpawnOnTile** (`OrbClass`, tile, `OrbHeight`) and count the successes. Then set `Rocket.RequiredOrbs` from the formula above.
6. `RoundReady = true`. For each player controller (**Get All Actors of Class** PlayerController) → **PlacePlayer** (its pawn).
7. **Set Timer by Event** (`Max(AlienSpawnDelay, 0.01)`) → **SpawnAliens** (loop `NumAliens` times → **SpawnAlien**).
8. If `RayGunAtStart` → **SpawnRayGunNearRocket**.
9. **Set Timer by Event** (`Max(RayGunRespawnDelay, 0.01)`) → **SpawnRayGun**.
10. **ShowBanner** ("Collect energy orbs and bring them back to your rocket!", 6).

**Event OnRestartPlayer** (Game Mode Base event) → **PlacePlayer** (the controller's pawn). This replaces the `RestartPlayer` override.

**`SpawnAlien`:**
1. Return if `RoundOver`, or if Maze isn't valid.
2. **SpawnOnTile** (`AlienClass`, **PickDistantTile** (`GetPlayerTile`, 0.5), `AlienHalfHeight + 10`).
3. Bind its `OnKilled` → custom event `OnAlienKilled`. Until step 9 that's the C++ alien's `On Killed` dispatcher: cast to `AlienMonster`. After step 9, cast to `BP_Alien`.

**`OnAlienKilled`** → **Set Timer by Event** (`Max(AlienRespawnDelay, 0.01)`) → **SpawnAlien**.

**`SpawnRayGun`:**
1. Return if `RoundOver`, if Maze isn't valid, or if `ActiveRayGun` **Is Valid**.
2. `Tile` = **PickDistantTile** (`GetPlayerTile`, 0.35). **TangentBasis** (Get Tile Up) → `T`.
3. If **PlaceRayGun** (`GetTileLocation(Tile, 90)`, T) returns valid → **ShowBanner** ("A ray gun has appeared - follow the pink beacon!", 4).

**`SpawnRayGunNearRocket`:**
1. `Outward` as in `PlacePlayer`. `Side = Normalize(Cross(TileUp, Outward))`.
2. `Location` = **Project To Surface** (`GetTileLocation(StartTile, 0) + (Side × 0.75 + Outward × 0.25) × GetTileSpacing × 0.42`, 90).
3. **PlaceRayGun** (Location, Side).

**`PlaceRayGun(Location, Forward)` → `BP_RayGunPickup`:**
1. **Spawn Actor** `RayGunClass` at Location, rotation **Get Surface Rotation** (Location, Forward), Always Spawn.
2. Bind its `OnCollected` → `OnRayGunCollected`. Set `ActiveRayGun`. Return it.

**`OnRayGunCollected`:**
1. Clear `ActiveRayGun`.
2. **Set Timer by Event** (`RayGunDuration + Max(RayGunRespawnDelay, 0.01)`) → **SpawnRayGun**.

**`ScatterOrbs(Location, Count)`:**
1. Return if Maze isn't valid, or if `Count <= 0`.
2. `Tiles` = **Get Open Neighbors** (**Find Nearest Tile** (Location)), plus that tile itself.
3. Repeat `Count` times:
   1. `DropTile` = random element of `Tiles`.
   2. **TangentBasis** (Get Tile Up) → T, B.
   3. Random `Angle` in [0, 2π]; `Distance` = random in [0, 0.28] × tile spacing.
   4. `OrbLocation` = **Project To Surface** (`TileLocation + (T·cos + B·sin) × Distance`, `OrbHeight`).
   5. **Spawn Actor** `OrbClass` at OrbLocation, rotation **Get Surface Rotation** (OrbLocation, T), with **Initial Pickup Delay = 1.5** (exposed on spawn), Always Spawn.

**`HandlePlayerDeath(Player)`** → **EndRound** (false). **`OnRocketLaunched`** → **EndRound** (true).

**`EndRound(Escaped)`:**
1. Return if `RoundOver`; otherwise set it.
2. **ShowBanner** (Escaped ? "Rocket refueled - you escaped the planet!" : "The aliens got you!", `RestartDelay + 1`).
3. **Set Timer by Event** (`Max(RestartDelay, 0.01)`) → **Open Level (by Name)** (**Get Current Level Name**, Remove Prefix String ✓).

## Temporary shim until step 10

The C++ character calls the C++ game mode's `ScatterOrbs` and `HandlePlayerDeath` directly. With `BP_CosmoGameMode`, those calls quietly do nothing, so dropped orbs vanish and the round never ends when you die. Bridge the gap in `BP_ThirdPersonCharacter`:

**Event AnyDamage.** When this event fires, the C++ hasn't yet cleared the carried orbs or applied death.
1. `Orbs` = **Get Carried Orbs**.
2. **Get Game Mode** → cast to `BP_CosmoGameMode`.
3. If `Orbs > 0` → **Scatter Orbs** (actor location, Orbs).
4. **Delay Until Next Tick** → if **Is Dead** → **Handle Player Death** (self).

Delete this shim in step 10, when `BP_Astronaut` replaces `BP_ThirdPersonCharacter`.

## Also update

- **`BP_CosmoHUD` (step 3):** change its game mode cast to `BP_CosmoGameMode`.
- **`BP_TestGameMode`:** in `Lvl_BlueprintTest`'s **World Settings**, set GameMode Override to `BP_CosmoGameMode`. `BP_TestGameMode` is no longer needed; delete it.

## Test

Play `Lvl_BlueprintTest`:

1. The maze generates, the rocket lands on the start tile, orbs fill every other tile, and you start next to the rocket facing it.
2. A ray gun is beside the rocket, and the opening banner shows.
3. After 4 s, four aliens appear far away.
4. Get hit by an alien while carrying orbs. They scatter nearby, and can be picked up after 1.5 s.
5. Collect half the orbs and deposit them. The rocket launches, the win banner shows, and the level restarts 6 s later.
6. Die. The lose banner shows, and the level restarts.
7. Pick up the ray gun. About 25 s later (10 s + 15 s), a new gun appears with its banner.

## Done when

- [ ] A full round plays from start to win, and from start to loss, on `BP_CosmoGameMode`.
- [ ] Spawn timings and banners match the C++ game.
