# Mfg Fix NG -- AI Smoke Tests

Pre-release checklist. Each scenario should be verified in-game or by code inspection before tagging a release.

**Priority key:** P0 = blocker, P1 = high, P2 = medium

---

## 1. Plugin Loading & Initialization

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 1.1 | P0 | Game starts with plugin installed | `SKSEPluginLoad` succeeds, logger initialized, `MfgFix::Init()` called | main.cpp |
| 1.2 | P0 | Detour hooks attached | `KeyframesUpdateHook` and `ModifyFaceGenCommandHook` detours both report `NO_ERROR` in log | BSFaceGenAnimationData::Init, ConsoleCommands::Init |
| 1.3 | P0 | Papyrus functions registered | `MfgConsoleFunc` and `MfgConsoleFuncExt` bindings available to scripts | MfgConsoleFunc::Register |
| 1.4 | P0 | INI settings loaded | `mfgfix.ini` read on init; blink timing, eye movement, transition speed populated | Settings::Read |
| 1.5 | P1 | Binary patches applied | Dead-NPC expression NOP, SetExpressionOverride mood-0 fix, null parent node crash prevention all applied | mfgfixinit.cpp |
| 1.6 | P1 | VR build path | All `REL::Module::IsVR()` branches use correct offsets; no SE/AE relocations used on VR | All files |

## 2. Keyframe Blending -- Regular Update (speed = 0)

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 2.1 | P0 | Expression merge | `expression3` built from `expression1` (dialogue/override) then `expression2` (user); non-zero source wins | RegularUpdate |
| 2.2 | P0 | Modifier merge order | `modifier3` reset, then blink + dialogue modifiers, then `merge(modifier1, modifier3)`, then eye direction, then `merge(modifier2, modifier3)` | RegularUpdate |
| 2.3 | P0 | Phoneme merge with dialogue active | `phoneme2` values below `fDialoguePhonemeThreshold` zeroed; above-threshold values override `phoneme3` | RegularUpdate |
| 2.4 | P1 | Phoneme merge without dialogue | Standard non-zero merge from `phoneme2` into `phoneme3` (no threshold filtering) | RegularUpdate |
| 2.5 | P1 | Spinlock held during entire update | `RE::BSSpinLockGuard` acquired at top, released at end; no data races | RegularUpdate |

## 3. Keyframe Blending -- Smooth Update (speed > 0)

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 3.1 | P0 | animMerge interpolation | For each value: dialogue direct-copy when modifier ~0, snap when within step, else step toward modifier | SmoothUpdate |
| 3.2 | P0 | Blink overlay math | Pre-blink undo (divide out previous blinkValue), post-blink apply (multiply in current blinkValue), `else` branch for near-1.0 avoids divide-by-zero | SmoothUpdate |
| 3.3 | P1 | Per-actor speed | `ActorManager::GetSpeed` returns actor-specific speed; `animationStep = timeDelta / speed` scales smoothing rate | SmoothUpdate, ActorManager |
| 3.4 | P1 | Eye direction smooth update | Eye heading/pitch clamped to `fTrackEyeXY`/`fTrackEyeZ` range, rate-limited by `fTrackSpeed * timeDelta` | SmoothUpdate |
| 3.5 | P2 | Operator precedence in animMerge | `i >= modifier.count || fabs(modifier.values[i]) < FLT_EPSILON && fabs(dialogue.values[i]) > FLT_EPSILON` -- `&&` binds before `||`; verify this is intentional (dialogue fallback only when modifier ~0 AND dialogue non-zero) | SmoothUpdate |

