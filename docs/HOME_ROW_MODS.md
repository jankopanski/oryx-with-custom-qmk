# Home-Row Mods Implementation

A QMK implementation for the Moonlander keyboard that provides **conditional home-row modifiers** using standard QMK Mod-Tap functionality - home-row keys act as regular letters on the base layer (Layer 0), but become dual-function modifier keys when accessing Layers 1 and 2 via thumb keys.

## Features

- **Layer-conditional behavior**: Home-row keys (A, S, D, F, H, J, K, L, ;) are regular letters on Layer 0
- **Modifier activation on layers 1 & 2**: Home-row keys become dual-function using QMK's `MT()` (tap = letter, hold = modifier)
- **Auto Shift compatible**: Works with Auto Shift on all layers (180ms timeout)
- **Permissive Hold**: Enabled for immediate modifier resolution when another key is pressed

## Layer Overview

### Layer 0 (Base/Alpha Layer)
- Home-row keys: Plain letter keys (`KC_A`, `KC_S`, `KC_D`, `KC_F`, `KC_H`, `KC_J`, `KC_K`, `KC_L`, `KC_SCLN`)
- Only Auto Shift applies (no Mod-Tap behavior)

### Layer 1 (Numbers Layer)
- Accessed via left thumb: `LT(1, KC_ENTER)`
- Left home-row becomes Mod-Tap:
  - `MT(MOD_LCTL, KC_A)` - Ctrl when held, A when tapped
  - `MT(MOD_LALT, KC_S)` - Alt when held, S when tapped
  - `MT(MOD_LGUI, KC_D)` - Cmd when held, D when tapped
  - `MT(MOD_LSFT, KC_F)` - Shift when held, F when tapped
- Right home-row becomes Mod-Tap (mirrored):
  - `MT(MOD_LSFT, KC_4)` through `MT(MOD_LCTL, KC_EQUAL)`

### Layer 2 (Symbols Layer)
- Accessed via right thumb: `LT(2, KC_SPACE)`
- Right home-row becomes Mod-Tap:
  - `MT(MOD_LSFT, KC_J)` - Shift when held, J when tapped
  - `MT(MOD_LGUI, KC_K)` - Cmd when held, K when tapped
  - `MT(MOD_LALT, KC_L)` - Alt when held, L when tapped
  - `MT(MOD_LCTL, KC_SCLN)` - Ctrl when held, ; when tapped

## Modifier Mappings (macOS Order)

### Layer 1 (Left hand active)
| Left Hand | Modifier | Right Hand (numbers) | Modifier |
|-----------|----------|----------------------|----------|
| A         | Control  | 6                    | Alt      |
| S         | Alt      | 5                    | Command  |
| D         | Command  | 4                    | Shift    |
| F         | Shift    | =                    | Control  |

### Layer 2 (Right hand active)
| Right Hand | Modifier |
|------------|----------|
| J          | Shift    |
| K          | Command  |
| L          | Alt      |
| ;          | Control  |

## Auto Shift Behavior

**CRITICAL DIFFERENCE**: Auto Shift behaves differently on normal letter keys versus Mod-Tap keys.

### Normal Letter Keys (All keys on Layer 0, non-home-row keys on all layers)

**Timeline:**
```
Press → 180ms → [OUTPUT: Uppercase letter] → still holding → Release
                 ↑
            Character appears immediately
```

**Behavior:**
- **Quick tap (<180ms)**: Outputs lowercase letter immediately on release
- **Hold (≥180ms)**: **Uppercase letter appears at the 180ms mark** while key is still held
- Output is **immediate** when the timeout is reached
- No waiting for key release

**Example:** Hold `Q` for 250ms → uppercase `Q` appears on screen at 180ms, continues to repeat if you keep holding

### Mod-Tap Keys (Home-row keys on Layers 1 & 2)

**Timeline:**
```
Press → 180ms → 200ms → Release at 190ms → [OUTPUT: Uppercase letter]
   [Auto Shift] [Mod-Tap]                    ↑
                                        Character appears ONLY on release

Press → 180ms → 200ms → [Modifier Active] → Release → [Modifier Deactivates]
   [Auto Shift] [Mod-Tap] ↑                           (no letter output)
                      Resolved as modifier
```

