# HRM Unit Test Plan & Suite Catalog

## Overview

The custom Home-Row Mod (HRM) engine (`AWDDG/hrm.h`, `AWDDG/hrm.c`) and per-key tap/hold timing callbacks (`keymap.c`) are verified by **32 automated unit tests** running in QMK's native Google Test harness (`tests/hrm/test_hrm.cpp`).

### Test Harness Features Enabled
- `DEFERRED_EXEC_ENABLE = yes`
- `AUTO_SHIFT_ENABLE = yes` (`AUTO_SHIFT_TIMEOUT 180`)
- `REPEAT_KEY_ENABLE = yes`
- `PERMISSIVE_HOLD_PER_KEY` (`false` for `LT(2, KC_0)`, `true` otherwise)
- `TAPPING_TERM_PER_KEY` (`150` for `LT(2, KC_0)`, `200` otherwise)
- `QUICK_TAP_TERM_PER_KEY` (`150` for `LT(1, KC_BSPC)`, `0` otherwise)
- `ONESHOT_TIMEOUT 3000`
- Tri-Layer (`update_tri_layer_state(state, 1, 2, 4)`)

### Running the Suite
```bash
cd ~/Moonlander/oryx-with-custom-qmk
./test.sh
```
`test.sh` syncs `AWDDG/` and `tests/hrm/` into `~/Moonlander/qmk_firmware/` and runs `make test:hrm`.

---

## Complete Test Suite (32 Tests)

### Baseline & Core Roll Protection (Tests 1–11)
| # | Test Name | Invariant Verified |
|---|:---|:---|
| 1 | `quick_tap_sends_letter` | Quick tap (< 180ms) outputs lowercase letter on release |
| 2 | `hold_past_timeout_sends_shifted_letter` | Hold ≥ 180ms outputs weak-Shift + uppercase letter in a single report |
| 3 | `home_row_first_modifier_activation` | Press HRM → press `LT(1, KC_BSPC)` → tap target resolves modifier via permissive hold |
| 4 | `fix_bug1_fast_roll_key_inversion` | Fast roll `HRM_F` → `KC_R` eagerly flushes `f` before `r` (never inverted) |
| 5 | `fix_bug1_inverted_release_roll` | Press `S` → `D`, release `D` → `S` preserves press order (`sd`) |
| 6 | `cross_hand_shortcut_ctrl_c_option_a` | Modifier persists after thumb release; emits `Ctrl+C` cleanly with no `;` leak |
| 7 | `cross_hand_shortcut_thumb_held_throughout` | Cross-hand shortcut works identically when thumb stays held |
| 8 | `bug4_autoshift_prevents_modifier` | Auto-shifted key (`TAP_FIRED`) cannot retroactively turn into a modifier |
| 9 | `modifier_chord_two_hrm_keys` | Two HRM keys (`Cmd` + `Shift`) chord cleanly |
| 10 | `hrm_h_with_thumb_no_stuck_state` | `HRM_H` (`modifier = 0`) falls back to Auto Shift cleanly and returns to `IDLE` |
| 11 | `release_by_position_cross_layer` | Matrix position release unregisters modifier cleanly across layer divergence |

### Firmware Bug Regression Tests (Tests 12–20)
| # | Test Name | Invariant Verified |
|---|:---|:---|
| 12 | `fix_bug_a_fast_roll_hrm_k_to_space` | Fast roll `HRM_K` (`e`) → `Space` tap emits `k`, `Space` (never `Cmd+Space`) |
| 13 | `fix_bug_a_fast_roll_hrm_scln_to_space` | Fast roll `HRM_SCLN` (`o`) → `Space` tap emits `;`, `Space` (never `Ctrl+Space`) |
| 14 | `fix_bug_a_fast_roll_hrm_d_to_bspc` | Fast roll `HRM_D` (`s`) → `Bspc` tap emits `d`, `Bspc` (never `Cmd+Bspc`) |
| 15 | `fix_bug_d1_cmd_tab_with_hrm_s` | Held `MT(MOD_LGUI, ...)` + `HRM_S` registers immediately, survives early mod release, no 180ms Shift |
| 16 | `fix_bug_d1_physical_shift_with_hrm` | Held physical `KC_LEFT_SHIFT` + `HRM_S` registers immediately and never clobbers physical Shift |
| 17 | `fix_bug_d1_osm_with_hrm` | `OSM(MOD_LCTL)` applies immediately to `HRM_S` and clears cleanly |
| 18 | `fix_bug_d1_option_a_cross_hand_to_hrm_key` | Option A cross-hand `Ctrl` + left `HRM_A` registers `Ctrl+A` immediately with no Auto Shift |
| 19 | `fix_bug_d2_non_hrm_to_hrm_roll_with_autoshift` | Inverted roll `KC_H` → `HRM_K` flushes QMK Auto Shift (`h` then `k`, never `kh` or `Hk`) |
| 20 | `fix_bug_c_quick_tap_then_hold_enters_layer_2` | Rapid tap-then-hold `LT(2, KC_SPACE)` enters Layer 2 instead of repeating Space |