## 4. Eye Blinking

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 4.1 | P0 | Normal blink cycle | `BlinkDelay` (random 0.5-8s) -> `BlinkDown` (0.04s) -> `BlinkUp` (0.14s) -> repeat | EyesBlinkingUpdate |
| 4.2 | P0 | blinkValue clamped | `std::clamp(blinkValue, 0.0f, 1.0f)` applied after switch | EyesBlinkingUpdate |
| 4.3 | P1 | BlinkDownAndWait1 with unk21A=true | Eyes close and stay closed while `unk21A` is true; no timer-expiry transition (intentional hold for headtracking) | EyesBlinkingUpdate |
| 4.4 | P1 | BlinkDownAndWait1 with unk21A=false | `blinkValue = 1.0`, transitions to `BlinkUp` with `fBlinkUpTime` timer | EyesBlinkingUpdate |
| 4.5 | P1 | Default/unknown stage | Resets to `BlinkDelay` with random timer; `blinkValue = 0` | EyesBlinkingUpdate |
| 4.6 | P2 | Zero blink times | `fBlinkDownTime = 0` -> `blinkValue = 1.0` (instant close); `fBlinkUpTime = 0` -> `blinkValue = 0.0` (instant open) | EyesBlinkingUpdate |

## 5. Eye Movement

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 5.1 | P1 | Emotion-aware offsets | Each of 10 expression pairs (dialogue+mood) uses its own heading/pitch/delay ranges from settings | EyesMovementUpdate |
| 5.2 | P1 | Disabled during headtracking | `!unk21A` gate prevents eye movement and direction updates during dialogue camera lock | RegularUpdate, SmoothUpdate |
| 5.3 | P2 | Fear emotion 50% chance | `rand(0,1) <= 0.5` chance to zero out heading/pitch offsets for fear | EyesMovementUpdate |
| 5.4 | P2 | LookLeft/Right/Down/Up clamped 0-1 | Final modifier3 look values clamped in SmoothUpdate eye direction block | SmoothUpdate |

## 6. Dialogue System

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 6.1 | P0 | Active dialogue updates modifiers | `DialogueModifiersUpdate` calls `sub_1FCD10` with `modifier1.timer`, writes to `modifier1.values` | DialogueModifiersUpdate |
| 6.2 | P0 | Active dialogue updates phonemes | `DialoguePhonemesUpdate` calls `sub_1FC9B0` with `phoneme1.timer`, writes to `phoneme1.values` | DialoguePhonemesUpdate |
| 6.3 | P0 | **Stale modifier cleanup (FIX)** | When `modifier1.timer > animEnd`, `modifier1.Reset()` called and interpolation skipped; prevents eyes stuck closed from trailing WAV silence | DialogueModifiersUpdate |
| 6.4 | P0 | **Stale phoneme cleanup (FIX)** | Same as 6.3 for `phoneme1` | DialoguePhonemesUpdate |
| 6.5 | P0 | Dialogue data release | `CheckAndReleaseDialogueData` releases when `phoneme1.timer > animEnd + 0.2s`; `modifier1.Reset()` + `phoneme1.Reset()` before nulling pointer | CheckAndReleaseDialogueData |
| 6.6 | P1 | Reset() preserves timer | `Reset()` zeros `values[]` and `isUpdated` only; `timer` field untouched so `CheckAndReleaseDialogueData` timer comparison still works | BSFaceGenKeyframeMultiple::Reset |
| 6.7 | P1 | refCount gating | Both dialogue update functions and `CheckAndReleaseDialogueData` return early when `dialogueData` is null or refCount check fails | All three functions |
| 6.8 | P1 | animEnd calculation | `(unk0 + abs(unk4 if negative)) * 0.033f` matches FaceFX frame-to-seconds conversion; consistent between DialogueModifiers/Phonemes and CheckAndRelease (+0.2s grace) | DialogueModifiersUpdate, CheckAndReleaseDialogueData |

## 7. Expression Transitions

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 7.1 | P0 | TransitionUpdate | `expression1.TransitionUpdate(timeDelta, transitionTarget)` interpolates toward target each frame | RegularUpdate, SmoothUpdate |
| 7.2 | P1 | SetExpressionOverride | Clears `expressionOverride`, calls engine function, re-enables override; expression1 updated | SetExpressionOverride, ConsoleCommands::SetValue |
| 7.3 | P2 | 17 expressions valid | Indices 0-16 map to 7 dialogue + 7 mood + 3 combat expressions | BSFaceGenKeyframeMultiple::Expression |

