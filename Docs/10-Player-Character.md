# Step 10: Player character

**Difficulty:** Hard (the camera and movement frame on a sphere, the curved ray-gun trace, and the most gameplay rules of any class)
**Replaces:** `Source/CosmoChomp/CosmoChompCharacter.h/.cpp`, and the template-era `BP_ThirdPersonCharacter`
**Creates:** `/Game/Blueprints/Core/BP_Astronaut` (parent: **Character**)

Build this in four passes, and test after each one:
1. Components and movement
2. Camera and look
3. Gameplay rules
4. Ray gun firing

## What the C++ does

**Movement on a sphere:**
- The gravity component supplies the surface up and a drift-free surface frame (step 7).
- **Look input** isn't fed to the controller. It's accumulated in `LookYaw` (unlimited) and `LookPitch` (clamped to ±85°).
- **Every tick**, the control rotation is set to: the surface frame, turned by `LookYaw` around up, then tilted by `LookPitch` around the turned frame's right axis. The camera boom uses pawn control rotation, so the camera orbits the astronaut relative to the planet's surface.
- **Moving:** forward is the camera direction flattened onto the surface; right = `Cross(Up, Forward)`.
- **The body** turns toward its direction of travel when moving faster than 10. When stopped, it keeps its facing, re-flattened onto the surface. It turns smoothly (speed 10) without sweeping.

**Gameplay:**
- **Health:** `MaxHealth` 3, and 2 s of invulnerability after a hit, during which the mesh blinks (hidden for 0.1 s out of every 0.2 s).
- **Taking damage**, when in play, not invulnerable, and the damage is above 0:
  1. Health drops by `Max(Round(Damage), 1)`, and invulnerability starts.
  2. Plays `S_Ough`.
  3. If carrying orbs, the game mode scatters them all, the count resets to 0 and `S_Bang` plays.
  4. At 0 health, the astronaut dies.
- **Death:**
  1. Clears the ray gun.
  2. Stops and disables movement.
  3. Tips the mesh over (+90° roll on top of its base rotation).
  4. Calls the game mode's `HandlePlayerDeath`.
- **Board rocket:** clears the gun, stops and disables movement, then turns off collision and hides the actor.
- **"In play"** means not dead and not inside the rocket. Movement, jumping and firing only work in play.
- **Equip ray gun:** `TimeRemaining = Max(TimeRemaining, Duration)`, and `RayGunDuration` is set to the same value (for the HUD bar). Shows the held gun mesh. When the timer reaches 0, the mesh hides again.

**Animation and sound:**
- **Idle/walk:** plays `SK_AstronautWalk` while on the ground and moving faster than 40. Otherwise it plays `SK_AstronautIdle`.
- **Walk speed:** the walk play rate is `1.6 × Clamp(GroundSpeed / MaxWalkSpeed, 0.4, 1)`.
- **Footsteps:** a looping footstep sound plays while on the ground and moving faster than 150.

