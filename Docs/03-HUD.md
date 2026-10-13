# Step 3: HUD: stats, banner and ray gun timer

**Difficulty:** Easy
**Replaces:** `Source/CosmoChomp/CosmoChompHUD.h/.cpp`, except `DrawMiniMap` (that's step 12)
**Creates:** `/Game/Blueprints/Core/BP_CosmoHUD` (parent: **HUD**)

The C++ HUD draws everything with Canvas calls in `DrawHUD`. The Blueprint HUD does the same in **Event Receive Draw HUD**, using the HUD's **Draw Text**, **Draw Rect** and **Get Text Size** nodes. Keeping it as a Canvas HUD, rather than a UMG widget, matches the C++ layout exactly and leaves room for the mini map in step 12.

## What the C++ draws

Everything scales with screen height: `Scale = Max(SizeY / 1080, 0.5) × 1.4`. The font is the engine's large font. If the HUD can't find a Cosmo Chomp game mode with a maze, it draws nothing.

| Element | Position | Content |
|---|---|---|
| Health | `X = 40·Scale`, `Y = 32·Scale` | `Health  {Health} / {MaxHealth}` in `TextColor` |
| Carrying | next line (+`34·Scale`) | `Carrying  {CarriedOrbs} orbs` |
| Rocket fuel | next line | `Rocket fuel  {Stored} / {Required}` |
| Fuel bar | next line | `300·Scale × 12·Scale`: a black 50% background, then a `FuelColor` fill (`Stored / Required`) |
| Ray gun timer | bottom center (only while holding the gun) | Panel with `RAY GUN  {ceil(seconds)}s`, the hint `Click / F / RT to fire` (right-aligned, 0.7× scale), and a 420·Scale × 16·Scale bar that drains with time left. It blinks between `RayGunColor` and `TextColor` every 0.2 s once fewer than `RayGunWarningTime` seconds remain. |
| Banner | centered, `Y = SizeY × 0.22` | The game mode's banner text at 1.4× scale, over a black 50% box padded 24 px sideways and 12 px up/down |

## Variables

| Name | Type | Default |
|---|---|---|
| `TextColor` | Linear Color | (0.9, 0.95, 1.0, 1) |
| `FuelColor` | Linear Color | (0.15, 1.0, 0.55, 1) |
| `RayGunColor` | Linear Color | (1.0, 0.2, 0.65, 1) |
| `RayGunWarningTime` | float | 3.0 |
| `HudFont` | Font | `/Engine/EngineFonts/Roboto`, or any font the team prefers |

Make the colors **Instance Editable**, like the C++ `EditAnywhere` properties.

## Functions

**`DrawBar(X, Y, Width, Height, Fill, Color)`:**
1. **Draw Rect** (black, alpha 0.5) at X, Y, W, H.
2. **Draw Rect** (`Color`) at X, Y, `Width × Clamp(Fill, 0, 1)`, H.

**`DrawRayGunTimer(Player, Scale, SizeX, SizeY)`.** `Player` is an Actor that implements `BPI_CosmoPlayer`.
1. `Remaining` = **Get Ray Gun Time Remaining** (interface message).
2. `Fill = Remaining / Max(Get Ray Gun Duration, 0.0001)`.
3. `Label` = **Format Text** `RAY GUN  {0}s` with **Ceil**(Remaining). `Hint` = `Click / F / RT to fire`.
4. `BarWidth = 420 × Scale`, `BarHeight = 16 × Scale`, `X = (SizeX − BarWidth) / 2`, `Y = SizeY − 130 × Scale`.
5. `Blinking = Remaining < RayGunWarningTime AND (Get Real Time Seconds % 0.4) < 0.2`. `Color = Blinking ? TextColor : RayGunColor` (a **Select** node).
6. **Get Text Size** (Label, Scale) → `LabelHeight`. `Padding = 12 × Scale`.
7. **Draw Rect** (black, 0.5): position `X − Padding, Y − Padding`, size `BarWidth + 2·Padding` × `LabelHeight + 8·Scale + BarHeight + 2·Padding`.
8. **Draw Text** Label at `(X, Y)` in `Color`.
9. **Get Text Size** (Hint, `0.7 × Scale`) → **Draw Text** Hint in `TextColor` at `(X + BarWidth − HintWidth, Y + (LabelHeight − HintHeight) / 2)`.
10. **DrawBar** at `(X, Y + LabelHeight + 8·Scale)`, size `BarWidth × BarHeight`, `Fill`, `Color`.

## Event Receive Draw HUD

1. **Get Game Mode** → cast to the game mode.
   - Until step 8, cast to `CosmoChompGameMode`, the C++ class; `BP_TestGameMode` is a child of it.
   - From step 8 on, cast to `BP_CosmoGameMode`.

   If the cast fails, or **Get Maze** isn't valid, stop drawing.
2. `Scale = Max(SizeY / 1080, 0.5) × 1.4`, `LineHeight = 34 × Scale`, `Left = 40 × Scale`, `Y = 32 × Scale` (a local variable).
3. **Get Owning Pawn** → if it **Does Implement Interface** `BPI_CosmoPlayer`:
   1. Draw the Health line, then add `LineHeight` to `Y`.
   2. Draw the Carrying line, then add `LineHeight` to `Y`.
4. **Get Rocket** (from the game mode). If valid:
   1. `Required = Max(RequiredOrbs, 1)`.
   2. Draw the fuel text, then add `LineHeight` to `Y`.
   3. **DrawBar** `(Left, Y, 300·Scale, 12·Scale, Stored / Required, FuelColor)`, then add `LineHeight` to `Y`.
5. If the pawn **Has Ray Gun** → **DrawRayGunTimer**.
6. `Banner` = **Get Banner Text** from the game mode. If not empty:
   1. **Get Text Size** at `1.4 × Scale` → W, H.
   2. `X = (SizeX − W) / 2`, `BannerY = SizeY × 0.22`.
   3. **Draw Rect** (black, 0.5) at `(X − 24, BannerY − 12)`, size `(W + 48, H + 24)`.
   4. **Draw Text** Banner at `(X, BannerY)`, scale `1.4 × Scale`.

Use **Format Text** for every label, so the numbers line up with the C++ wording.

## Hooking it up

In `BP_TestGameMode`, set **HUD Class** to `BP_CosmoHUD`.

The test level loses its mini map until step 12. The shipping level keeps the C++ HUD until cutover.

## Test

1. Play `Lvl_BlueprintTest`. Compare it side by side with `Lvl_CosmoChomp`: health, orbs and fuel text sit in the same place, at the same size.
2. Pick up the ray gun by the rocket. The bottom panel appears, counts down from 10 and blinks for the last 3 seconds.
3. The opening banner ("Collect …") shows for a few seconds and then disappears.
4. Resize the PIE window; everything scales with height.

## Done when

- [ ] Every element in the table above matches the C++ HUD's position, color and behavior.
- [ ] The HUD draws nothing in levels without a maze (open the default `Lvl_ThirdPerson` to check).