**Behavior:**
- **Quick tap (<180ms)**: Outputs lowercase letter **on release**
- **Medium hold (180-200ms)**: Outputs uppercase letter **on release** (Auto Shift applied, but still within tapping term)
- **Long hold (≥200ms)**: Activates **modifier** at 200ms, no letter output
- Output is **deferred** until key is released or TAPPING_TERM expires
- QMK waits to determine tap vs. hold before sending any output

**Example:** Hold `A` on Layer 1 for 190ms then release → uppercase `A` appears only when you release the key

### Why This Difference Exists

1. **Normal keys** have only one function → QMK can immediately output the shifted character at 180ms
2. **Mod-Tap keys** have dual functions → QMK must complete the tap/hold decision algorithm before outputting anything:
   - Must wait until release OR until TAPPING_TERM expires to know which function to execute
   - If released before 200ms → send letter (apply Auto Shift if held >180ms)
   - If held past 200ms → activate modifier (no letter sent)

### Perceptual Impact

- **Normal keys feel responsive**: Character appears instantly at timeout
- **Mod-Tap keys have perceptible delay**: Slight lag between release and character appearance, especially in the 180-200ms range
- This is an **inherent trade-off** of using home-row modifiers

### The 20ms Ambiguous Zone

With `AUTO_SHIFT_TIMEOUT = 180ms` and `TAPPING_TERM = 200ms`, there's a **20ms window** where:
- Auto Shift has triggered (wants to send uppercase)
- But Mod-Tap hasn't resolved yet (still deciding tap vs. hold)
- Result: Uppercase letter on release (if released in this window)

This can make the behavior feel slightly unpredictable when timing falls in this range.

## Configuration

### config.h
```c
#define PERMISSIVE_HOLD              // Enable immediate modifier on other key press
#define AUTO_SHIFT_TIMEOUT 180       // Auto Shift threshold (180ms)
// Note: TAPPING_TERM defaults to 200ms (not explicitly defined)
```

### rules.mk
```makefile
AUTO_SHIFT_ENABLE = yes              // Enable Auto Shift feature
```

## Timing Recommendations

To avoid the 20ms ambiguous zone and create more predictable behavior:

**Option 1: Increase TAPPING_TERM** (more separation)
```c
#define TAPPING_TERM 250    // Creates 70ms separation
```

**Option 2: Decrease AUTO_SHIFT_TIMEOUT** (faster Auto Shift)
```c
#define AUTO_SHIFT_TIMEOUT 150    // Creates 50ms separation
```

**Option 3: Disable Auto Shift on Mod-Tap keys**
```c
#define NO_AUTO_SHIFT_MODIFIERS   // Simplifies Mod-Tap behavior
```

## Usage

### Normal Typing (Layer 0)
- Type normally - home-row keys output letters
- Hold any letter key for ≥180ms → uppercase (Auto Shift)
- Output appears **immediately at timeout**

### Using Modifiers (Layers 1 & 2)
1. Hold left thumb `LT(1, KC_ENTER)` or right thumb `LT(2, KC_SPACE)`
2. While holding, interact with home-row keys:
   - **Quick tap (<180ms)**: Lowercase letter (output on release)
   - **Medium hold (180-200ms)**: Uppercase letter (output on release)
   - **Long hold (≥200ms)**: Modifier activates (no letter)
3. With PERMISSIVE_HOLD: Press another key while holding home-row → modifier resolves immediately

## Other Dual-Function Keys

The `DUAL_FUNC_0-3` custom keys on Layer 2 provide additional functionality:
- `DUAL_FUNC_0` (Line 14): `LT(8, KC_F16)` - Layer 8 on hold, with custom tap behavior (Alt+Backspace)
- `DUAL_FUNC_1` (Line 15): `LT(2, KC_F22)` - Custom dual-function (~ on tap, Ctrl on hold)
- `DUAL_FUNC_2` (Line 16): `LT(2, KC_Z)` - Custom dual-function ({ on tap, Alt on hold)
- `DUAL_FUNC_3` (Line 17): `LT(4, KC_P)` - Custom dual-function (( on tap, Shift on hold)
