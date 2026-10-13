# Step 5: Ray gun pickup and beam

**Difficulty:** Easy–Medium
**Replaces:** `Source/CosmoChomp/Gameplay/RayGunPickup.h/.cpp` (both `ARayGunPickup` and `ARayGunBeam`)
**Creates:** `/Game/Blueprints/Gameplay/BP_RayGunPickup`, `/Game/Blueprints/Gameplay/BP_RayGunBeam` (both parent: **Actor**)

## What the C++ does

**`ARayGunPickup`** is the floating gun in the maze:
- **Gun model:** spins at 120°/s around its local up axis. It's sized so its longest side is 110 units, with its center on a pivot.
- **Beacon:** a pink glowing column (`M_Glow`, Intensity 6) of height 1600 and width 14 rises from the pickup, so it can be seen over the walls. A pink point light (intensity 8000, radius 450, no shadows) makes it glow.
- **Pickup:** a 110-unit sphere that overlaps pawns. When a player who is in play touches it:
  1. Calls `EquipRayGun(Duration = 10)` on the player.
  2. Plays `PickupSound` (`S_Whoosh`).
  3. Broadcasts `OnCollected`. The game mode uses this to schedule the next gun.
  4. Destroys itself.

**`ARayGunBeam`** is the short-lived laser drawn when the player fires:
- **Path:** the shot follows the planet's curve, so the beam is a chain of straight cylinder segments between the points the character passes in. All segments are instances of one Instanced Static Mesh. The material is `M_Glow` with `Color` = beam color and Intensity 25.
- **Flashes:** two point lights, one at the muzzle and one where the beam stops. Each starts at `FlashIntensity` (20000) with a 500 radius and no shadows.
- **Fade:** over `Lifetime` (0.3 s), the segments get thinner and the lights dim in step. Then the beam destroys itself.

## `BP_RayGunPickup`

### Components

```
PickupSphere (root)  Sphere Collision, radius 110; Custom collision: Query Only, all Ignore, Pawn = Overlap
├─ Pivot             Scene Component
│  └─ GunMesh        Static Mesh SM_RayGun; No Collision
├─ Beacon            Static Mesh /Engine/BasicShapes/Cylinder; No Collision; Cast Shadow ✗
└─ Glow              Point Light; Intensity 8000; Attenuation Radius 450; Cast Shadows ✗
Spinner              Rotating Movement; Rotation Rate (0, 0, 120) yaw; Updated Component = Pivot
```

To make `Spinner` turn only the gun: in **Event BeginPlay**, call `Spinner` → **Set Updated Component** (`Pivot`). If you skip this, it spins the whole actor, beacon included.

### Variables

| Name | Type | Default |
|---|---|---|
| `Duration` | float | 10.0 |
| `DisplaySize` | float | 110.0 |
| `PickupSound` | Sound Base | `/Game/Audio/S_Whoosh` |
| `BeaconHeight` | float | 1600.0 |
| `BeaconWidth` | float | 14.0 |
| `BeaconColor` | Linear Color | (1.0, 0.1, 0.6, 1) |
| `BeaconMaterial` | Material Interface | `/Game/Materials/M_Glow` |

Add an **Event Dispatcher** `OnCollected`, with no inputs.

### Event graph

**Event BeginPlay:**
1. `Spinner` → **Set Updated Component** (`Pivot`).
2. **BPFL_PlanetMath → FitStaticMesh** (`GunMesh`, `DisplaySize`, StandOnBase false).
3. `Beacon` → **Set Relative Location** `(0, 0, BeaconHeight / 2)`. The engine cylinder is 100 units tall, with its pivot at its center.
4. `Beacon` → **Set Relative Scale 3D** `(BeaconWidth / 100, BeaconWidth / 100, BeaconHeight / 100)`.
5. **Create Dynamic Material Instance** on `Beacon` from `BeaconMaterial`, then set `Color` = `BeaconColor` and `Intensity` = 6.
6. `Glow` → **Set Light Color** (`BeaconColor`).

