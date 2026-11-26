# Home-Row Mods & Typing-Feel Reference (`AWDDG`)

A custom QMK home-row modifier engine (`AWDDG/hrm.h`, `AWDDG/hrm.c`) for the ZSA Moonlander running with **macOS virtual Colemak**.

Home-row keys act strictly as **plain letters on Layer 0** (preventing roll misfires across Colemak's `A R S T N E I O`, which account for ~63.5% of typed letters) and become **0ms modifiers** when gated by the primary thumb layer keys (Layers 1, 2, and 4).

---

## 1. Modifier Mappings (Mirrored macOS Order)

| Physical QMK Key | Colemak Output | Hand | Modifier (Layers 1 / 2 / 4) |
|---|---|---|---|
| `HRM_A` | `A` | Left Pinky | Control (`KC_LCTL`) |
| `HRM_S` | `R` | Left Ring | Option / Alt (`KC_LALT`) |
| `HRM_D` | `S` | Left Middle | Command (`KC_LGUI`) |
| `HRM_F` | `T` | Left Index | Shift (`KC_LSFT`) |
| `HRM_J` | `N` | Right Index | Shift (`KC_LSFT`) |
| `HRM_K` | `E` | Right Middle | Command (`KC_LGUI`) |
| `HRM_L` | `I` | Right Ring | Option / Alt (`KC_LALT`) |
| `HRM_SCLN` | `O` | Right Pinky | Control (`KC_LCTL`) |

*(Note: `HRM_H` remains in `hrm.h`/`hrm.c` with `modifier = 0` as a guard tested by the unit suite; `keymap.c` uses plain `KC_H`.)*

---

## 2. How Modifiers Activate

### Primary Thumb Gating
Three exact Layer-Tap keycodes drive thumb tracking in `hrm.c`:
- **Left Primary Thumb (`HRM_THUMB_1_KEYCODE`)**: `LT(1, KC_BSPC)` — tap = Backspace, hold = Layer 1 (Numbers + Mirrored Symbols + Left HRMs).
- **Right Primary Thumb (`HRM_THUMB_2_KEYCODE`)**: `LT(2, KC_SPACE)` — tap = Space, hold = Layer 2 (Mirrored Symbols/Numbers + Right HRMs).
- **Layer 1 Right Primary Thumb (`HRM_THUMB_2_ALT_KEYCODE`)**: `LT(2, KC_0)` — tap = `0`, hold = Layer 2 (combined with Layer 1 activates **Layer 4** via `update_tri_layer_state(state, 1, 2, 4)`).

### Activation Modes
1. **Thumb-First (0ms)**: Hold a primary thumb → press an HRM key → modifier registers immediately at 0ms (ideal for `Cmd+Click`, `Shift+Click`, `Cmd+Opt+$`, `Cmd+Ctrl+4`).
2. **Home-Row First**: Press an HRM key on Layer 0 → press a primary thumb within 180ms → cancels the Auto Shift timer and activates the modifier cleanly.
3. **Option A Persistence**: Once activated, a modifier stays held until you physically release the HRM key, even if you release the thumb key first.
4. **Layer 4 Opposite-Hand Letter Resolution**: Hold **both** primary thumbs (in either order) to enter Layer 4 (Mod-Alpha). Hold modifiers on one hand (e.g., right `E`+`I` for `Cmd+Opt`), then press any key on the opposite hand — including opposite-hand HRM keys (`A R S T` or `N E I O`) — which automatically resolve as **letters** (`Cmd+Opt+A`, `Cmd+Ctrl+J`).

---

## 3. Auto Shift, One-Shot Shift & Repeat Key

### Auto Shift (`AUTO_SHIFT_ENABLE = yes`, `AUTO_SHIFT_TIMEOUT 180`)
- **Layer 0 normal typing**: Quick tap (<180ms) outputs lowercase; holding ≥180ms outputs uppercase using **weak Shift** (so it never clears a physically held Shift key).
- **Full toggle (`AS_TOGG`)**: Mapped on the **right red thumb** (Layer 0) and on Layer 3. Toggling Auto Shift off disables it for **all** keys, including `HRM_A..HRM_SCLN` (`hrm_autoshift_callback()` checks `get_autoshift_state()`).
- **Active-modifier bypass**: Holding `MT(MOD_LGUI, KC_ENTER)`, `OSM(MOD_LSFT)`, `KC_LEFT_CTRL`, `KC_LEFT_ALT`, or an Option A cross-hand mod bypasses the 180ms timer and registers the HRM letter immediately on press.
- **Roll protection**: Regular keys flush pending HRM keys chronologically (`hrm_flush_pending_keys()`), and HRM presses call `process_auto_shift(KC_NO, record)` so buffered regular Auto-Shifted keys flush in press order.

### Right Middle Thumb `OSM(MOD_LSFT)`
- **Hold**: Behaves identically to standard Shift.
- **Tap**: Capitalizes the next key pressed (including `HRM_*` keys and `Enter` for two-thumb `Shift+Enter`), then clears automatically (or times out after `ONESHOT_TIMEOUT` = 3000ms).

### Repeat Key (`REPEAT_KEY_ENABLE = yes`, `QK_REP`)
- Placed on Layer 0 directly to the right of `↑`.
- `remember_last_key_user()` ignores HRM modifier holds on Layers 1/2/4, and repeating an HRM letter bypasses the HRM state machine for 0ms output.

---

## 4. Per-Key Tap/Hold Timing Configuration

### `AWDDG/config.h`
```c
#define PERMISSIVE_HOLD_PER_KEY
#define QUICK_TAP_TERM 0
#define QUICK_TAP_TERM_PER_KEY
#define TAPPING_TERM_PER_KEY
#define ONESHOT_TIMEOUT 3000
#define AUTO_SHIFT_TIMEOUT 180
```

### `AWDDG/keymap.c` Callbacks
| Callback | Setting | Purpose |
|---|---|---|
| `get_permissive_hold()` | `false` for `LT(2, KC_0)`, `true` for all others | Prevents fast numpad rolls (`07`, `09`, `105`) from misfiring into Layer 2/4 while keeping responsive holds elsewhere |
| `get_tapping_term()` | `150ms` for `LT(2, KC_0)`, `200ms` default | Snappy symmetric Layer 4 entry when pressing Left Thumb → Right Thumb |
| `get_quick_tap_term()` | `150ms` for `LT(1, KC_BSPC)`, `0ms` for all others | Allows tap-then-hold Backspace auto-repeat while keeping rapid `Space → Layer 2` and `Tab → Layer 3` instant |

### `AWDDG/rules.mk`
```makefile
AUTO_SHIFT_ENABLE = yes
DEFERRED_EXEC_ENABLE = yes
REPEAT_KEY_ENABLE = yes
SRC += hrm.c
```