## 8. Console Commands

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 8.1 | P0 | `mfg expression <id> <value>` | Sets expression override on selected actor; value divided by 100 | ConsoleCommands |
| 8.2 | P0 | `mfg modifier <id> <value>` | Sets `modifier2` value on selected actor | ConsoleCommands |
| 8.3 | P0 | `mfg phoneme <id> <value>` | Sets `phoneme2` value on selected actor | ConsoleCommands |
| 8.4 | P0 | `mfg reset` | Clears expression override, calls `Reset(0, true, true, true, false)` | ConsoleCommands |
| 8.5 | P1 | `mfg expression` (no args) | Prints all expression values to console | ConsoleCommands::PrintInfo |
| 8.6 | P1 | `mfg custom <id> <value>` | Sets `custom2` value on selected actor | ConsoleCommands |
| 8.7 | P2 | No selected actor | Falls back to `RE::PlayerCharacter::GetSingleton()` | ConsoleCommands |

## 9. Papyrus API

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 9.1 | P0 | `SetPhonemeModifier(actor, type, id, value)` | Dispatches via UI task; type 0=phoneme (0-15), 1=modifier (0-13), 2=expression (0-16); value 0-200 | MfgConsoleFunc |
| 9.2 | P0 | `GetPhonemeModifier(actor, type, id)` | Returns current value * 100; -1 on invalid actor/animData | MfgConsoleFunc |
| 9.3 | P0 | `SetPhonemeModifierSmooth(actor, type, id, value, speed)` | Same as SetPhonemeModifier but sets `ActorManager::SetSpeed` for smooth transitions | MfgConsoleFunc |
| 9.4 | P1 | `ApplyExpressionPreset(actor, float[32], ...)` | 32-element vector: [0-15] phonemes, [16-29] modifiers, [30] exprID, [31] strength; applies via UI task | MfgConsoleFunc |
| 9.5 | P1 | `ResetMFGSmooth(actor, mode, speed)` | mode -1=all, 0=phonemes, 1=modifiers; clears values then resets | MfgConsoleFunc |
| 9.6 | P1 | `GetPlayerSpeechTarget()` | Returns current dialogue partner via `MenuTopicManager::speaker` | MfgConsoleFunc |
| 9.7 | P1 | `IsInDialogue(actor)` | Returns true when `animData->dialogueData` is non-null | MfgConsoleFunc |
| 9.8 | P2 | Value clamping | All set functions clamp input to 0-200 before dividing by 100 (storage range 0.0-2.0) | MfgConsoleFunc |

## 10. Settings & Configuration

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 10.1 | P1 | INI read/write | `Settings::Read()` / `Settings::Write()` persist all settings to `mfgfix.ini` via SimpleINI | Settings.cpp |
| 10.2 | P1 | Papyrus settings bindings | `SettingsPapyrus` exposes get/set for blink timing, eye movement, transition speed to Papyrus scripts | SettingsPapyrus.cpp |
| 10.3 | P2 | Default values | `fBlinkDownTime=0.04`, `fBlinkUpTime=0.14`, `fBlinkDelayMin=0.5`, `fBlinkDelayMax=8.0`, `fDefaultSpeed=0.0`, `fDialoguePhonemeThreshold=50.0` | Settings.h |

## 11. Binary Patches

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 11.1 | P0 | Dead NPC expression fix | NOP bytes at `sub_3DB770 + 0x4B` (9 or 13 bytes depending on version) allow expression changes on dead NPCs | mfgfixinit.cpp |
| 11.2 | P0 | SetExpressionOverride mood 0 fix | Two patches at `Papyrus::Actor::SetExpressionOverride` fix mood index 0 (neutral) handling | mfgfixinit.cpp |
| 11.3 | P1 | Null parent node crash prevention | 15 NOP bytes at `BSFaceGenNiNode::sub_3F0C90 + 0x2E3/0x395` disable crash-prone eye animation code | mfgfixinit.cpp |
| 11.4 | P1 | Eye update relocation | `BSFaceGenNiNode::sub_3F1800 + 0x139` short-jump (0x47EB) removes eyes update from `UpdateDownwardPass` (moved to KeyframesUpdate hook) | BSFaceGenAnimationData::Init |

