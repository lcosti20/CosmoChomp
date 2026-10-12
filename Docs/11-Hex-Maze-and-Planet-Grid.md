# Step 11: Hex maze and planet grid

**Difficulty:** Hard (the most code and the most math in the project, plus the heaviest loops for Blueprints)
**Replaces:** `Source/CosmoChomp/Maze/HexMaze.h/.cpp`, `Source/CosmoChomp/Maze/HexPlanetGrid.h/.cpp`
**Creates:**
- `/Game/Blueprints/Maze/S_HexTile` (Structure)
- `/Game/Blueprints/Maze/BP_HexMaze` (parent: **Actor**; implements `BPI_Maze`)

## What the C++ does

### The grid (`FHexPlanetGrid`)

The planet's surface is tiled with hexagons, plus exactly 12 pentagons, like a soccer ball. This shape is called a *Goldberg polyhedron*. It's built from a subdivided icosahedron:

1. **Icosahedron:** 12 vertices from the golden ratio `φ = (1 + √5) / 2`, and 20 triangular faces. Both tables are in `HexPlanetGrid.cpp`; copy them exactly.
2. **Geodesic subdivision** at `Frequency` f: on each face with corners A, B, C:
   1. For every `i = 0…f` and `j = 0…f−i`, make the point `Normalize(A·(1−u−v) + B·u + C·v)`, where `u = i/f` and `v = j/f`.
   2. Points on shared face edges come up more than once, so merge duplicates (within 1e‑5).
   3. Join the points into small triangles: `(i,j) (i+1,j) (i,j+1)`, plus `(i+1,j) (i+1,j+1) (i,j+1)` whenever `i + j < f − 1`.
3. **Every vertex becomes a tile center.** There are `10·f² + 2` tiles; at the game's frequency 5, that's 252.
4. **Every small triangle becomes a tile corner.** The corner's direction is the normalized average of the triangle's three vertices.
5. **Adjacency:** each triangle side connects two tiles as neighbors. The two triangles that share a side are the two corners at the ends of the wall between those tiles (`EdgeCorners`).
6. **Winding order:** each tile's neighbors and corners are sorted by angle around its center, so they wind in a consistent order.

Everything is stored on the unit sphere. Multiply by the planet radius to place things.

### The maze (`AHexMaze`)

**`BuildGrid`:**
1. **Measure the planet:** find the nearest actor tagged `Planet`. Measure its radius from components that **block Pawn**. This skips `BP_Planet`'s much larger GravityWell sphere.
2. **Pick the frequency:** `Clamp(Round(Radius × 1.10714872 / TargetTileSize), 1, 24)`. `1.10714872` is the angle of one icosahedron edge in radians; `TargetTileSize` is 650.
3. **Build** the grid at that frequency.

**`GenerateMaze(StartTile)`:**
1. **Seed:** create a random stream from `Seed`, or from a random number if `Seed` is 0.
2. **Carve** with a randomized depth-first search from `StartTile`:
   1. Push the start tile onto a stack.
   2. Look at the top tile. If it has unvisited neighbors, pick one at random, mark the edge as a passage, and push it.
   3. If it doesn't, pop it.
   4. Repeat until the stack is empty.
3. **Braid:** loop over the tiles in random order (a Fisher–Yates shuffle). For each dead end (exactly 1 open neighbor), with probability `BraidChance` (0.85), open one of its walls. Prefer walls into another dead end, which removes two dead ends with one opening. This adds the loops a Pac-Man-style chase needs.
4. **Open the start tile** to all its neighbors, to make a plaza for the rocket.
5. **Build the walls:**
   - **Wall pieces:** for every edge that isn't a passage, place a wall mesh, chosen at random from `WallMeshes` (`SM_Wall_04`, `SM_Wall_05`). Stretch it to fit the edge:
     - **Length:** the distance between the two corners.
     - **Thickness:** 40.
     - **Height:** 220 + 40, sunk 40 units into the ground so the curve never leaves a gap.
   - **Posts:** put a post (`SM_Wall_03`, 75 × 75, 15 taller than the walls) on every corner that touches a wall.
   - **Instancing:** all pieces are instances of Instanced Static Mesh components (Static mobility, BlockAll). There's one component per mesh.

**Queries:** these are the `BPI_Maze` functions; see the overview.
- `FindNearestTile` picks the tile whose center has the largest dot product with the direction to the location.
- `ComputeDistances` is a breadth-first search through passages.
- `FindPath` walks "downhill" on the distance field from the destination.

**Editor helpers:**
- **Preview In Editor:** generates walls in the editor.
- **Clear Preview:** removes them.
- **BeginPlay** clears any preview, because the game mode generates the real maze. Preview components are transient, so they're never saved into the level.

## Structure `S_HexTile`

| Field | Type |
|---|---|
| `Center` | Vector (unit direction) |
| `Neighbors` | int [] |
| `Corners` | int [] |

