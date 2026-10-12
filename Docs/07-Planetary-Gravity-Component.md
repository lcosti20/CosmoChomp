# Step 7: Planetary gravity component

**Difficulty:** Medium (short, but it's all 3D vector math, and both characters depend on it)
**Replaces:** `Source/CosmoChomp/PlanetaryGravityComponent.h/.cpp`
**Creates:** `/Game/Blueprints/Core/BPC_PlanetaryGravity` (Blueprint class, parent: **Actor Component**)

## What the C++ does

Any actor with this component sticks to the nearest planet and stays upright on its surface. Every tick it:

1. **Finds the planet.** It looks for actors tagged `Planet` within `GravitySearchRadius` (10,000) and keeps the closest one. `BP_Planet` has that tag. If none is in range, it keeps the previous planet.
2. **Works out "up":** `SurfaceUp = Normalize(OwnerLocation − PlanetCenter)`. If the owner is at the exact center, it falls back to the previous up, or world up.
3. **Updates a surface frame,** a rotation whose Z axis is the surface up and whose X axis is a "heading" that never drifts:
   - On the first tick: X = the owner's forward direction flattened onto the surface.
   - After that, the frame is turned by the *smallest* rotation that moves the old up onto the new up. Walking around the planet then keeps the same heading, instead of the camera slowly spinning (yaw drift).
4. **Sets gravity:** if `bDriveMovementComponentGravity` is on, it calls **Set Gravity Direction** (`−SurfaceUp`) on the owner's Character Movement component. That's what makes the characters fall toward the planet.
5. **Broadcasts** `OnSurfaceFrameUpdated(SurfaceUp, SurfaceOrientation)`.

**Other functions:**
- `GetCurrentSurfaceUp()` recomputes up from the owner's *current* location. Movement code calls it after the owner has moved this frame.
- `OrientOwnerToSurface(DeltaTime, DesiredForward, InterpSpeed)` smoothly rotates the owner so its Z is up and its X faces `DesiredForward`. It's non-sweeping, so collisions can't knock the rotation off.

## Blueprint differences

- **No quaternions.** The C++ stores the frame as a quaternion. In Blueprints, store it as two vectors, `FrameForward` and `FrameUp`, and build a rotator with **Make Rot from XZ** when you need one.

  For the "smallest rotation from old up to new up": re-flatten the old forward onto the new surface plane. The up direction only changes by a fraction of a degree per frame, so the result is the same, without yaw drift:

  `FrameForward = Normalize(Project Vector on to Plane(FrameForward, NewUp))`

- **Gravity is a direct call.** The C++ calls `SetGravityDirection` through reflection, to avoid depending on the Character class. In Blueprints, get the owner's **Character Movement** component and call **Set Gravity Direction** directly.

## Variables

| Name | Type | Default |
|---|---|---|
| `GravitySearchRadius` | float | 10000 |
| `DriveMovementComponentGravity` | bool | true |
| `CurrentPlanet` | Actor | — |
| `PlanetCenter` | Vector | (0, 0, 0) |
| `FrameForward` | Vector | (1, 0, 0) |
| `FrameUp` | Vector | (0, 0, 1) |
| `CurrentSurfaceUp` | Vector | (0, 0, 1) |
| `FrameInitialized` | bool | false |
| `CachedPlanets` | Actor [] | — |

**Event Dispatcher:** `OnSurfaceFrameUpdated` (`NewSurfaceUp`: Vector, `SurfaceRotation`: Rotator).

## Event graph

**Event BeginPlay:** **Get All Actors with Tag** (`Planet`) → `CachedPlanets`. This replaces the per-tick search the C++ does. The planet is placed in the level and never spawned, so caching it is safe.

**Event Tick:**

1. **Find the nearest planet:** loop over `CachedPlanets` and keep the closest one within `GravitySearchRadius`. Compare squared distances to skip the square roots. If you found one, set `CurrentPlanet` and `PlanetCenter` from it.
2. `NewUp = Normalize(Owner location − PlanetCenter)`. If it's nearly zero, use `FrameUp` if `FrameInitialized`, or `(0, 0, 1)` if not.
3. **Update the frame:**
   - If `FrameInitialized` is false:
     1. `FrameForward = FlattenDirection(Owner forward vector, NewUp)`. If that's zero, use `FlattenDirection((1, 0, 0), NewUp)`.
     2. Set `FrameInitialized` to true.
   - Otherwise: `FrameForward = FlattenDirection(FrameForward, NewUp)`.
4. `FrameUp = NewUp`, `CurrentSurfaceUp = NewUp`.
5. If `DriveMovementComponentGravity`:
   1. Get the owner's component by class **Character Movement Component**.
   2. If valid → **Set Gravity Direction** (`−NewUp`).
6. **Call OnSurfaceFrameUpdated** (`NewUp`, **Make Rot from XZ** (`FrameForward`, `FrameUp`)).

**Component ticking.** In **Class Defaults**, keep **Component Tick → Start with Tick Enabled** checked.

Each owner should tick this component **before** its own Tick, as the C++ effectively did. In the owner's **Event BeginPlay**, call **Add Tick Prerequisite Component** (Target: Self, PrerequisiteComponent: `BPC_PlanetaryGravity`). The owner's Tick then always sees this frame's up.

## Functions

- **`GetCurrentSurfaceUp`** (pure):
  1. `Up = Normalize(Owner location − PlanetCenter)`.
  2. If nearly zero → return `FrameUp` (if initialized) or `(0, 0, 1)`.
  3. Otherwise return `Up`.
- **`GetSurfaceRotation`** (pure): **Make Rot from XZ** (`FrameForward`, `FrameUp`).
- **`GetPlanetCenter`**, **`GetCurrentPlanet`** (pure): return the variables.
- **`OrientOwnerToSurface(DeltaTime, DesiredForward, InterpSpeed)`**:
  1. `Target = Make Rot from XZ (DesiredForward, CurrentSurfaceUp)`.
  2. `New = Lerp (Rotator)` (A = owner's **Get Actor Rotation**, B = `Target`, Alpha = `Clamp(DeltaTime × InterpSpeed, 0, 1)`, **Shortest Path ✓**). Shortest Path makes it slerp through quaternions, like C++ `QInterpTo`.
  3. Owner → **Set Actor Rotation** (`New`, **Teleport Physics ✓**). It doesn't sweep, matching the C++.

## Test

1. Make a test Blueprint `BP_GravityTest` (parent **Character**). Add `BPC_PlanetaryGravity`, and set **Auto Possess AI** to *Placed in World or Spawned*.
2. Place it in `Lvl_BlueprintTest` about 500 units above the planet's surface, over the side of the planet (for example, at `(3500, 0, 500)`).
3. Play. It should fall *sideways* in world space, onto the planet, and come to rest.
4. On Tick, add **Draw Debug Arrow** from the actor along `GetCurrentSurfaceUp × 200`. The arrow should point straight away from the planet center.

Delete `BP_GravityTest` afterwards.

## Done when

- [ ] Characters with the component fall toward the planet from any side.
- [ ] `GetCurrentSurfaceUp` and the frame vectors are correct, with no drift in `FrameForward` after walking a full lap of the planet (check this again in step 10).