## 12. Thread Safety

| # | P | Scenario | Expected | Source |
|---|---|----------|----------|--------|
| 12.1 | P0 | Update functions hold spinlock | `RegularUpdate` and `SmoothUpdate` both acquire `RE::BSSpinLockGuard(lock)` at entry | RegularUpdate, SmoothUpdate |
| 12.2 | P0 | Console commands hold spinlock | `SetValue`, `PrintInfo`, `Reset` all acquire spinlock before accessing animData | ConsoleCommands |
| 12.3 | P1 | Papyrus UI task dispatch | `SetPhonemeModifierSmooth`, `ApplyExpressionPreset`, `ResetMFGSmooth` all execute via `SKSE::GetTaskInterface()->AddUITask` with spinlock inside | MfgConsoleFunc |
| 12.4 | P1 | CheckAndReleaseDialogueData outside lock | Runs after `SmoothUpdate`/`RegularUpdate` release lock; modifies `dialogueData` pointer without lock -- safe because hook replaces the only caller | KeyframesUpdateHook |

---

## 13. Known Issues & Fragile Areas

| # | P | Issue | Location | Impact |
|---|---|-------|----------|--------|
| 13.1 | ~P0~ | ~~**Stale dialogue modifiers -- eyes stuck closed** after dialogue lines with trailing WAV silence past FaceFX end~~ **FIXED** -- `DialogueModifiersUpdate`/`DialoguePhonemesUpdate` now clear values and skip interpolation when timer > animEnd; `CheckAndReleaseDialogueData` resets both on release | BSFaceGenAnimationData.cpp | Eyes no longer stuck shut after dialogue |
| 13.2 | ~P2~ | ~~**animMerge operator precedence** -- missing parentheses made intent unclear~~ **FIXED** -- added explicit parens around `&&` clause; no behavior change | SmoothUpdate | Clarity improvement only |
| 13.3 | ~P2~ | ~~**BlinkDownAndWait1 stuck state** -- when `unk21A` is true and `eyesBlinkingTimer` reaches 0, no stage transition occurs~~ **FIXED** -- added timer-expiry transition to `BlinkUp` when `unk21A` is true, matching `BlinkDown` behavior. Defensive fix; stage only reachable if the engine externally sets `eyesBlinkingStage` | EyesBlinkingUpdate | Eyes no longer stuck if stage is entered during headtracking |
| 13.4 | ~P2~ | ~~**ActorManager forward declaration** -- `ActorManager.h` references `BSFaceGenAnimationData*` without forward declaration~~ **FIXED** -- added `class BSFaceGenAnimationData;` forward declaration in `ActorManager.h` | ActorManager.h | Build no longer depends on include order |
| 13.5 | ~~ | ~~**Modifier range check inconsistency** -- `SetModifier` bounds-checks `a_id > 13` but enum defines 17 modifiers (0-16)~~ **BY DESIGN** -- HeadPitch/Roll/Yaw (14-16) are engine-controlled via headtracking; allowing script writes would fight the engine every frame. Range 0-13 is the correct script-accessible subset | MfgConsoleFunc | Not a bug |
| 13.6 | ~P2~ | ~~**Phoneme value scale mismatch** -- `SetValue` clamps to 0-200 and divides by 100 (max 2.0), but `IsValueValid` checks range [0, 1]~~ **BY DESIGN** -- `IsValueValid` is an engine vtable stub never called by the mod; 0-200 range is intentional for exaggerated morphs. API docs updated to document full range | MfgConsoleFunc, BSFaceGenKeyframeMultiple | Not a bug |

---

*Last updated: 2026-04-27*
