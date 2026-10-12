# Step 6: Rocket

**Difficulty:** Medium (mostly because of the launch sequence and its two Event Dispatchers)
**Replaces:** `Source/CosmoChomp/Gameplay/CosmoRocket.h/.cpp`
**Creates:** `/Game/Blueprints/Gameplay/BP_Rocket` (parent: **Actor**)

## What the C++ does

- **Model:** the crashed rocket, `SM_Rocket` scaled to 520 units tall and standing on its base. It blocks everything.
- **Deposit zone:** a 420-unit sphere that overlaps pawns. Every tick (not just on entering), it takes all carried orbs from any in-play player standing in it. That way a player waiting by the rocket hands over new orbs as soon as they have any.
- **Each deposit:**
  1. Adds the orbs to `StoredOrbs`.
  2. Broadcasts `OnFuelChanged(StoredOrbs, RequiredOrbs)`.
  3. Plays `S_Whoosh`.
- **Launch:** when `StoredOrbs >= RequiredOrbs`:
  1. Tells the player to board the rocket.
  2. Turns off the rocket's collision and shows the exhaust flame: an engine cone pointing down, glowing orange with Intensity 20.
  3. Plays `S_RocketLaunch` and broadcasts `OnLaunched`. The game mode ends the round with a win.
- **Flight:** the rocket speeds up along its own up axis at 900 units/s². The flame flickers in length (`Z scale = 2 + 0.4·sin(40·t)`). After 6 seconds the rocket hides itself and stops ticking.

`RequiredOrbs` is set by the game mode to half the orbs in the maze.

## Components

```
Root (root)      Scene Component
├─ RocketMesh    Static Mesh SM_Rocket; Collision Preset BlockAll
├─ FlameMesh     Static Mesh /Engine/BasicShapes/Cone; No Collision; Cast Shadow ✗; Hidden in Game ✓
│                Location (0, 0, −40); Rotation (Pitch 180 — points down); Scale (1.2, 1.2, 2.0)
└─ DepositZone   Sphere Collision radius 420; Custom: Query Only, all Ignore, Pawn = Overlap
```

## Variables

| Name | Type | Default |
|---|---|---|
| `RequiredOrbs` | int | 50 (Instance Editable, Expose on Spawn) |
| `RocketHeight` | float | 520 |
| `LaunchDuration` | float | 6.0 |
| `LaunchAcceleration` | float | 900 |
| `DepositSound` | Sound Base | `S_Whoosh` |
| `LaunchSound` | Sound Base | `S_RocketLaunch` |
| `FlameMaterial` | Material Interface | `M_Glow` |
| `StoredOrbs` | int | 0 |
| `Launched` | bool | false |
| `LaunchTime` | float | 0 |
| `LaunchSpeed` | float | 0 |

**Event Dispatchers:**
- `OnFuelChanged` (`StoredOrbs`: int, `RequiredOrbs`: int)
- `OnLaunched` (no inputs)

**Pure functions** (the HUD and game mode use these):
- `GetStoredOrbs` → `StoredOrbs`
- `HasLaunched` → `Launched`

## Event graph

**Event BeginPlay:**
1. **BPFL_PlanetMath → FitStaticMesh** (`RocketMesh`, `RocketHeight`, StandOnBase **true**).
2. If `FlameMaterial` is valid:
   1. **Create Dynamic Material Instance** on `FlameMesh`.
   2. Set `Color` = (1, 0.45, 0.05) and `Intensity` = 20.

**Event Tick, while `Launched` is false:**
1. `DepositZone` → **Get Overlapping Actors** (no class filter).
2. For each actor → **DepositFrom** (Actor).
3. Stop. Nothing below runs until launch.

**Event Tick, once `Launched` is true:**
1. `LaunchTime += DeltaSeconds`.
2. `LaunchSpeed += LaunchAcceleration × DeltaSeconds`.
3. **Add Actor World Offset** (`Get Actor Up Vector × LaunchSpeed × DeltaSeconds`, no sweep).
4. `FlameMesh` → **Set Relative Scale 3D** `(1.2, 1.2, 2 + 0.4 × sin(LaunchTime × 40))`. Use the **Sin (Radians)** node.
5. If `LaunchTime >= LaunchDuration`:
   1. **Set Actor Tick Enabled** (false).
   2. **Set Actor Hidden In Game** (true).

**Function `DepositFrom(Player: Actor)`:**
1. Return if `Launched` is true, or if Player doesn't implement `BPI_CosmoPlayer`.
2. Return if **Is In Play** is false, or if **Get Carried Orbs** is 0 or less.
3. `StoredOrbs += Take All Orbs` (interface message).
4. **Call OnFuelChanged** (`StoredOrbs`, `RequiredOrbs`).
5. **Play Sound at Location** (`DepositSound`).
6. If `StoredOrbs >= RequiredOrbs` → **Launch** (Player).

**Function `Launch(Player: Actor)`:**
1. `Launched = true`.
2. **Board Rocket** (interface message to Player).
3. `RocketMesh` → **Set Collision Enabled** (No Collision).
4. `FlameMesh` → **Set Hidden in Game** (false).
5. **Play Sound at Location** (`LaunchSound`).
6. **Call OnLaunched**.

## Hooking it up

- The rocket goes into the round with the Blueprint game mode (step 8). The game mode spawns it, sets `RequiredOrbs`, and binds `OnLaunched`.
- The HUD's rocket lines (step 3) read **Get Stored Orbs** and `RequiredOrbs` from it once it's spawned there.

## Test

1. Place a `BP_Rocket` in `Lvl_BlueprintTest`, away from the C++ rocket.
2. In the Details panel, set its `RequiredOrbs` to 3.
3. Play, and pick up 3 orbs. If you placed `BP_EnergyOrb`s in step 4, use those.
4. Walk up to `BP_Rocket`:
   - The orbs are deposited (*Carrying* drops to 0) and you hear the whoosh.
   - The player disappears, and the rocket lifts off with a flickering orange flame.
   - It vanishes after 6 seconds.

The round won't restart, because the C++ game mode isn't listening to this rocket yet. That's expected until step 8.

## Done when

- [ ] Standing by the rocket deposits orbs continuously, not only when you first walk in.
- [ ] The launch hides the player, plays the sound and flies off like the C++ rocket.
- [ ] `OnFuelChanged` and `OnLaunched` fire. Check by binding them to **Print String** in the Level Blueprint.
