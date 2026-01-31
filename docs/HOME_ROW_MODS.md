# Conditional Home-Row Mods

A custom QMK implementation for the Moonlander keyboard that provides **conditional home-row modifiers** - home-row keys act as regular letters on the base layer, but become modifier keys when a thumb layer is active.

## Features

- **Layer-conditional behavior**: Home-row keys (A, S, D, F, J, K, L, ;) are regular letters on layer 0
- **Modifier activation**: When holding a thumb layer key, home-row keys become dual-function (tap = letter, hold = modifier)
- **Flexible key sequencing**: Press home-row first, then add thumb within 200ms → activates modifier
- **Safe release ordering**: Releasing thumb before home-row → no action (prevents accidental input)
- **Auto Shift compatible**: Works with Auto Shift on base layer (150ms timeout)
- **Same-hand chording**: Chordal Hold enabled for comfortable same-hand modifier combinations

## How It Works

```
┌─────────────────────────────────────────────────────────────────┐
│                     HRM Key Pressed                             │
└─────────────────────┬───────────────────────────────────────────┘
                      ▼
              ┌───────────────┐
              │ Start Timer   │
              │ (200ms)       │
              └───────┬───────┘
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
  ┌───────────┐ ┌───────────┐ ┌───────────┐
  │ Thumb     │ │ Key       │ │ Thumb     │
  │ Pressed   │ │ Released  │ │ Released  │
  │ (in time) │ │ (no thumb)│ │ First     │
  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
        ▼             ▼             ▼
  ┌───────────┐ ┌───────────┐ ┌───────────┐
  │ Modifier  │ │ Tap Letter│ │ No Action │
  │ Mode      │ │ (+AutoShift│ │(Invalidated)
  └───────────┘ │  if >150ms)│ └───────────┘
                └───────────┘
```

## Modifier Mappings (macOS)

| Left Hand | Modifier | Right Hand | Modifier |
|-----------|----------|------------|----------|
| A         | Control  | J          | Shift    |
| S         | Option   | K          | Command  |
| D         | Command  | L          | Option   |
| F         | Shift    | ;          | Control  |

## Configuration

### config.h
```c
#define TAPPING_TERM 200          // 200ms window for tap/hold decision
#define TAPPING_TERM_PER_KEY      // Per-key customization support
#define CHORDAL_HOLD              // Same-hand chord detection
#define AUTO_SHIFT_TIMEOUT 150    // Auto Shift threshold
```

### rules.mk
```makefile
DEFERRED_EXEC_ENABLE = yes        // Timer-based callbacks
AUTO_SHIFT_ENABLE = yes           // Auto Shift feature
```

## Implementation Details

### Custom Keycodes
- `HRM_A` through `HRM_SCLN` - Custom keycodes for each home-row key

### State Tracking
Each home-row key tracks:
- Press time (for tapping term calculation)
- Layer state at press time
- Decision status (tap/hold/invalidated)

### Key Functions
| Function | Purpose |
|----------|---------|
| `hrm_handle_press()` | Initialize state, start timer |
| `hrm_handle_release()` | Execute tap/hold/nothing based on state |
| `hrm_activate_modifier()` | Register modifier key |
| `hrm_activate_tap()` | Send letter (with Auto Shift support) |
| `hrm_check_pending_keys_for_hold()` | Transition pending keys when thumb pressed |
| `hrm_invalidate_pending_keys()` | Cancel pending keys when thumb released |

## Usage

### Normal Typing (Layer 0)
Just type normally - home-row keys output their letters. Hold for Auto Shift.

### Using Modifiers
1. Hold left thumb (`LT(1, KC_ENTER)`) or right thumb (`LT(2, KC_SPACE)`)
2. While holding, tap or hold a home-row key:
   - **Tap**: Outputs the letter
   - **Hold**: Activates the modifier

### Advanced Key Sequence
Press home-row key first, then quickly add thumb within 200ms:
- The key transitions to modifier mode
- Useful for quick modifier activation

## Preserved Features

The existing `DUAL_FUNC_0-3` keys on layer 2 are unchanged:
- `DUAL_FUNC_0`: Alt+Backspace on tap, Shift on hold
- `DUAL_FUNC_1`: ~ on tap, Ctrl on hold
- `DUAL_FUNC_2`: { on tap, Alt on hold
- `DUAL_FUNC_3`: ( on tap, Shift on hold