### 5-Layer Ergonomic Layout & Typing-Feel Tests (Tests 21–32)
| # | Test Name | Invariant Verified |
|---|:---|:---|
| 21 | `layer4_cmd_opt_a` | Layer 4 right `Cmd+Opt` + left `HRM_A` emits `Cmd+Opt+A` via opposite-hand resolution |
| 22 | `layer4_cmd_ctrl_j` | Layer 4 left `Cmd+Ctrl` + right `KC_Y` / `HRM_J` emits `Cmd+Ctrl+Y` and `Cmd+Ctrl+J` |
| 23 | `layer4_right_thumb_first` | `LT(2, KC_SPACE)` → `LT(1, KC_BSPC)` activates Layer 4 and exits cleanly |
| 24 | `layer4_left_thumb_first_via_lt2_zero` | `LT(1, KC_BSPC)` → hold `LT(2, KC_0)` (150ms) activates Layer 4 symmetrically |
| 25 | `layer1_zero_fast_roll_no_misfire` | Layer 1 nested roll `LT(2, KC_0)` → `KC_7` sends `0`, `7` and never enters Layer 2/4 |
| 26 | `layer1_cmd_zero_shortcut` | Layer 1 `HRM_D` (`Cmd`) + tap `LT(2, KC_0)` sends `Cmd+0` |
| 27 | `autoshift_off_disables_hrm_autoshift` | `AS_TOGG` disables Auto Shift for HRM keys too; toggling back ON restores it |
| 28 | `osm_shift_with_hrm_and_enter` | `OSM(MOD_LSFT)` + `HRM_A` sends `Shift+A`; `OSM(MOD_LSFT)` + `MT(MOD_LGUI, KC_ENTER)` sends `Shift+Enter` |
| 29 | `repeat_key_repeats_hrm_letter_not_modifier` | `QK_REP` repeats `HRM_K` at 0ms and ignores Layer 2 `HRM_SCLN` modifier holds |
| 30 | `quick_tap_bspc_repeat_vs_space_layer` | Tap-then-hold `LT(1, KC_BSPC)` repeats Backspace (<150ms); `LT(2, KC_SPACE)` enters Layer 2 |
| 31 | `layer1_and_layer2_bspc_on_inner_thumb` | Right inner thumb on L1 and left inner thumb on L2 tap `KC_BSPC` without dropping layers |
| 32 | `layer4_thumb_keys_explicit` | Layer 4 inner thumbs send explicit `KC_LEFT_CTRL` / `KC_LEFT_ALT` (not L1/L2 `KC_BSPC`) |

---

## Test Harness Notes
1. **Apple Clang compatibility (`tests/hrm/test.mk`)**: Uses `-Wno-include-next-absolute-path`, `-Wno-strict-prototypes`, and `-Wno-c++11-narrowing` for upstream QMK compatibility on macOS.
2. **Avoid `KEYMAP_C` collision**: `test.sh` copies `hrm.h` and `hrm.c` directly into `qmk_firmware/tests/hrm/` rather than adding `AWDDG/` to `VPATH`.
3. **Monotonic clock & full state reset**: `Hrm::SetUp()` advances time by `10000ms` per test (preventing `deferred_exec_task()` throttling) and resets `hrm_states`, thumb flags, layers, oneshot mods, Auto Shift state, and Repeat Key state.
