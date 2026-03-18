# CLAUDE.md — SMS Cape Powerup Mod

## Project Overview

BetterSunshineEngine (BSE) Kuribo module that adds a cape powerup to Super Mario Sunshine (NTSC-U / GMSE01). Cape activates on triple jump, gives SMW-style dive/swoop flight.

**GitHub:** https://github.com/ryanbevins/sms-cape-powerup

## Build Commands

```bash
# First time only (or after CMake changes):
cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=targets/GCNKuriboClangRelease.cmake

# Build:
ninja -C build
# Output: build/CapePowerup.kxe
```

## Deploy & Test

See `BUILD_AND_DEPLOY.md` for full setup. Quick rebuild cycle:

```bash
ninja -C build
# Copy .kxe to extracted ISO (use Python for the Kuribo! path):
python -c "import shutil, pathlib; shutil.copy('build/CapePowerup.kxe', pathlib.Path(r'C:\Users\ryana\documents\sms-cape-test\root\files\Kuribo!\Mods\_CapePowerup.kxe'))"
# Rebuild ISO:
python -m pyisotools C:\Users\ryana\documents\sms-cape-test\root B --dest C:\Users\ryana\documents\sms-cape-modded.iso
# Open sms-cape-modded.iso in Dolphin
```

## Architecture

- **Framework:** BetterSunshineEngine + Kuribo mod loader
- **Compiler:** Clang PowerPC cross-compiler (bundled in BSE at `lib/BetterSunshineEngine/compiler/`)
- **BSE version:** Must use commit `deca478` — matches the v4.0.0 prebuilt release. Latest main branch will NOT work with the prebuilt BetterSunshineEngine.kxe.
- **Region:** NTSC-U (`SMS_REGION us` in toolchain)
- **Flight runs in player update callback**, NOT via BSE's custom state machine (BSE state callbacks only fire once, not per-frame)
- Cross-compiles to PowerPC with `-Os`, no exceptions, no RTTI, no stdlib — cannot use standard library containers or dynamic allocation patterns
- `lib/BetterSunshineEngine` is a git submodule — don't modify files there directly
- Static variables in `cape_state.cpp` track flight state (takeoff timer, vertical speed, roll). File-scoped singletons since SMS is single-player.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Module entry, BSE callbacks, flight activation logic |
| `src/cape_state.cpp` | All flight physics (takeoff, dive/swoop, collision) |
| `src/cape_timer.cpp` | 60-second timer, FLUDD store/restore |
| `src/cape_data.cpp` | CapeData getter/init via BSE's registerData |
| `src/cape_box.cpp` | CapeBox pickup object (registered but not placed in levels yet) |
| `src/cape_render.cpp` | Visual stub (fade timer logic, no model yet) |
| `include/cape_data.hxx` | CapeData struct, all constants, physics tuning values, button masks |
| `include/cape_state.hxx` | Flight function declarations |
| `include/cape_timer.hxx` | Timer function declarations |
| `include/cape_box.hxx` | TCapeBox class |

## SMS Decomp Reference

The companion decomp project at `C:\Users\ryana\documents\sms` provides decompiled source for understanding SMS internals. Key reference files:
- `src/Player/MarioMove.cpp` — Mario state machine, jump logic, `checkStickRotate()` (spin jump detection, line 3888)
- `src/Player/MarioInit.cpp` — Default physics values: gravity=0.5, jumpPow=42, maxSpeed=32
- `src/Player/MarioDraw.cpp` — Animation table (`marioAnimeFiles[199]` at line 36), animation IDs
- `include/Player/MarioMain.hpp` — TMario class layout with all field offsets
- `include/System/MarioGamePad.hpp` — TMarioControllerWork struct, button enum

## Critical Knowledge (Lessons Learned the Hard Way)

### BSE Module Setup
- `ModuleInfo` REQUIRES a `SettingsGroup` pointer, NOT `nullptr`. Passing nullptr crashes on boot (black screen).
- BSE submodule must be on commit `deca478` to match the v4.0.0 prebuilt `.kxe`. Wrong commit = black screen.
- Module filename must start with `_` (e.g., `_CapePowerup.kxe`) to load after BSE.

### Controller Input — THE MOST IMPORTANT THING
- **`TMarioControllerWork::mStickH/V` range is approximately -122 to 122, NOT -1 to 1.** Always normalize: `stickY / 122.0f`. Failing to normalize makes all stick-based physics ~100x too strong. This caused spinning, instant stalls, and death-plunge dives.
- `mInput` = currently held buttons. `mFrameInput` = newly pressed this frame.
- Button masks: R=0x20, A=0x100, B=0x200, L=0x4000 (from `TMarioControllerWork::Buttons` enum).
- For raw gamepad input (D-pad), use `player->mController->mButtons.mInput` (JUTGamePad). D-pad: UP=0x8, DOWN=0x4, RIGHT=0x2, LEFT=0x1.

