# Step 4: Energy orb

**Difficulty:** Easy
**Replaces:** `Source/CosmoChomp/Gameplay/EnergyOrb.h/.cpp`
**Creates:** `/Game/Blueprints/Gameplay/BP_EnergyOrb` (parent: **Actor**)

## What the C++ does

- **Looks:** a glowing green sphere 36 units across, using `M_Glow` with its `Color` parameter set to `GlowColor`. It doesn't cast a shadow.
- **Pickup:** has a 70-unit pickup sphere that only overlaps pawns. When a player who is "in play" (alive and not inside the rocket) overlaps it:
  1. Gives the player `Value` orbs.
  2. Plays `CollectSound`, if one is set.
  3. Destroys itself.
- **Pickup delay:** `InitialPickupDelay`, set when the orb is spawned, disables pickup for that many seconds. Orbs the player drops when hurt use this, so they aren't instantly picked back up. When the delay ends, it re-checks overlaps, so a player already standing on the orb collects it right away.

## Components

| Component | Type | Settings |
|---|---|---|
| `PickupSphere` (root) | Sphere Collision | Sphere Radius 70; Collision Preset **Custom**: Collision Enabled = Query Only, Object Type = WorldDynamic, all responses **Ignore** except **Pawn = Overlap**; Generate Overlap Events ✓ |
| `OrbMesh` | Static Mesh | `/Engine/BasicShapes/Sphere`; Collision **No Collision**; Cast Shadow ✗ |

## Variables

| Name | Type | Default | Flags |
|---|---|---|---|
| `Value` | int | 1 | Instance Editable |
| `OrbSize` | float | 36.0 | Instance Editable |
| `GlowColor` | Linear Color | (0.15, 1.0, 0.55, 1) | Instance Editable |
| `GlowMaterial` | Material Interface | `/Game/Materials/M_Glow` | Instance Editable |
| `CollectSound` | Sound Base | none | Instance Editable |
| `InitialPickupDelay` | float | 0.0 | Instance Editable, **Expose on Spawn** |

## Event graph

**Event BeginPlay:**
1. `OrbMesh` → **Set Relative Scale 3D** = `OrbSize / 100` on all axes. The engine sphere is 100 units across.
2. If `GlowMaterial` is valid:
   1. **Create Dynamic Material Instance** (OrbMesh, element 0, `GlowMaterial`).
   2. **Set Vector Parameter Value** `Color` = `GlowColor`.
3. If `InitialPickupDelay > 0` → **SetPickupDelay** (`InitialPickupDelay`).

**Function `SetPickupDelay(Seconds)`:**
1. `PickupSphere` → **Set Generate Overlap Events** (false).
2. **Set Timer by Event** (`Seconds`, not looping) → custom event **EnablePickup**.

**Custom event `EnablePickup`:**
1. `PickupSphere` → **Set Generate Overlap Events** (true).
2. **Get Overlapping Actors** (PickupSphere) → for each → **TryCollect**. This stands in for C++ `UpdateOverlaps`, which Blueprints can't call.

**`PickupSphere` → On Component Begin Overlap** → **TryCollect** (Other Actor).

**Function `TryCollect(OtherActor)`:**
1. If `OtherActor` doesn't implement `BPI_CosmoPlayer`, return.
2. If **Is In Play** (interface message) is false, return.
3. **Add Orbs** (`Value`).
4. If `CollectSound` is valid → **Play Sound at Location** (actor location).
5. **Destroy Actor**.

Using the interface means the orb already works with the current C++ astronaut, through `BP_ThirdPersonCharacter`. Nothing has to change when the astronaut becomes a Blueprint in step 10.

## Hooking it up

The C++ game mode can only spawn the C++ orb class, so `BP_EnergyOrb` goes into the game when the Blueprint game mode spawns it (step 8). Until then, test it by hand.

## Test

1. Drag five `BP_EnergyOrb`s into `Lvl_BlueprintTest`, next to the start position.
2. Play and walk through them. The HUD's *Carrying* count goes up by 1 for each.
3. Set `InitialPickupDelay` to 3 on one placed orb. Stand on it at the start; it gets collected after 3 seconds without moving.

## Done when

- [ ] Orbs look the same as the C++ ones: size, color, no shadow.
- [ ] Walking through an orb collects it once.
- [ ] The pickup delay works, including when you're already standing on the orb.
