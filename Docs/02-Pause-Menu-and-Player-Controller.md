# Step 2: Pause menu and player controller

**Difficulty:** Easy
**Replaces:** `Source/CosmoChomp/CosmoChompPauseMenu.h/.cpp`, `Source/CosmoChomp/CosmoChompPlayerController.h/.cpp`
**Creates:** `/Game/Blueprints/UI/WBP_PauseMenu` (Widget Blueprint), `/Game/Blueprints/Core/BP_CosmoPlayerController` (parent: **PlayerController**)

This is the easiest conversion. The C++ builds its widget tree in code only because there was no Widget Blueprint; in Blueprints you just lay the menu out in the UMG designer.

## What the C++ does

**Pause menu (`UCosmoChompPauseMenu`):**
- **Layout:** a full-screen black border at 65% opacity, with a centered "Paused" title (size 56) above **Continue** and **Quit** buttons (size 30 text).
- **Keys:** Escape, P, gamepad Start and gamepad B (face button right) also mean Continue.
- **Focus:** when the menu opens, keyboard and gamepad focus goes to the Continue button.

**Player controller (`ACosmoChompPlayerController`):**
- **Input mapping contexts:** at startup, it adds the `DefaultMappingContexts` array. It also adds `MobileExcludedMappingContexts`, unless touch controls are in use. On mobile, it creates `MobileControlsWidgetClass`.
- **Pause key:** binds `IA_Pause` (Started) to `TogglePauseMenu`.
- **`OpenPauseMenu`:**
  1. Creates the menu once and adds it to the viewport at Z-order 100.
  2. Pauses the game and shows the cursor.
  3. Sets the input mode to *Game and UI* (cursor not hidden during capture).
  4. Focuses Continue.
- **`ClosePauseMenu`:** removes the menu, unpauses, hides the cursor and sets the input mode to *Game Only*.
- **`QuitGame`:** calls **Quit Game**.

`IA_Pause` already has **Trigger when Paused** checked. That setting is what lets the same key close the menu while the game is paused. Keep it checked.

## `WBP_PauseMenu`

Create a **Widget Blueprint** at `/Game/Blueprints/UI/WBP_PauseMenu`.

**Designer hierarchy:**

```
[Border] Background         Brush Color (0, 0, 0, 0.65); H/V Align: Center; Fill the screen
  [Vertical Box] Menu
    [Text] Title            "Paused", Font Size 56, slot H Align Center, Padding bottom 30
    [Button] ContinueButton slot H Align Fill, Padding (0, 8)
      [Text] "Continue"     Font Size 30, slot Padding (80, 12)
    [Button] QuitButton     slot H Align Fill, Padding (0, 8)
      [Text] "Quit"         Font Size 30, slot Padding (80, 12)
```

- Check **Is Variable** on both buttons.
- In **Class Defaults**, check **Is Focusable**.

**Event graph:**

- **ContinueButton → On Clicked** → **Get Owning Player** → **Cast To BP_CosmoPlayerController** → **Close Pause Menu**.
- **QuitButton → On Clicked** → same cast → **Quit Game** (the controller's function).
- Add a function **FocusFirstButton**: **Set Keyboard Focus** on `ContinueButton`.

**Key handling.** Override the **On Key Down** function:
1. **Get Key** (from the Key Event).
2. Check whether it equals Escape, P, Gamepad Special Right or Gamepad Face Button Right. Use **Equal (Key)** nodes joined with **OR**.
3. If it matches: call the same Close Pause Menu path as Continue, and return **Handled**.
4. Otherwise: return **Unhandled**.

## `BP_CosmoPlayerController`

Create a Blueprint at `/Game/Blueprints/Core/BP_CosmoPlayerController`, parent **PlayerController**.

**Variables:**

| Name | Type | Default |
|---|---|---|
| `DefaultMappingContexts` | Input Mapping Context [] | `IMC_Default` |
| `MobileExcludedMappingContexts` | Input Mapping Context [] | `IMC_MouseLook` |
| `MobileControlsWidgetClass` | User Widget class | same as on `BP_ThirdPersonPlayerController` |
| `ForceTouchControls` | bool | false |
| `PauseMenuClass` | `WBP_PauseMenu` class | `WBP_PauseMenu` |
| `PauseMenu` | `WBP_PauseMenu` reference | — |

To find the current values, open `BP_ThirdPersonPlayerController` and look at its **Class Defaults**. Copy them across.

**Event BeginPlay** (only on the local controller: check **Is Local Player Controller**):
1. **Get Enhanced Input Local Player Subsystem** → **For Each** `DefaultMappingContexts` → **Add Mapping Context** (Priority 0).
2. Work out whether touch controls are in use: `ForceTouchControls` OR the platform shows a touch interface. Use the **Get Platform Name** node to test for Android/iOS, or skip mobile support if the team doesn't ship there.
3. If touch controls are **not** in use → add each `MobileExcludedMappingContexts` the same way.
4. If touch controls **are** in use → **Create Widget** (`MobileControlsWidgetClass`) → **Add to Player Screen**.

**Input:** add an **Enhanced Input Action IA_Pause** event → from **Started** → call **Toggle Pause Menu**.

**Functions:**

- **TogglePauseMenu:** if `PauseMenu` is valid AND **Is In Viewport** → **Close Pause Menu**; otherwise → **Open Pause Menu**.
- **OpenPauseMenu:**
  1. If `PauseMenu` is valid and already in the viewport, return.
  2. If `PauseMenu` isn't valid → **Create Widget** (`PauseMenuClass`, Owning Player = self) → set `PauseMenu`.
  3. **Add to Viewport** (Z-order 100).
  4. **Set Game Paused** (true).
  5. **Set Show Mouse Cursor** (true).
  6. **Set Input Mode Game and UI**: Widget to Focus = `PauseMenu`, Hide Cursor During Capture = false.
  7. `PauseMenu` → **Focus First Button**.
- **ClosePauseMenu:**
  1. If `PauseMenu` is valid → **Remove from Parent**.
  2. **Set Game Paused** (false).
  3. **Set Show Mouse Cursor** (false).
  4. **Set Input Mode Game Only**.
- **QuitGame:** **Quit Game** (Specific Player = self, Quit Preference = Quit, Ignore Platform Restrictions = false).

## Hooking it up

1. In `BP_TestGameMode`, set **Player Controller Class** to `BP_CosmoPlayerController`.
2. Optional: this class doesn't depend on any other Cosmo Chomp class, so it can go live right away. Set the same thing in `BP_ThirdPersonGameMode` to use it in the shipping level too.

## Test

In `Lvl_BlueprintTest`:

1. Press **Play**, then press **Escape**. The game freezes and the menu appears with the cursor.
2. Press **Escape** again; the game resumes.
3. Repeat with **P**, and with gamepad **Start**.
4. Click **Continue**; it resumes.
5. Click **Quit**; Play-In-Editor ends.
6. Walk and look around; WASD and the mouse still work (the mapping contexts were added).

## Done when

- [ ] Pause opens and closes with Escape, P and Start, and from the Continue button.
- [ ] Quit ends the game.
- [ ] Movement, look and fire input still work.
