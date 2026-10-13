# Step 12: Mini map

**Difficulty:** Hard (projection math, and it redraws hundreds of lines every frame, which is the most performance-sensitive Blueprint in the game)
**Replaces:** `ACosmoChompHUD::DrawMiniMap` in `Source/CosmoChomp/CosmoChompHUD.cpp`
**Adds to:** `BP_CosmoHUD` (step 3)
**Creates:**
- `/Game/Blueprints/UI/M_HUDFlat` (Material)
- `/Game/Blueprints/UI/S_MapEdge` (Structure)

This comes after the maze (step 11) because it reads the maze's grid: tile corners, edges and passages. The C++ maze doesn't expose those to Blueprints.

## What the C++ does

**Placement and projection:**
- **Where:** a circular map in the top-right corner. Its diameter is `MiniMapSize` (0.34) × the screen's shorter side, capped so it never leaves the top-right quadrant. The margin is 3% of the shorter side.
- **Projection:** an **azimuthal equidistant projection** centered on the player:
  - Distance from the map's center = true great-circle distance over the planet.
  - Direction from the center = compass bearing, with **the camera's heading pointing up**.
  - `MiniMapRangeDegrees` (75) is the angle around the planet that reaches the rim. 180 would show the whole planet.

**What it draws, in order:**
1. A dark translucent disc.
2. Every tile edge, curved by splitting it into 4 pieces. Open passages are faint lines; walls are bright, thicker lines. Pieces outside the range are skipped. So are pieces that would jump more than 30% of the map's radius; those happen where the far side of the planet wraps around the rim.
3. A small green dot for each energy orb.
4. A yellow square for each alien that isn't dead.
5. A green triangle pointing sideways for the ray gun.
6. A red triangle pointing up for the rocket. The ray gun and rocket stay pinned to the rim, in their direction, when they're out of range.
7. A white arrow at the center for the player, pointing the way the astronaut faces.
8. A border ring.

The C++ batches all of this into one draw call of colored triangles. Blueprints can't build that batch, so they use the HUD's own draw nodes: **Draw Line**, **Draw Rect** and **Draw Material Triangle**.

## The projection

**Set up the view, once per frame:**
- `Up` = `Normalize(PlayerLocation − PlanetCenter)`.
- `Heading` = the camera's forward vector (**Get Player Camera Manager → Get Camera Rotation → Get Forward Vector**). If its flattened length is under 0.1, because the camera is looking straight down, use the camera's up vector instead.
- `Forward` = **FlattenDirection** (Heading, Up). If that's zero, use the player's flattened forward.
- `Right = Cross(Up, Forward)`.
- `Range` = `MiniMapRangeDegrees` in radians.
- `Origin` (the screen center of the map) and `MapRadius` (in pixels): see the variables below.

**Function `Project(Direction, ClampToRim)` → (bool Visible, Vector2D Screen).** `Direction` is a unit vector from the planet center.
1. `Cos = Clamp(Dot(Up, Direction), −1, 1)`; `Angle` = **Acos** (radians).
2. If `Angle > Range` and not `ClampToRim` → return not visible.
3. `Tangent = Direction − Up × Cos`.
4. `Bearing = Normalize2D(Dot(Tangent, Right), Dot(Tangent, Forward))`.
5. `Screen = Origin + (Bearing.X, −Bearing.Y) × Min(Angle / Range, 1) × MapRadius`. Screen Y points down, hence the minus.
6. Return visible.

**Function `ScreenDirection(WorldDir)` → Vector2D:** `Normalize2D(Dot(WorldDir, Right), −Dot(WorldDir, Forward))`.

## Assets

**`M_HUDFlat`:** a material that draws flat colored triangles.
1. Set **Material Domain** to *User Interface* and **Blend Mode** to *Translucent*.
2. Connect **Vertex Color** RGB to **Final Color**, and Vertex Color A to **Opacity**.

`Draw Material Triangle` passes each corner's color through as vertex color.

**`S_MapEdge`** (Structure): `CornerA` (int), `CornerB` (int), `IsWall` (bool).

## New variables on `BP_CosmoHUD`

| Name | Type | Default |
|---|---|---|
| `ShowMiniMap` | bool | true |
| `MiniMapSize` | float | 0.34 |
| `MiniMapRangeDegrees` | float | 75 |
| `MiniMapBackgroundColor` | Linear Color | (0.01, 0.02, 0.06, 0.78) |
| `MiniMapTileColor` | Linear Color | (0.35, 0.45, 0.7, 0.3) |
| `MiniMapWallColor` | Linear Color | (0.8, 0.88, 1.0, 0.95) |
| `MiniMapOrbColor` | Linear Color | (0.35, 1.0, 0.85, 1) |
| `MiniMapRocketColor` | Linear Color | (1.0, 0.12, 0.1, 1) |
| `MiniMapRayGunColor` | Linear Color | (0.2, 1.0, 0.25, 1) |
| `MiniMapAlienColor` | Linear Color | (1.0, 0.9, 0.1, 1) |
| `MiniMapPlayerColor` | Linear Color | white |
| `FlatMaterial` | Material Interface | `M_HUDFlat` |
| `MapEdges` | `S_MapEdge` [] | built once per round |
| `MapEdgesBuilt` | bool | false |