Blueprint arrays can't hold arrays directly, but an array of structs that contain arrays works fine.

## `BP_HexMaze` variables

| Name | Type | Default |
|---|---|---|
| `AutoDetectPlanet` | bool | true |
| `PlanetRadius` | float | 3000 |
| `TargetTileSize` | float | 650 |
| `BraidChance` | float | 0.85 |
| `Seed` | int | 0 |
| `WallMeshes` | Static Mesh [] | `SM_Wall_04`, `SM_Wall_05` |
| `PostMesh` | Static Mesh | `SM_Wall_03` |
| `WallHeight`, `WallThickness`, `PostWidth`, `WallSink` | float | 220, 40, 75, 40 |
| `Tiles` | `S_HexTile` [] | — |
| `Corners` | Vector [] | — |
| `EdgeCorners` | Map: int → IntPoint | key = `MakeEdgeKey(A, B)`, value = the two corner indices |
| `Passages` | Set of int | edge keys |
| `PlanetCenter` | Vector | — |
| `Radius` | float | — |
| `Frequency` | int | — |
| `WallComponents` | Instanced Static Mesh Component [] | — |
| `PostComponent` | Instanced Static Mesh Component | — |

**Root component:** a Scene Component with Mobility **Static**.

## Building the grid in Blueprints

Write `BuildHexGrid(Frequency)` as its own function. Keep it to these steps.

1. **Tables:** store the 12 icosahedron vertices and the 20 faces as `Vector` and `IntVector` arrays, as variable defaults on the Blueprint, copied from `HexPlanetGrid.cpp`. Typing them by hand is tedious but one-off.
2. **Merging duplicates.** Don't port `FindOrAddVertex` as a loop over all vertices. That's about 100,000 comparisons, which is slow in Blueprints and can trip the infinite-loop limit. Instead, keep a **Map: String → int**:
   1. For each new point, build a key by rounding X, Y and Z to 4 decimals. Use **To String (Float)** after multiplying by 10,000 and **Round**ing, then append the three parts.
   2. Use **Find** on the map. If the key is there, reuse its index. Otherwise add the point and record its index.
3. **Face grid:** a local `int` array of size `(f+1)²`, indexed by `i × (f+1) + j`. Reset it to −1 for each face, then fill it as you create points.
4. **Triangles:** collect them in an `IntVector` array.
5. **Tiles, corners and adjacency:**
   1. Create one `S_HexTile` per vertex, with `Center` set to the vertex.
   2. For each triangle `t`, add its normalized average to `Corners`.
   3. For each of its three sides `(V0, V1)`:
      - Add `t` to tile V0's Corners and V1 to V0's Neighbors, both with **Add Unique**.
      - Add V0 to tile V1's Neighbors, with **Add Unique**.
      - Record `t` against `MakeEdgeKey(V0, V1)` in a temporary **Map: int → IntPoint**: the first triangle found goes in X, the second in Y.

   Structs in Blueprint arrays are copies. **Get (a copy)** the tile, change it, then **Set Array Elem** it back. Forgetting this is the most common bug in this step.
6. **Edge corners:** copy every entry of the temporary map that has both X and Y set into `EdgeCorners`.
7. **Sorting by angle.** Blueprints have no sort-with-comparator node. Each tile has at most 6 entries, so a simple insertion sort in a helper function is fine:
   1. Get the tile's `T`, `B` from **TangentBasis** (`Center`).
   2. Each entry's angle is **Atan2** (`Dot(P − Center, B)`, `Dot(P − Center, T)`), where P is the neighbor's center or the corner's direction.
   3. Sort the entries by that angle.

**Iteration budget:** this whole function runs about 5,000 loop iterations at frequency 5. It's fine at the default **Maximum Loop Iteration Count**. If the team lowers `TargetTileSize` or makes the planet bigger, so the frequency goes above about 12, raise that limit (Project Settings → Engine → General Settings).

**Timing:** it runs once per round, before play starts. Even at 50–100 ms it's hidden by the level load.

## Generating the maze in Blueprints

