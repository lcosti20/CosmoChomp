# Step 1: Shared helpers and mesh fitting

**Difficulty:** Easy
**Replaces:** `Source/CosmoChomp/Gameplay/CosmoChompMeshFitting.h`
**Creates:** `/Game/Blueprints/Library/BPFL_PlanetMath` (Blueprint Function Library)

Every later step uses a few small pieces of vector math over and over. This step puts them in one Blueprint Function Library, so the later graphs stay short. It also replaces the C++ mesh-fitting helpers.

## What the C++ does

The Sketchfab models were imported at inconsistent scales, so `CosmoChompMeshFitting.h` sizes them at runtime:

- **`FitStaticMesh(Component, Size, bStandOnBase)`**
  1. Scales a static mesh component so the mesh's largest dimension equals `Size`.
  2. Moves it so the mesh's center sits at the component's origin. If `bStandOnBase` is set, the bottom-center sits there instead.

  Used by: the rocket (520 tall, standing), the ray gun pickup (110 long, centered) and the held ray gun (55 long, centered).

- **`FitSkeletalMeshToCapsule(Mesh, Capsule, Height, Yaw)`**
  1. Scales a skeletal mesh to `Height`.
  2. Stands its feet on the bottom of the capsule.
  3. Turns it by `Yaw`, because the models face +Y instead of +X.

  Used by: the astronaut (175 tall, yaw −90) and the alien (190 tall, yaw −90).

## Part A: Bake the skeletal mesh transforms (no Blueprint code)

The astronaut and alien always use the same size, so you don't need runtime code for them. Copy the values the C++ calculates:

1. Open `Lvl_CosmoChomp` and press **Play**.
2. Press **F8** to eject from the player. Select the player character in the **Outliner**. Then select its **Mesh** (`CharacterMesh0`) component.
3. Write down the component's **Location**, **Rotation** and **Scale**.
4. Do the same for one `AlienMonster`.

In steps 9 and 10, you'll type these values into the Mesh component of the new Blueprints.

## Part B: Create `BPFL_PlanetMath`

Create a **Blueprint Function Library** at `/Game/Blueprints/Library/BPFL_PlanetMath`. Mark every function **Pure** unless it says otherwise.

### `FitStaticMesh` (not pure)

**Inputs:** `Component` (Static Mesh Component), `Size` (float), `StandOnBase` (bool)

1. **Get Static Mesh** (Component). If it isn't valid, return.
2. **Get Bounding Box** (Static Mesh) → `Min`, `Max`.
3. `MeshSize = Max − Min`.
4. `Scale = Size / Max(MeshSize.X, MeshSize.Y, MeshSize.Z, 0.0001)`.
5. `Pivot = (Min + Max) / 2`. If `StandOnBase`, set `Pivot.Z = Min.Z`.
6. **Set Relative Scale 3D** = `(Scale, Scale, Scale)`.
7. **Set Relative Location** = **Rotate Vector** (`−Pivot × Scale`, by **Get Relative Rotation** of the Component).

### `TangentBasis`

Replaces C++ `FVector::FindBestAxisVectors`, which has no Blueprint node.

**Inputs:** `Up` (Vector)
**Outputs:** `Tangent`, `Bitangent` (Vector)

1. `Helper = (0, 0, 1)` if `Abs(Up.Z) < 0.9`, else `(1, 0, 0)` (a **Select** node).
2. `Tangent = Normalize(Cross(Helper, Up))`.
3. `Bitangent = Cross(Up, Tangent)`.

### `SurfaceUp`

**Inputs:** `Location`, `PlanetCenter` (Vector)
**Output:** `Up` (Vector)

`Normalize(Location − PlanetCenter)`. If that's nearly zero, return `(0, 0, 1)`.

### `SurfaceRotation`

Copy of C++ `AHexMaze::GetSurfaceRotation`.

**Inputs:** `Up`, `Forward` (Vector)
**Output:** Rotator

1. `T = Normalize(Project Vector on to Plane(Forward, Up))`.
2. If `T` is nearly zero, use `Project Vector on to Plane((1, 0, 0), Up)`. If that's also zero, use `(0, 1, 0)`.
3. Return **Make Rot from XZ** (`X = T`, `Z = Up`).

### `FlattenDirection`

**Inputs:** `Direction`, `Up` (Vector)
**Output:** Vector

`Normalize(Project Vector on to Plane(Direction, Up))`. Used everywhere movement is kept along the surface.

### `MakeEdgeKey`

**Inputs:** `A`, `B` (int)
**Output:** int

`Min(A, B) × 10000 + Max(A, B)`. Used by the maze (step 11) and the mini map (step 12) as an order-independent key for the edge between two tiles.

## Test

1. In the test level's Level Blueprint, on **Event BeginPlay**, spawn a **Static Mesh Actor** with `SM_Rocket`.
2. Call `FitStaticMesh` (Size 520, StandOnBase true) on it.
3. It should stand on its spawn point and be 5.2 m tall.

Delete the test nodes afterwards.

## Done when

- [ ] Astronaut and alien mesh transforms are written down.
- [ ] `BPFL_PlanetMath` compiles with `FitStaticMesh`, `TangentBasis`, `SurfaceUp`, `SurfaceRotation`, `FlattenDirection` and `MakeEdgeKey`.
