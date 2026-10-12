# Step 9: Alien monster

**Difficulty:** Medium–Hard (a small state machine with tile-graph pathfinding; performance needs some care in Blueprints)
**Replaces:** `Source/CosmoChomp/Gameplay/AlienMonster.h/.cpp`
**Creates:** `/Game/Blueprints/Gameplay/BP_Alien` (parent: **Character**), `/Game/Blueprints/Gameplay/E_AlienState` (Enum)

## What the C++ does

Navmesh doesn't work on a sphere, so the aliens navigate the maze's **tile graph**. All maze queries go through the maze actor.

- **Setup:** a capsule of radius 50 and half height 90. The mesh is `SK_Alien` at height 190, yaw −90. Animations: `SK_AlienArmature_Idle`, `_Walk`, `_Running_attack`. Sounds: `S_ClawAttack` and `S_Slime`. It has the planetary gravity component, and an AIController possesses it so Character Movement runs.
- **States:** `Wander`, `Chase`, `Flee`, `Attack`, `Dead`.

**Every tick:**
1. **If Dead:** shrink to nothing over 0.6 s, then destroy.
2. `CurrentTile` = the tile nearest to the alien. Count down `AttackCooldownRemaining`.
3. **If Attacking:** run the attack timing and face the player. Skip the rest.
4. **Every `RepathInterval` (0.35 s), update the route.** Each alien starts at a random offset, so they don't all update on the same frame.
   1. **Player distances:** compute the walking distance (in tiles) from the player's tile to every tile.
   2. **Flee** if the player holds the ray gun: move to the open neighbor that's furthest from the player (ties broken randomly). Speed 300.
   3. **Chase** if the player is within `ChaseTileRange` (4) steps: move to the open neighbor that's one step closer. Speed 400, slower than the player's 500.
   4. **Wander** otherwise: pick a random goal tile, follow `FindPath` to it, and pick a new goal on arrival or if it's unreachable. Speed 260.
5. **Steer.**
   - **Chasing, on the player's tile:** head straight for the player.
   - **Otherwise:** follow the line from the current tile's center to the next tile's center. Aim at a point 0.35 tile-spacings ahead of the alien's progress along that line. This keeps it through the middle of each opening, clear of the corner posts.
   - **How steering works:** "steer" means *Add Movement Input* along the direction flattened onto the surface, after setting Max Walk Speed for the state. Within 15 units of the target, it stops.
6. **Attack** if not fleeing, the cooldown is over, and the player is within 170 units:
   1. Play the attack animation once, and the claw sound.
   2. After a 0.35 s windup, damage the player by 1 if they're still within `170 × 1.3`.
   3. 0.5 s after the windup, go back to Chase, with a 1.6 s cooldown and an immediate repath.
7. **Face:** the direction of travel, or the player while attacking. Turn using `OrientOwnerToSurface` at speed 8.
8. **Animate:** play the walk animation when moving faster than 30, otherwise idle. Only switch when the animation changes.

**On damage** (any amount, from the ray gun):
1. Die: set the state to Dead and turn off capsule collision.
2. Stop and disable movement.
3. Play the slime sound.
4. Broadcast `OnKilled(self)`.

## Enum `E_AlienState`

`Wander`, `Chase`, `Flee`, `Attack`, `Dead`

## `BP_Alien` setup

**Class Defaults:**
- **AI Controller Class** = `AIController`; **Auto Possess AI** = *Placed in World or Spawned*.
- Use Controller Rotation Pitch/Yaw/Roll = all ✗.
- **Character Movement:**
  - Orient Rotation to Movement ✗.
  - Max Walk Speed 260.
  - Braking Deceleration Walking 1500.

**Components:**
- **Capsule:** radius 50, half height 90. Make sure its **RayGun** channel response is **Block** (the default for new channels), so shots hit it.
- **Mesh:**
  - Skeletal Mesh `SK_Alien`, Collision **No Collision**.
  - Animation Mode **Use Animation Asset**.
  - Location, rotation and scale from the values you wrote down in step 1, part A.
- **Gravity:** add `BPC_PlanetaryGravity`.

## Variables

| Name | Type | Default |
|---|---|---|
| `ChaseTileRange` | int | 4 |
| `WanderSpeed` / `ChaseSpeed` / `FleeSpeed` | float | 260 / 400 / 300 |
| `RepathInterval` | float | 0.35 |
| `AttackRange` | float | 170 |
| `AttackDamage` | float | 1.0 |
| `AttackWindup` | float | 0.35 |
| `AttackCooldown` | float | 1.6 |
| `IdleAnimation`, `WalkAnimation`, `AttackAnimation` | Animation Asset | the three `SK_AlienArmature_*` assets |
| `AttackSound` / `DeathSound` | Sound Base | `S_ClawAttack` / `S_Slime` |
| `State` | `E_AlienState` | Wander |
| `Maze` | Actor (`BPI_Maze`) | — |
| `CurrentTile`, `NextTile`, `WanderGoal` | int | −1 |
| `RepathTimer`, `AttackCooldownRemaining`, `AttackPhase`, `DeathTimer` | float | 0 |
| `AttackLanded` | bool | false |
| `CurrentAnimation` | Animation Asset | — |

**Event Dispatcher:** `OnKilled` (`Alien`: `BP_Alien`).

**Pure function:** `GetState` → `State`. The mini map (step 12) uses it to hide dead aliens.