## Building the edge list once

The C++ loops over every tile and neighbor and looks up `EdgeCorners` every frame. In Blueprints, do that **once**. The maze doesn't change during a round, so the list stays valid.

The first time `DrawMiniMap` runs with a generated maze:
1. Cast the maze to `BP_HexMaze`.
2. For each tile, and each neighbor with a higher index, find the `EdgeCorners` entry. Add an `S_MapEdge` with `IsWall = NOT IsPassage`.
3. Set `MapEdgesBuilt`.

About 750 edges at frequency 5.

## `DrawMiniMap` (called from Receive Draw HUD, after the stats)

1. **Layout:**
   - `Shorter = Min(SizeX, SizeY)`, `Margin = 0.03 × Shorter`.
   - `MapRadius = Min(MiniMapSize × Shorter, 0.5 × Shorter − 2 × Margin) / 2`.
   - `Origin = (SizeX − Margin − MapRadius, Margin + MapRadius)`.
   - `Unit = MapRadius / 40` (marker sizes scale with the map).
2. **Background disc:** 64 calls to **Draw Material Triangle** (`FlatMaterial`) fanning out from `Origin`, with all three vertex colors set to the background color.
3. **Edges:** `CosRange = cos(Range)`. For each `S_MapEdge`:
   1. `A = Corners[CornerA]`, `B = Corners[CornerB]`.
   2. **Early out:** if both `Dot(Up, A)` and `Dot(Up, B)` are below `CosRange`, skip the edge. This rejects about two thirds of the edges before any trig.
   3. Split the edge into **2** pieces (the C++ uses 4; at a 75° range, 2 looks the same). Points: A, `Normalize(A + B)`, B.
   4. **Project** each point. For each consecutive pair where both are visible and less than `0.3 × MapRadius` apart, **Draw Line**:
      - walls: `MiniMapWallColor`, thickness `Max(0.6 × Unit, 1.5)`
      - passages: `MiniMapTileColor`, thickness `Max(0.3 × Unit, 1)`
4. **Orbs:** for each orb, if it's visible, **Draw Rect** a square of `0.9 × Unit` centered on its screen position.
   - Use the game mode's list of spawned orbs; add an array to `BP_CosmoGameMode`.
   - Don't call **Get All Actors of Class** every frame.
   - Skip entries that aren't **Is Valid**; collected orbs are destroyed.
5. **Aliens:** for each alien whose **Get State** ≠ Dead and that's visible, **Draw Rect** a `2 × Unit` square in `MiniMapAlienColor`. As with the orbs, read a list the game mode keeps: add the aliens to it in `SpawnAlien`, and skip entries that aren't valid.
6. **Ray gun:** if the game mode's `ActiveRayGun` is valid, **Project** with ClampToRim ✓. Draw **Arrow** (Screen, (1, 0), `1.6 × Unit`, `MiniMapRayGunColor`).
7. **Rocket:** **Project** with ClampToRim ✓. Draw **Arrow** (Screen, (0, −1), `1.9 × Unit`, `MiniMapRocketColor`).
8. **Player:** **Arrow** (`Origin`, **ScreenDirection** (pawn forward), `1.5 × Unit`, `MiniMapPlayerColor`).
9. **Rim:** 64 short **Draw Line** segments around `Origin` at `MapRadius`, in `MiniMapWallColor`, thickness `Max(0.5 × Unit, 1.5)`.

**Function `Arrow(Center, Dir, Size, Color)`:** one **Draw Material Triangle** (`FlatMaterial`, all three vertex colors = Color).
- `Side = (−Dir.Y, Dir.X)`.
- Vertices: `Center + Dir × Size`, `Center − Dir × 0.7·Size + Side × 0.8·Size`, `Center − Dir × 0.7·Size − Side × 0.8·Size`.

## Performance

Budget: under 1 ms of Blueprint time for the mini map. Check with `stat game` and Unreal Insights (**Trace → Blueprint** channel).

If it's over budget, apply these in order:
1. Draw edges with **1** piece (straight lines). At a 75° range the bending is small.
2. Only redraw the edge geometry every second frame. Store the projected endpoints in an array, and draw from the array on the other frames. Markers still update every frame.
3. Lower `MiniMapRangeDegrees` to 60.
4. Last resort: move only `DrawMiniMap`'s edge loop into a small C++ Blueprint Function Library (see the overview).

## Hooking it up

The mini map is part of `BP_CosmoHUD`, which `BP_CosmoGameMode` already uses. `Lvl_BlueprintTest` now has every feature of the shipping level.

## Test

1. Play `Lvl_BlueprintTest` and `Lvl_CosmoChomp` side by side (two PIE windows, or screenshots). The mini map sits in the same corner at the same size, and shows the same features.
2. Turn the camera. The map rotates so the view direction always points up.
3. Walk around. Walls on the map line up with walls around the astronaut.
4. Collected orbs disappear from the map; dead aliens disappear.
5. When the rocket is out of range, its red triangle sits on the rim, in the direction you'd walk to reach it.

## Done when

- [ ] The mini map matches the C++ version's behavior.
- [ ] It costs under 1 ms of Blueprint time per frame, with all aliens active.