### State Flags (from `player->mState`)
- `& 0x800` = airborne (`STATE_AIRBORN`)
- `& 0x2000` = in water (`STATE_WATERBORN`)
- `(& 0xFFF) == 0x882` = triple jump (`STATE_TRIPLE_J`)
- `(& 0xFFF) == 0x890/0x895/0x896` = spin jump
- `(& 0xFFF) == 0x441` = spin

### BSE Angle Convention
- SMS uses `s16` angle units: 0–65535 = 0°–360°
- Convert: `degrees * (32768.0f / 180.0f)` → s16, or `s16 * (180.0f / 32768.0f)` → degrees

### Flight Implementation Details
- **BSE's `Player::registerStateMachine()` callback only fires once**, not per-frame. All flight logic must run in `Player::addUpdateCallback()` instead.
- **The game's air physics fight `mSpeed` values.** We set `mSpeed` every frame; the game applies its own air decel between frames. Works because we overwrite, but values feel slightly dampened vs what we set.
- **Do NOT directly update `mTranslation`** — bypasses collision, causes insane velocities and geometry clipping.
- **Writing `mAngle.y` for yaw rotation works** after stick input is properly normalized.
- **Roll banking** via `mAngle.z` — smoothly interpolated, must be zeroed on flight exit and every frame when not gliding.
- **Prevent spin jump during flight** by zeroing `player->_534` (stick rotation history counter) every frame. `checkStickRotate()` at decomp line 3888 needs all 4 quadrants visited — clearing the counter prevents this.

### Timer
- BSE's player update callback runs at ~60hz (not 30hz like game logic). Timer decrement of `1.0f/30.0f` per call means the timer runs ~2x fast. `CAPE_TIMER_DURATION = 120.0f` gives ~60 real seconds.

### Animation
- `player->setAnimation(id, speed)` — must be called every frame during flight to prevent game from overriding with its own animation.
- 0x4C = fall (arms spread) — current placeholder for flight pose.
- Full animation table: `src/Player/MarioDraw.cpp` line 36, 199 entries indexed from 0.
- Transition from triple jump spin to flight animation at takeoff frame 60.

### Collision
- `player->mFloorBelow` = ground height below Mario (f32, offset 0xEC)
- `player->mWallTriangle` = non-null when touching wall (const TBGCheckData*, offset 0xD8)
- `player->mRoofTriangle` = non-null when touching ceiling (offset 0xDC)
- `player->mWaterHeight` = water surface at Mario's position (offset 0xF0, -10000 when no water)
- Ground collision needs 60-frame grace period after takeoff to avoid instant exit.

## Current Physics Constants (in cape_state.cpp)

| Constant | Value | Notes |
|----------|-------|-------|
| TAKEOFF_FRAMES | 90 | ~3 sec rise |
| TAKEOFF_RISE_SPEED | 42.0 | Matches SMS jump power |
| TAKEOFF_FORWARD_SPEED | 32.0 | Matches SMS max run speed |
| FLIGHT_GRAVITY | 0.25 | Half of SMS normal (0.5) |
| DRAG | 0.02 | Very gentle speed decay |
| MAX_SPEED | 70.0 | Forward speed cap |
| STALL_SPEED | 3.0 | Flight ends below this |
| DIVE_DOWN_FORCE | 5.0 | Extra downward force diving |
| DIVE_SPEED_GAIN | 1.2 | Forward speed gained/frame diving |
| CLIMB_UP_FORCE | 1.2 | Upward force (scaled by speed) |
| CLIMB_SPEED_COST | 0.09 | Forward speed lost/frame climbing |
| YAW_SPEED | 0.67 | Degrees/frame turn rate |
| Vertical drag | 0.96 multiplier | Makes climb/dive plateau naturally |
| Vertical clamp | ±30.0 | Prevents geometry clipping |
| Diminishing lift | 1/(1 + vy*0.15) | Climbing gets weaker as you go higher |

## Current Controls

| Input | Action |
|-------|--------|
| D-pad Up | Give cape (debug, skips CapeBox) |
| Triple Jump | Activate flight (requires cape) |
| Stick Forward | Dive (gain speed + descend) |
| Stick Back | Climb (trade speed for altitude) |
| Stick Left/Right | Turn (with roll banking on model) |
| D-pad Down | Cancel flight (debug) |

## TODO / Known Issues

- No visual cape model on Mario (uses fall animation as placeholder)
- FLUDD swap not connected to flight activation (timer only runs from D-pad Up debug trigger)
- CapeBox object registered but no levels have one placed (use SMS Bin Editor)
- Sound effects not implemented (voice IDs found in decomp: 0x78B6=triple jump, 0x78AB=land, etc.)
- Camera doesn't adjust during flight
- No ground pound dive bomb attack
- Landing is abrupt (no forward momentum carry into run)
- Pitch tilt on model not implemented (only roll banking via mAngle.z)
- The `!` in `Kuribo!` directory name causes shell escaping issues — always use Python's pathlib for file operations touching that path