**Ray gun firing** (`IA_Fire` Triggered; at most once per 0.25 s; needs the gun and to be in play):
1. **Aim:** the camera direction flattened onto the surface. The astronaut snaps to face it.
2. **Muzzle:** `RayGunHolder` location + `Aim × 27.5` (half the held gun's size).
3. **Trace along the curve:** a straight trace would leave the curved ground, so the shot follows a great circle at muzzle height.
   1. `Center` = planet center; `ArcRadius = |Muzzle − Center|`.
   2. `ArcUp = (Muzzle − Center) / ArcRadius`; `ArcForward` = Aim flattened onto `ArcUp`.
   3. Make `N = Ceil(FireRange / 150)` segments (17 for the 2500 range).
   4. Point `i` = `Center + (ArcUp·cos(a) + ArcForward·sin(a)) × ArcRadius`, where `a = FireRange × i / N / ArcRadius`.
   5. Sweep a sphere of radius 20 between consecutive points, and stop at the first hit.
4. **Hit:** if the shot hit an alien, apply 1 damage to it.
5. **Effects:** spawn the beam through all the points, using pink `BeamColor`, and play `S_GunShoot` at the muzzle.

## Components

```
CapsuleComponent       radius 42, half height 96
├─ Mesh                SK_Astronaut; Animation Mode "Use Animation Asset"; transform from step 1 part A
├─ CameraBoom          Spring Arm; Target Arm Length 400; Use Pawn Control Rotation ✓; Inherit Pitch/Yaw/Roll ✓
│  └─ FollowCamera     Camera; Use Pawn Control Rotation ✗
├─ RayGunHolder        Scene; Location (45, 38, −5)
│  └─ RayGunMesh       Static Mesh SM_RayGun; No Collision; Hidden in Game ✓
├─ FootstepAudio       Audio; Sound S_Footsteps; Auto Activate ✗; Volume Multiplier 0.6
CharacterMovement      Orient Rotation to Movement ✗; Rotation Rate yaw 500; Jump Z Velocity 500;
                       Air Control 0.35; Max Walk Speed 500; Min Analog Walk Speed 20;
                       Braking Deceleration Walking 2000; Braking Deceleration Falling 1500
BPC_PlanetaryGravity
```

- **Class Defaults:** Use Controller Rotation Pitch/Yaw/Roll = all ✗.
- **Capsule:** leave the **RayGun** response at its default. The firing trace ignores the astronaut itself (Ignore Self), so the response doesn't matter.

## Variables

| Group | Variables (defaults) |
|---|---|
| Look | `LookYaw` 0, `LookPitch` 0, `LookYawRate` 1, `LookPitchRate` 1, `MaxLookPitch` 85, `BodyRotationInterpSpeed` 10 |
| Rules | `MaxHealth` 3, `InvulnerabilityDuration` 2, `Health`, `CarriedOrbs`, `InvulnerabilityRemaining`, `Dead`, `InRocket` |
| Ray gun | `FireInterval` 0.25, `FireRange` 2500, `FireDamage` 1, `BeamColor` (1, 0.1, 0.6), `RayGunHeldSize` 55, `RayGunTimeRemaining`, `RayGunDuration`, `FireCooldownRemaining` |
| Presentation | `IdleAnimation` `SK_AstronautIdle`, `WalkAnimation` `SK_AstronautWalk`, `WalkPlayRate` 1.6, `WalkSpeedThreshold` 40, `CurrentAnimation`, `MeshBaseLocation`, `MeshBaseRotation` |
| Sounds | `HurtSound` `S_Ough`, `DropOrbsSound` `S_Bang`, `ShootSound` `S_GunShoot` |
| Cached | `CurrentSurfaceUp` |

## Pass 1: Components and movement

**Event BeginPlay:**
1. `Health = MaxHealth`.
2. **Add Tick Prerequisite Component** (`BPC_PlanetaryGravity`).
3. **FitStaticMesh** (`RayGunMesh`, `RayGunHeldSize`, false).
4. **PlayAnimation** (`IdleAnimation`).
5. Store the Mesh's relative location and rotation in `MeshBaseLocation` and `MeshBaseRotation`.

**Input.** Add Enhanced Input Action events:
- **IA_Move** (Triggered, Vector2D) → **DoMove** (X = right, Y = forward).
- **IA_Look** and **IA_MouseLook** (Triggered, Vector2D) → **DoLook** (X, Y).
- **IA_Jump** Started → if **IsInPlay** → **Jump**. Completed → **Stop Jumping**.
- **IA_Fire** Triggered → **DoFire**.

The player controller already adds the mapping contexts (step 2).

**Function `DoMove(Right, Forward)`:**
1. Return if there's no controller, or if not **IsInPlay**.
2. `Up` = gravity component **Get Current Surface Up**.
3. `F` = **FlattenDirection** (**Get Control Rotation** → **Get Forward Vector**, Up). If it's zero, use `FlattenDirection(Actor Forward, Up)`.
4. `R = Normalize(Cross(Up, F))`.
5. **Add Movement Input** (F, Forward), then **Add Movement Input** (R, Right).

## Pass 2: Camera and look

**Function `DoLook(Yaw, Pitch)`:**
- `LookYaw += Yaw × LookYawRate`.
- `LookPitch = Clamp(LookPitch + Pitch × LookPitchRate, −MaxLookPitch, MaxLookPitch)`.

**Event Tick, part A: control rotation.** This replaces the C++ quaternion chain with vector rotations.
1. `U` = gravity component `FrameUp`; `F0` = `FrameForward`.
2. `F1` = **Rotate Vector Around Axis** (F0, `LookYaw`, U).
3. `Y` = `Cross(U, F1)` (the turned frame's right axis).
4. `F2` = **Rotate Vector Around Axis** (F1, `LookPitch`, Y). `U2` = **Rotate Vector Around Axis** (U, `LookPitch`, Y).
5. **Get Controller** → **Set Control Rotation** (**Make Rot from XZ** (F2, U2)).

The C++ rotates in the same direction for both angles. If mouse-up looks down in your version, negate `LookPitch` in step 4. Don't change it in the input asset, because the gamepad uses the same path.

**Event Tick, part B: body facing.**
1. `CurrentSurfaceUp` = **Get Current Surface Up**.
2. `Planar` = **Project Vector on to Plane** (Character Movement **Velocity**, `CurrentSurfaceUp`).
3. `Desired` = `Normalize(Planar)` if `Length(Planar) > 10`; otherwise `FlattenDirection(Actor Forward, Up)`, and if that's zero, `FlattenDirection((1, 0, 0), Up)`.
4. `Target` = **Make Rot from XZ** (Desired, `CurrentSurfaceUp`).
5. **Set Actor Rotation** (**Lerp (Rotator)** (Actor Rotation, Target, `Clamp(DeltaSeconds × BodyRotationInterpSpeed, 0, 1)`, Shortest Path ✓), Teleport ✓).

**Test pass 2:**
1. Set `BP_CosmoGameMode`'s Default Pawn Class to `BP_Astronaut`, then play.
2. Walk a full lap around the planet with the mouse untouched. Forward should stay "forward" the whole way, without the view slowly spinning.
3. Walk over the planet's poles: no flipping or snapping.
4. Walk on the underside: controls stay relative to the camera.

If any of these fail, the problem is in part A or in the gravity component's frame update (step 7).

## Pass 3: Gameplay rules

Implement **BPI_CosmoPlayer** (Class Settings → Interfaces). Each function reads or writes the matching variable:
- `IsInPlay` returns `NOT Dead AND NOT InRocket`.
- `HasRayGun` returns `RayGunTimeRemaining > 0`.
- `AddOrbs` adds `Max(Count, 0)`.
- `TakeAllOrbs` returns `CarriedOrbs` and sets it to 0.
- `EquipRayGun`:
  1. `RayGunTimeRemaining = Max(RayGunTimeRemaining, Duration)`.
  2. `RayGunDuration = RayGunTimeRemaining`.
  3. Show `RayGunMesh` (**Set Hidden in Game** false).
- `BoardRocket`:
  1. Set `InRocket`; set the gun time to 0.
  2. Character Movement → **Stop Movement Immediately**, then **Disable Movement**.
  3. **Set Actor Enable Collision** (false); **Set Actor Hidden In Game** (true).

**Event Tick, part C: `UpdateGameplay` (runs first in Tick):**
1. `FireCooldownRemaining −= DeltaSeconds`; `InvulnerabilityRemaining = Max(InvulnerabilityRemaining − DeltaSeconds, 0)`.
2. **Ray gun timer:** if `RayGunTimeRemaining > 0`, subtract DeltaSeconds. When it reaches 0 or below, clamp it to 0 and hide `RayGunMesh`.
3. If **IsInPlay**:
   1. **Blink:** `Hidden = InvulnerabilityRemaining > 0 AND (InvulnerabilityRemaining % 0.2) < 0.1`. Mesh → **Set Visibility** (`NOT Hidden`, Propagate to Children ✓).
   2. **Animation:** `GroundSpeed` = length of the velocity flattened onto `CurrentSurfaceUp`.
      - If **Is Moving On Ground** AND `GroundSpeed > WalkSpeedThreshold`: **PlayAnimation** (Walk), then Mesh → **Set Play Rate** (`WalkPlayRate × Clamp(GroundSpeed / MaxWalkSpeed, 0.4, 1)`).
      - Otherwise: **PlayAnimation** (Idle), then **Set Play Rate** (1).
4. **Footsteps:** `Walking = IsInPlay AND Is Moving On Ground AND GroundSpeed > 150`.
   - If `Walking` and `FootstepAudio` isn't playing → **Play**.
   - If not `Walking` and it is playing → **Stop**.

**Event AnyDamage:**
1. Return if not **IsInPlay**, if `InvulnerabilityRemaining > 0`, or if `Damage <= 0`.
2. `Health = Max(Health − Max(Round(Damage), 1), 0)`; `InvulnerabilityRemaining = InvulnerabilityDuration`.
3. **Play Sound at Location** (`HurtSound`).
4. If `CarriedOrbs > 0`:
   1. **Get Game Mode** → cast to `BP_CosmoGameMode` → **Scatter Orbs** (location, CarriedOrbs).
   2. `CarriedOrbs = 0`.
   3. Play `DropOrbsSound`.
5. If `Health <= 0` → **Die**.

**Function `Die`:**
1. `Dead = true`; set the gun time to 0 and hide `RayGunMesh`.
2. Stop and disable movement.
3. Mesh → **Set Visibility** (true, propagate), then **Set Relative Location and Rotation** (`MeshBaseLocation`, `MeshBaseRotation` + Roll 90).
4. Game mode → **Handle Player Death** (self).

## Pass 4: Ray gun firing

**Function `DoFire`:**
1. Return if not **IsInPlay**, if there's no gun, if `FireCooldownRemaining > 0`, or if there's no controller. Then set `FireCooldownRemaining = FireInterval`.
2. `Up` = **Get Current Surface Up**. `Aim` = **FlattenDirection** (Control Rotation forward, Up), falling back to the actor's forward. **Set Actor Rotation** (**Make Rot from XZ** (Aim, Up)).
3. `Center` = gravity component **Get Planet Center**. `Muzzle = RayGunHolder world location + Aim × RayGunHeldSize × 0.5`.
4. `ArcRadius = Max(Distance(Muzzle, Center), 1)`; `ArcUp = (Muzzle − Center) / ArcRadius`; `ArcForward` = **FlattenDirection** (Aim, ArcUp).
5. `Points` = [Muzzle]; `HitActor` = none; `N = Max(Ceil(FireRange / 150), 1)`.
6. **For Loop** `i = 1 … N` (use a **For Loop with Break**):
   1. `a = FireRange × i / N / ArcRadius` (radians).
   2. `P = Center + (ArcUp × cos(a) + ArcForward × sin(a)) × ArcRadius`. Use **Cos (Radians)** and **Sin (Radians)**.
   3. **Sphere Trace By Channel**: Start = last point, End = P, Radius 20, Trace Channel **RayGun**, Ignore Self ✓.
   4. If it hit: add the hit's **Location** to `Points`. That's the sphere's center at impact, which matches the C++; don't use **Impact Point**. Set `HitActor`, and **Break**.
   5. Otherwise add `P` to `Points`.
7. If `HitActor` is a `BP_Alien` → **Apply Damage** (HitActor, `FireDamage`, Get Controller, self).
8. **Spawn Actor** `BP_RayGunBeam` (identity transform, Always Spawn) → **SetPoints** (Points, `BeamColor`).
9. **Play Sound at Location** (`ShootSound`, Muzzle).

The **RayGun** trace channel (overview, setup step 5) is what lets the shot pass through orbs, the pickup, the rocket's deposit zone and the planet's gravity well, but stop at walls, the rocket body, aliens and the ground. If shots stop short, open **Show → Collision** in Play-In-Editor and look for any other trigger volume that blocks `RayGun`.

## Hooking it up

1. In `BP_CosmoGameMode`, set **Default Pawn Class** = `BP_Astronaut`.
2. Delete the temporary **Event AnyDamage** shim you added to `BP_ThirdPersonCharacter` in step 8.
3. Make sure no other Blueprint casts to `BP_ThirdPersonCharacter`. Everything should go through `BPI_CosmoPlayer`.

## Test

Play `Lvl_BlueprintTest`. Compare it side by side with `Lvl_CosmoChomp`:
- [ ] Walking, looking and jumping feel the same, including on the far side and over the poles.
- [ ] Idle and walk animations switch at the same speeds; footsteps play only while walking.
- [ ] Orbs are collected and deposited; the rocket launch hides the astronaut.
- [ ] A hit costs 1 health, makes the astronaut blink for 2 s, and scatters carried orbs.
- [ ] Dying topples the astronaut and ends the round.
- [ ] Firing draws the beam along the curve and stops at walls. Aliens hit by it shrink away, and new ones respawn 8 s later.
- [ ] The HUD's health, orb and ray gun numbers match what happens in play.

## Done when

- [ ] Every checkbox above passes.
- [ ] No Blueprint references `BP_ThirdPersonCharacter` or the C++ `CosmoChompCharacter` any more.
