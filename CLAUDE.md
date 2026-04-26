# Mfg Fix NG

Skyrim SE SKSE plugin: extended facial animation/expression system with smooth transitions, eye blinking/movement, and Papyrus API. C++ plugin built on CommonLibSSE-NG. Version-independent (SE/AE/VR) via RelocationID hooks.

## Build

```sh
# xmake 2.9.5+, MSVC v143 (VS 2022), C++23
# Env vars: XSE_TES5_GAME_PATH (SSE install), XSE_TES5_MODS_PATH (MO2 mods folder)
git submodule update --init --recursive
xmake f -m release && xmake        # -> dist/SKSE/Plugins/mfgfix.dll
```

Post-build compiles Papyrus scripts via `XSE_TES5_GAME_PATH/Papyrus Compiler/`, copies to `dist/SKSE/Plugins/`, and optionally copies to MO2 mod folder.

## Dependencies

- CommonLibSSE-NG (submodule at `lib/commonlibsse-ng`)
- Microsoft Detours (submodule at `lib/detours`)
- SimpleINI, DirectXTK (xmake packages)

## Project Structure

```
src/
  main.cpp                          SKSE entry point -> MfgFix::Init()
  PCH.h                             Precompiled header
  mfgfix/
    mfgfixinit.cpp/.h               Init orchestrator: hooks, patches, registration
    BSFaceGenAnimationData.cpp/.h    Core: animation update hook, transitions, blinking, eye movement
    BSFaceGenKeyframe.h              Abstract keyframe base class
    BSFaceGenKeyframeMultiple.cpp/.h Keyframe impl (float arrays for expressions/modifiers/phonemes)
    Settings.cpp/.h                  INI config singleton (transition speed, blink timing, eye movement)
    SettingsPapyrus.cpp/.h           Papyrus bindings for settings read/write
    ConsoleCommands.cpp/.h           Console SetValue/PrintInfo for keyframe manipulation
    MfgConsoleFunc.cpp/.h            Papyrus native functions (phoneme/modifier/expression get/set)
    ActorManager.h                   Per-actor animation speed map (FormID -> speed)
    Offsets.h                        RelocationID mappings for all hooked functions
dist/
  source/scripts/
    MfgConsoleFunc.psc               Native wrappers: Get/SetPhonemeModifier, expression queries
    MfgConsoleFuncExt.psc            Extended API: smooth transitions, ApplyExpressionPreset, ResetMFGSmooth
  scripts/                           Compiled .pex output
  SKSE/Plugins/                      Compiled .dll output
```

## Core Systems

### 1. Keyframe Blending & Update Hook

`BSFaceGenAnimationData::KeyframesUpdateHook()` is the main tick. Three keyframe layers per category (expression, modifier, phoneme, custom):
- `keyframe1`: dialogue-driven values
- `keyframe2`: user/console/Papyrus-set values
- `keyframe3`: final blended output

Linear interpolation blends layers. Transition speed per-actor via `ActorManager`.

### 2. Expression System

17 expressions: 7 dialogue, 7 mood, 3 combat. Values stored as 0.0-1.0 floats internally; Papyrus API uses 0-200 integer scale (divided by 200). Smooth transitions interpolate between `transitionTarget` and `expression3`.

### 3. Eye Blinking

6-stage state machine (closed -> opening -> open -> delay -> closing -> closed). Configurable blink down/up times and random delay range via INI settings.

### 4. Eye Movement

Emotion-aware eye offsets with per-mood heading/pitch ranges. Random target selection with configurable delay timers.

### 5. Dialogue System

Tracks `dialogueData`; applies modifier/phoneme interpolation from dialogue-specific keyframes. Spinlock-protected for thread safety.

### 6. Binary Patches

Applied in `mfgfixinit.cpp`:
- Fixes expression changes on dead NPCs (NOPs dead-NPC expression blocks)
- Fixes `SetExpressionOverride` mood 0 handling
- Disables null parent node crash in eye animation code

## Code Conventions

- Thread-safe access via `RE::BSSpinLock` on `BSFaceGenAnimationData`
- All values clamped to 0.0-1.0 range in storage
- Console variable `mfgfixng` for runtime setting adjustments
- spdlog-based logging; debug builds log to MSVC output, release to file
- RelocationID-based hooking for version independence (SE/AE/VR)

## Papyrus API

**MfgConsoleFunc** (native): `SetPhonemeModifier(actor, type, id, value)`, `GetPhonemeModifier(actor, type, id)` where type 0=phoneme, 1=modifier, 2=expression. Value 0-200 (0-100 normal range; 101-200 allowed for exaggerated morphs).

**MfgConsoleFuncExt** (Papyrus): Smooth animation wrappers, `ApplyExpressionPreset(actor, float[32])` where the 32-element array is [16 phonemes (0.0-1.0), 14 modifiers (0.0-1.0), expressionID (0-16), strength (0.0-1.0)]. `ResetMFGSmooth(actor, mode, speed)` for smooth reset.