## Event graph

**Event BeginPlay:**
1. **Add Tick Prerequisite Component** (`BPC_PlanetaryGravity`).
2. `Maze` = first result of **Get All Actors with Interface** (`BPI_Maze`).
3. **PlayAnimation** (`IdleAnimation`, loop).
4. `RepathTimer` = **Random Float in Range** (0, `RepathInterval`).

**Function `GetPlayer` → Actor:** the player pawn, if it implements `BPI_CosmoPlayer` and **Is In Play**; otherwise none.

**Event Tick:** follow "Every tick" above, top to bottom. Split it into these functions to keep the graph readable:
- `UpdateRoute(Player)`
- `SteerTowards(Target, Speed)`
- `StartAttack`
- `TickAttack(DeltaSeconds, Player)`
- `UpdateFacing(DeltaSeconds, Player)`
- `PlayAnimation(Animation, Loop)`

Notes on each:
- **Dead branch:**
  1. `DeathTimer += DeltaSeconds`; `Alpha = Clamp(1 − DeathTimer / 0.6, 0, 1)`.
  2. **Set Actor Scale 3D** (`Max(Alpha, 0.01)`).
  3. If `Alpha <= 0` → **Destroy Actor**.
- **`SteerTowards`:**
  1. `Dir` = **Project Vector on to Plane** (`Target − ActorLocation`, gravity component's **Get Current Surface Up**).
  2. If `Length(Dir) < 15`, return.
  3. Set Character Movement **Max Walk Speed** = Speed.
  4. **Add Movement Input** (`Normalize(Dir)`, 1).
- **Follow-the-corridor target:**
  1. `From = GetTileLocation(CurrentTile, 0)`, `To = GetTileLocation(NextTile, 0)`, `Seg = To − From`.
  2. `Pos = ProjectToSurface(ActorLocation, 0)`.
  3. `Progress = Dot(Pos − From, Seg) / VectorLengthSquared(Seg)`.
  4. `Lookahead = 0.35 × GetTileSpacing / Length(Seg)`.
  5. `Along = Clamp(Progress + Lookahead, 0, 1)`; target = `From + Seg × Along`.
- **`UpdateRoute`:** translate the three branches directly. **Compute Distances** returns an `int` array; `−1` means unreachable.
- **`TickAttack`:** when `AttackPhase` first passes `AttackWindup`, set `AttackLanded` and **Apply Damage** to the player (Base Damage = `AttackDamage`, Event Instigator = **Get Controller**, Damage Causer = self).
- **`UpdateFacing`:** compute `Forward` as described, then call the gravity component's **OrientOwnerToSurface** (DeltaSeconds, Normalize(Forward), 8).
- **`PlayAnimation`:**
  1. If Animation ≠ `CurrentAnimation`, set `CurrentAnimation`.
  2. Mesh → **Play Animation** (Animation, Looping).

**Event AnyDamage:**
1. If State is Dead, ignore it.
2. Otherwise:
   1. Set State to Dead and reset `DeathTimer`.
   2. Capsule → **Set Collision Enabled** (No Collision).
   3. Character Movement → **Stop Movement Immediately**, then **Disable Movement**.
   4. **Play Sound at Location** (`DeathSound`).
   5. **Call OnKilled** (self).

## Performance in Blueprints

The C++ version does more work per tick than a Blueprint can comfortably afford with four aliens, so make these two changes:

1. **Nearest tile without scanning all 252 tiles.** The nearest tile can only change to a neighbor of the current one.
   1. Once `CurrentTile` is known, check only it and its **Get Neighbors**: pick whichever has the highest `Dot(Get Tile Up(tile), Normalize(ActorLocation − PlanetCenter))`.
   2. Do a full **Find Nearest Tile** only when `CurrentTile` is −1.
2. **One distance field shared by all aliens.** Each alien runs a full breadth-first search from the player every 0.35 s, which in C++ costs almost nothing.
   1. In `BP_CosmoGameMode`, add a timer every 0.3 s that stores `PlayerDistances` = **Compute Distances** (`GetPlayerTile`).
   2. Have `UpdateRoute` read the game mode's array instead of computing its own.

## Hooking it up

1. In `BP_CosmoGameMode`, set `AlienClass` = `BP_Alien` and `AlienHalfHeight` = 90.
2. Change the `OnKilled` binding in `SpawnAlien` to cast to `BP_Alien`.
3. The HUD mini map (step 12) and the character's ray gun (step 10) cast to `BP_Alien`, or call **Get State**.

**Known gap until step 10:** the C++ character only damages the C++ alien class, so you can't shoot `BP_Alien`s yet. Test wandering, chasing, fleeing and attacking now, and test shooting in step 10.

## Test

1. Play. Aliens appear after 4 s and wander through the corridors, staying centered in the openings, without catching on the corner posts.
2. Move within 4 tiles. An alien chases you, more slowly than you run.
3. Let it reach you. It plays the attack, the claw sound plays, and you lose 1 health unless you step back in time.
4. Pick up the ray gun. Nearby aliens turn and run.
5. Watch `stat game` while 4 aliens are active. Blueprint time should stay well under 1 ms.

## Done when

- [ ] All five states behave like the C++ alien.
- [ ] Aliens never get stuck on posts or walls during a 5-minute playtest.
- [ ] The game mode respawns them after `OnKilled` (test this in step 10, once shooting works).