**`PickupSphere` → On Component Begin Overlap:**
1. Other Actor implements `BPI_CosmoPlayer` AND **Is In Play** → otherwise stop.
2. **Equip Ray Gun** (`Duration`).
3. **Play Sound at Location** (`PickupSound`).
4. **Call OnCollected**.
5. **Destroy Actor**.

## `BP_RayGunBeam`

### Components

```
Segments (root)  Instanced Static Mesh, /Engine/BasicShapes/Cylinder; Mobility Movable; No Collision; Cast Shadow ✗
├─ MuzzleFlash   Point Light; Attenuation Radius 500; Cast Shadows ✗
└─ ImpactFlash   Point Light; Attenuation Radius 500; Cast Shadows ✗
```

### Variables

| Name | Type | Default |
|---|---|---|
| `BeamWidth` | float | 16.0 |
| `Lifetime` | float | 0.3 |
| `FlashIntensity` | float | 20000 |
| `BeamMaterial` | Material Interface | `M_Glow` |
| `Age` | float | 0 |

### Function `SetPoints(Points: Vector[], Color: Linear Color)`

1. If `BeamMaterial` is valid:
   1. **Create Dynamic Material Instance** on `Segments`.
   2. Set `Color` = Color and `Intensity` = 25.
2. If `Points` isn't empty:
   1. `MuzzleFlash` → **Set World Location** (`Points[0]`).
   2. `ImpactFlash` → **Set World Location** (last point).
3. For both lights: **Set Light Color** (Color), **Set Intensity** (`FlashIntensity`).
4. **For Loop** `Index` from 1 to `Points.Length − 1`:
   1. `Start = Points[Index − 1]`, `End = Points[Index]`, `Length = Distance(Start, End)`. Skip the segment if it's nearly 0.
   2. `Rotation = Make Rot from Z (End − Start)`.
   3. `Scale = (BeamWidth / 100, BeamWidth / 100, Length / 100)`.
   4. `Segments` → **Add Instance** (Transform: Location `(Start + End) / 2`, Rotation, Scale; **World Space ✓**).

### Event Tick

1. `Age += DeltaSeconds`. `Alpha = 1 − Age / Lifetime`.
2. If `Alpha <= 0` → **Destroy Actor**, and stop.
3. Both lights → **Set Intensity** (`FlashIntensity × Alpha`).
4. For each instance index (`0` to **Get Instance Count** − 1):
   1. **Get Instance Transform** (World Space ✓).
   2. Set its scale to `(BeamWidth / 100 × Alpha, BeamWidth / 100 × Alpha, current Z)`.
   3. **Update Instance Transform**: World Space ✓; **Mark Render State Dirty** only on the last index.

The character spawns the beam (step 10) with **Spawn Actor from Class** at the world origin, using collision handling **Always Spawn**, and then calls `SetPoints`.

## Hooking it up

- `BP_RayGunPickup` joins the round when the Blueprint game mode spawns it (step 8).
- `BP_RayGunBeam` is used once the Blueprint character fires it (step 10).

## Test

1. Drag a `BP_RayGunPickup` into `Lvl_BlueprintTest`, near the start position. It spins, and the pink column and glow are visible from across the maze.
2. Walk into it. The HUD's ray gun timer starts at 10, and the C++ character can fire.
3. To test the beam on its own, add a temporary key event in the Level Blueprint (for example, **B**):
   1. **Spawn Actor** `BP_RayGunBeam`.
   2. Call `SetPoints` with three points: the player's location, then two points 300 and 600 units in front of the player, each raised 100 units.
   3. Use pink `(1, 0.1, 0.6)` as the color.

   A thick pink beam with flashes at both ends should fade out over 0.3 s. Delete the test event afterwards.

## Done when

- [ ] The pickup looks and behaves like the C++ one, and fires `OnCollected` once.
- [ ] The beam draws, flashes and fades like the C++ one.