- **Random stream:** **Make Random Stream** (`Seed`, or **Random Integer** (2147483647) when it's 0). Pass it into each function by reference.
- **`CarvePassages(StartTile)`:** an `int` array as the stack, a `bool` array for "visited", and a **While Loop** that runs while the stack isn't empty. The C++ is already iterative, so translate it line for line.
- **`BraidDeadEnds`:**
  1. Fisher–Yates shuffle: for `Index` from the last element down to 1, swap `Order[Index]` with `Order[Random Integer from Stream(Index + 1)]`.
  2. Then translate the rest of the loop directly. **Random Float from Stream** replaces `Stream.FRand()`.
- **`BuildWalls`:**
  1. **ClearWalls**.
  2. For each wall mesh: **Add Instanced Static Mesh Component**. Then set Static Mesh = the mesh, Collision Preset = BlockAll, Mobility = Static, and add it to `WallComponents`. Do the same once for `PostMesh`.
  3. Keep a `bool` array `CornerHasWall` with one entry per corner.
  4. For each tile, and for each neighbor with a higher index that isn't a passage, look up `EdgeCorners` → `CornerA`, `CornerB`. Then:
     1. `PA = Corners[CornerA] × Radius`, `PB = Corners[CornerB] × Radius`.
     2. `Up = Normalize(PA + PB)`; `Along = FlattenDirection(PB − PA, Up)`; `Rotation = Make Rot from XZ (Along, Up)`.
     3. `Base = PlanetCenter + Up × (Radius − WallSink)`.
     4. `Size = (Distance(PA, PB), WallThickness, WallHeight + WallSink)`.
     5. Pick a random mesh index from the stream. **Add Instance** to that component, with the transform from **FitMeshTransform** (below) and **World Space ✓**.
     6. Mark both corners in `CornerHasWall`.
  5. For every marked corner, add a post at `PlanetCenter + Corner × (Radius − WallSink)`. Rotation is **Make Rot from Z** (Corner); size is `(PostWidth, PostWidth, WallHeight + WallSink + 15)`.
- **`FitMeshTransform(Mesh, Base, Rotation, Size)` → Transform:**
  1. **Get Bounding Box** → Min, Max. `MeshSize = Max − Min` (each axis at least 0.0001). `Scale = Size / MeshSize`, per axis.
  2. `BottomCenter = ((Min.X + Max.X) / 2, (Min.Y + Max.Y) / 2, Min.Z)`.
  3. Return Location `Base − Rotate Vector(BottomCenter × Scale, Rotation)`, with that Rotation and Scale.
- **`ClearWalls`:** **Destroy Component** on every entry in `WallComponents` and on `PostComponent`, then clear the references.
- **`BuildGrid`:**
  1. Find the nearest `Planet`-tagged actor, then **Get Components by Class** (Primitive Component) on it.
  2. For each component that has collision enabled and whose **Get Collision Response to Channel** (Pawn) is **Block**, take the smallest axis of **Get Bounds → Box Extent**. Keep the largest of these values as the radius.
  3. Fall back to `PlanetRadius`, and **Print String** a warning, if nothing qualifies.
  4. Set `Frequency`, then **BuildHexGrid**, and clear `Passages`.

**Queries.** Implement every `BPI_Maze` function on `BP_HexMaze`. Each one is a few nodes:
- **`GetTileSpacing`:** `Radius × 1.10714872 / Frequency`.
- **`ComputeDistances`:**
  1. Fill an `int` array with −1. Use a second `int` array as the queue, with a `Head` index; don't remove items from the front, because that's slow.
  2. While `Head < Queue length`: for each neighbor that's still −1 and **IsPassage**, set its distance to the current tile's + 1 and add it to the queue.
- **`IsPassage`:** `Passages` **Contains** `MakeEdgeKey(A, B)`.

**Editor preview.** Add two **Custom Events**, `PreviewInEditor` and `ClearPreview`, and tick **Call In Editor** on both:
- `PreviewInEditor` → **BuildGrid** → **GenerateMaze** (**FindNearestTile** (`PlanetCenter + (0, 0, Radius)`)).
- `ClearPreview` → **ClearWalls**.

Components added with **Add … Component** in the editor aren't saved with the level, which matches the C++ "transient" behavior. Check this: after a preview, save the level, reload it, and confirm the preview walls are gone.

**Event BeginPlay** → **ClearWalls**. The game mode builds the real maze one tick later (step 8).

## Hooking it up

1. In `Lvl_BlueprintTest`, delete `BP_MazeAdapter` and place `BP_HexMaze` at the origin.
2. The game mode, aliens and HUD all use `BPI_Maze`, so nothing else needs to change.
3. Delete `BP_MazeAdapter`.

## Test

1. In the editor, select `BP_HexMaze` and click **Preview In Editor**. A maze appears around the planet.
2. Compare it with **Preview In Editor** on the C++ `HexMaze` in `Lvl_CosmoChomp`, using the same nonzero `Seed`. The layouts won't match exactly, because the random number streams differ. But the tile count (252), wall height and corridor width should match, and there should be no gaps at posts or the 12 pentagons.
3. Play several rounds. Every orb is reachable, and the start plaza is open on all sides.
4. Aliens path correctly with no "stuck" behavior. A bad `EdgeCorners` or `Passages` entry shows up as aliens walking into walls.
5. `stat game` during round start: note the one-off spike, and make sure it stays under the level-load time.

## Done when

- [ ] `BP_HexMaze` builds a 252-tile maze with braided loops, walls and posts that match the C++ look.
- [ ] All `BPI_Maze` queries return the same kinds of values as the C++. Spot-check `GetNumTiles`, `GetTileSpacing` (about 664) and `ComputeDistances` from the start tile.
- [ ] Editor preview works, and doesn't save into the level.
