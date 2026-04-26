# Dialogue Modifier Lifecycle

Data flow through the facial animation system during dialogue, showing how
`modifier1`/`phoneme1` values are produced, consumed, and cleared.

## 1. Active FaceFX (timer <= animEnd)

```
KeyframesUpdateHook
  +-- SmoothUpdate / RegularUpdate
  |     +-- DialogueModifiersUpdate
  |     |     +-- dialogueData valid, refCount OK -> proceed
  |     |     +-- modifier1.timer += timeDelta
  |     |     +-- timer <= animEnd -> call sub_1FCD10
  |     |     +-- modifier1.values <- fresh animation data (blinks, brows, etc.)
  |     |
  |     +-- DialoguePhonemesUpdate
  |     |     +-- (same pattern -> phoneme1.values <- lip sync data)
  |     |
  |     +-- EyesBlinkingUpdate -> blinkValue into modifier3 or modifier2.timer
  |     |
  |     +-- merge/animMerge(modifier1, modifier2, modifier3)
  |     |     +-- modifier1 blink values override blinking system (intended)
  |     |
  |     +-- final modifier3 -> rendered face
```

FaceFX interpolation provides live modifier/phoneme data each frame.
The merge logic lets dialogue-driven blinks and expressions take priority
over the procedural blinking system.

## 2. Trailing WAV silence (timer > animEnd, dialogueData still alive)

### Before (bug)

```
KeyframesUpdateHook
  +-- SmoothUpdate / RegularUpdate
  |     +-- DialogueModifiersUpdate
  |     |     +-- dialogueData valid, refCount OK -> proceed
  |     |     +-- modifier1.timer += timeDelta
  |     |     +-- call sub_1FCD10 (timer past FaceFX end)
  |     |     +-- modifier1.values <- STALE last-frame data (BlinkLeft/Right != 0)
  |     |
  |     +-- DialoguePhonemesUpdate
  |     |     +-- (same -> phoneme1.values <- stale lip sync data)
  |     |
  |     +-- EyesBlinkingUpdate -> blinkValue computed normally
  |     |
  |     +-- merge/animMerge(modifier1, modifier2, modifier3)
  |     |     +-- modifier1 blink values != 0 -> OVERRIDE blinking system
  |     |     +-- eyes forced shut by stale dialogue data
  |     |
  |     +-- final modifier3 -> eyes stuck closed
```

`sub_1FCD10` returns the last FaceFX frame's values for any time past the
animation end. If that frame has non-zero BlinkLeft/BlinkRight, the merge
logic treats them as live dialogue data and overrides the blinking system
every tick. Eyes remain shut for the entire trailing silence and beyond
(values are never cleared).

### After (fix)

```
KeyframesUpdateHook
  +-- SmoothUpdate / RegularUpdate
  |     +-- DialogueModifiersUpdate
  |     |     +-- dialogueData valid, refCount OK -> proceed
  |     |     +-- modifier1.timer += timeDelta
  |     |     +-- timer > animEnd -> modifier1.Reset(), return   <-- FIX
  |     |     +-- sub_1FCD10 NOT called (no stale values written)
  |     |
  |     +-- DialoguePhonemesUpdate
  |     |     +-- (same -> phoneme1.Reset(), skip interpolation)
  |     |
  |     +-- EyesBlinkingUpdate -> blinkValue computed normally
  |     |
  |     +-- merge/animMerge(modifier1, modifier2, modifier3)
  |     |     +-- modifier1 is all zeros -> blinking system drives eyes
  |     |
  |     +-- final modifier3 -> eyes blink normally despite WAV still playing
```

The interpolation call is skipped once the timer exceeds the FaceFX
animation duration, and `modifier1` is zeroed so the blinking system
regains control immediately.

## 3. Dialogue data release (phoneme1.timer > animEnd + 0.2s)

```
KeyframesUpdateHook
  +-- SmoothUpdate / RegularUpdate
  |     +-- (modifier1/phoneme1 already zeroed by phase 2)
  |
  +-- CheckAndReleaseDialogueData
        +-- phoneme1.timer > timer threshold -> release
        +-- ReleaseDialogueData(dialogueData)
        +-- modifier1.Reset()    <-- safety net
        +-- phoneme1.Reset()     <-- safety net
        +-- dialogueData = nullptr
```

`CheckAndReleaseDialogueData` runs outside the spinlock, after
`SmoothUpdate`/`RegularUpdate`. The `Reset()` calls here are a safety net
in case the phase-2 clearing was bypassed (e.g. refCount state prevented
`DialogueModifiersUpdate` from running).

`Reset()` only zeros the `values` array and `isUpdated` flag -- it does
not touch the `timer` field, so the `phoneme1.timer` comparison that gates
the release decision is unaffected.

## 4. No dialogue (dialogueData == nullptr)

```
KeyframesUpdateHook
  +-- SmoothUpdate / RegularUpdate
  |     +-- DialogueModifiersUpdate
  |     |     +-- dialogueData == nullptr -> return (unchanged)
  |     |          modifier1.values already zero from phase 2/3
  |     |
  |     +-- EyesBlinkingUpdate -> normal blink cycle
  |     |
  |     +-- merge/animMerge -> modifier1 all zeros -> no override
  |     |
  |     +-- blinking system has full control
```

No changes needed here. Once `modifier1` is zeroed during phase 2 or 3,
the values stay zero because `DialogueModifiersUpdate` returns early
when `dialogueData` is null.
