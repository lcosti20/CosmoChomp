# Step 13: Cutover — removing the C++

**Difficulty:** Medium (no new logic, but many small settings across the project, and a few that are easy to miss)

At this point `Lvl_BlueprintTest` plays the whole game on Blueprints. This step switches the shipping level over and removes the C++ module. After it, the project is Blueprint-only: no Visual Studio needed, and no compile step.

**Work on a branch, and commit before starting.** Several of these actions (deleting `Source/`, fixing up redirectors) are hard to undo by hand.

## 1. Switch the shipping level and project defaults

1. **Swap the maze actor.** Open `Lvl_CosmoChomp`. Delete the `HexMaze` actor and place `BP_HexMaze` at the origin. Copy over any non-default settings, such as `Seed` and wall sizes.
2. **Set the level's game mode.** In **World Settings**, set **GameMode Override** to `BP_CosmoGameMode`, or clear it and rely on the project default below.
3. **Change the project default game mode.** In **Project Settings → Maps & Modes**, set **Default GameMode** to `BP_CosmoGameMode`. This changes `GlobalDefaultGameMode` in `Config/DefaultEngine.ini`.
4. **Play `Lvl_CosmoChomp` end to end.** Win once and lose once. Then delete `Lvl_BlueprintTest`, or keep it as a sandbox.

## 2. Remove leftovers that point at C++ classes

- **Old ThirdPerson Blueprints.** `BP_ThirdPersonCharacter`, `BP_ThirdPersonGameMode` and `BP_ThirdPersonPlayerController` (in `/Game/ThirdPerson/Blueprints`) are children of the C++ classes. Delete them; right-click → **Reference Viewer** first to confirm nothing uses them. If the team wants to keep the stock `Lvl_ThirdPerson` map working, reparent them to **Character**, **GameModeBase** and **PlayerController** instead (**Class Settings → Parent Class**).
- **Template variants.** `/Game/Variant_Combat`, `/Game/Variant_Platforming` and `/Game/Variant_SideScrolling` are Unreal template samples. Cosmo Chomp doesn't use them, but their Blueprints are built on the C++ in `Source/CosmoChomp/Variant_*`. Delete these three content folders along with the code.
- **Redirectors.** Right-click `/Game` → **Fix Up Redirectors**.
- **Check for remaining references.** Run **Tools → Audit → Asset Audit**, or search the Content Browser for `/Script/CosmoChomp`. Nothing should still reference the module. Every hit is an asset whose parent class or component is a C++ type.

## 3. Remove the C++ module

**Close the editor first.**

1. **Delete the code:** delete `Source/`, and the `Binaries/` and `Intermediate/` folders at the project root.
2. **Edit `CosmoChomp.uproject`:** delete the whole `"Modules": [ … ]` block. Keep `"Plugins"` as it is.
3. **Edit `Config/DefaultEngine.ini`:** remove the lines that redirect the old template names to the C++ module:
   ```ini
   +ActiveGameNameRedirects=(OldGameName="TP_ThirdPerson",NewGameName="/Script/CosmoChomp")
   +ActiveGameNameRedirects=(OldGameName="/Script/TP_ThirdPerson",NewGameName="/Script/CosmoChomp")
   +ActiveClassRedirects=(OldClassName="TP_ThirdPersonPlayerController",NewClassName="CosmoChompPlayerController")
   +ActiveClassRedirects=(OldClassName="TP_ThirdPersonGameMode",NewClassName="CosmoChompGameMode")
   +ActiveClassRedirects=(OldClassName="TP_ThirdPersonCharacter",NewClassName="CosmoChompCharacter")
   ```
4. **Delete the solution file:** if a `CosmoChomp.sln` exists at the project root, delete it.
5. **Reopen the project** by double-clicking `CosmoChomp.uproject`. It should open straight away, without asking to rebuild modules.
   - If it reports missing classes, an asset still references the C++. Revert, find the asset with step 2's search, fix it, and try again.

**Keep:** `Config/DefaultEditorSettings.ini`, which moves the editor's Stop shortcut so Escape reaches the pause menu. It's an editor setting, not code.

## 4. Update the tooling

- **`Tools/AssetPipeline/ue_create_level.py`** spawns the C++ maze with `spawn(unreal.HexMaze, …)`. Change it to load the Blueprint class:
  ```python
  maze_class = unreal.load_asset("/Game/Blueprints/Maze/BP_HexMaze").generated_class()
  maze = spawn(maze_class, label="HexMaze")
  ```
  Also update the docstring, which says the C++ module must be built.
- **`Tools/AssetPipeline/ue_import.py`** creates the input actions, materials and imported meshes the Blueprints use. It has no C++ dependency, so it needs no change.
- **`.github/workflows/main.yml`** runs `RunUAT` BuildCookRun to build, cook and package. That works for Blueprint-only projects too; there's just no compile step. Run the workflow once to confirm packaging still succeeds.
- **`README.md`:**
  - Replace the source tree layout and the C++ class names in the gameplay section with the `/Game/Blueprints/` layout.
  - Remove any build-the-C++ instructions.
  - Point to these docs for how the conversion was done.

## 5. Final checks

- [ ] `CosmoChomp.uproject` opens without a compile prompt, on a machine without Visual Studio.
- [ ] A packaged Win64 build (via the workflow or **Platforms → Windows → Package Project**) launches into `Lvl_CosmoChomp` and plays a full round.
- [ ] In the packaged build:
  - pause works with Escape, P and gamepad Start;
  - Quit closes the game;
  - the mini map, ray gun timer and banners all show.
- [ ] No asset references `/Script/CosmoChomp`.
- [ ] The team has re-run the playtest checklist from steps 8–12 on the shipping level.
