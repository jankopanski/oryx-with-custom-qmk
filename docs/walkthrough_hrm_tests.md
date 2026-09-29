# Walkthrough: HRM Unit Tests & Bug Fixes

We have extracted the Home-Row Mod (HRM) state machine into a standalone module, created a comprehensive QMK unit test suite running via Google Test, implemented the fixes for **Bug 1 (Key Inversion & Rolling Order)**, **Bug 2 (Immediate Modifier Activation on Thumb-First)**, **Bug 5 (Option A: Cross-Hand Shortcuts Across Layer Switches)**, and **Bug 6 (HRM_H Auto-Shift Fallback)**.

All 11 unit tests pass in 4 ms, and the Moonlander firmware compiles cleanly with zero warnings or errors.

---

## 1. Git Commit History

All changes have been committed cleanly across atomic commits in [`oryx-with-custom-qmk`](file:///Users/jankopanski/Moonlander/oryx-with-custom-qmk):

| Commit Hash | Commit Message | Description |
| :--- | :--- | :--- |
| `30d53c7` | `docs: add comprehensive layout analysis and HRM unit test plan` | Preserved design analysis and test plan in `docs/` |
| `9e4c5b6` | `refactor(AWDDG): extract HRM state machine into standalone module` | Extracted `hrm.h` & `hrm.c`; simplified `keymap.c` |
| `1104030` | `test(hrm): add QMK unit test harness and baseline tests` | Added `tests/hrm/` and `./test.sh` test runner |
| `c52b115` | `fix(hrm): resolve Bug 1 key inversion and rolling order` | Added eager pending flush and chronological release ordering |
| `93b5c3c` | `feat(hrm): support cross-hand shortcuts and immediate modifier on Thumb-First` | Added immediate modifier on Thumb-First and cross-hand tests |
| `d5312c0` | `feat(hrm): resolve HRM_H stuck state and add remaining edge-case tests` | Fallback to auto-shift for `HRM_H` and added 4 edge-case tests |

---

## 2. Implemented Features & Bug Fixes

### A. Bug 1: Key Inversion & Rolling Order (Fixed)
- **Fast Rolling ("from")**: When typing fast, `HRM_F` enters `PENDING` (180ms window). When regular `KC_R` is pressed, `hrm_flush_pending_keys()` eagerly flushes `HRM_F` as a tap (`f`), then types `r`. Result is **"fr"**, never "rf".
- **Inverted Release Rolling**: If two HRM keys are rolled (`S` then `D`) and released out of order (`D` released before `S`), `hrm_handle_release()` flushes the older pending key first, emitting **"sd"** in press order.

### B. Bug 2: Immediate Modifier Activation on Thumb-First (Fixed)
- When a thumb key is already held (or the active layer is Layer 1 or 2), pressing an HRM key immediately registers the modifier at 0ms. There is zero delay, making mouse clicks and quick shortcut keypresses instantaneous.

### C. Bug 5: Cross-Hand Shortcuts Across Layer Switches (Option A Supported)
- **Option A Behavior**: Modifiers persist across layer changes and thumb releases until physical release of the HRM key.
- **Workflow Supported**:
  1. Hold Right Thumb (`LT(2, KC_SPACE)`) $\rightarrow$ enters Layer 2.
  2. Press Right Pinky (`HRM_SCLN`) $\rightarrow$ registers `Ctrl` immediately.
  3. Release Right Thumb $\rightarrow$ keyboard drops back to Layer 0 while `Ctrl` remains held.
  4. Press Left Hand `C` $\rightarrow$ host receives `Ctrl + C`.
  5. Release Left Hand `C`.
  6. Release Right Pinky $\rightarrow$ `Ctrl` unregisters cleanly; **no semicolon is typed**.

### D. Bug 6: HRM_H Auto-Shift Fallback (Fixed)
- `HRM_H` has no modifier assigned (`state->modifier == 0`). If held past 180ms while a thumb key is held, it falls back to auto-shift (`Shift + H`, state `HRM_STATE_TAP_FIRED`) instead of getting stuck in `PENDING` with an invalid token.

### E. Layer-Independent Release by Matrix Position
- Releasing an HRM key uses `hrm_get_state_by_position(row, col)`, looking up the active HRM key by its physical matrix coordinates. Even if a layer switch occurred while the key was held and the new layer has a different keycode at that position, the modifier releases cleanly without stuck keys.

---

## 3. Unit Test Suite (11 Tests)

All 11 unit tests in [`tests/hrm/test_hrm.cpp`](file:///Users/jankopanski/Moonlander/oryx-with-custom-qmk/tests/hrm/test_hrm.cpp) pass:

| # | Test Name | Behavior Verified | Result |
|---|:---|:---|:---:|
| 1 | `quick_tap_sends_letter` | Quick tap (< 180ms) sends lowercase letter | **PASSED** (0 ms) |
| 2 | `hold_past_timeout_sends_shifted_letter` | Hold (> 180ms) on Layer 0 sends Shift + uppercase letter | **PASSED** (0 ms) |
| 3 | `home_row_first_modifier_activation` | Press HRM $\rightarrow$ press thumb $\rightarrow$ tap target key: resolves mod via `PERMISSIVE_HOLD` | **PASSED** (0 ms) |
| 4 | `fix_bug1_fast_roll_key_inversion` | Fast roll `HRM_F` + `KC_R`: eager flush guarantees "fr", never "rf" | **PASSED** (0 ms) |
| 5 | `fix_bug1_inverted_release_roll` | Press `S` $\rightarrow$ Press `D` $\rightarrow$ Release `D` $\rightarrow$ Release `S`: outputs "sd" | **PASSED** (0 ms) |
| 6 | `cross_hand_shortcut_ctrl_c_option_a` | Right Thumb $\rightarrow$ Right Pinky Ctrl $\rightarrow$ Thumb release $\rightarrow$ Left C: outputs `Ctrl+C` | **PASSED** (0 ms) |
| 7 | `cross_hand_shortcut_thumb_held_throughout` | Cross-hand shortcut with thumb held throughout | **PASSED** (0 ms) |
| 8 | `bug4_autoshift_prevents_modifier` | Holding past 180ms fires Shift+A; subsequent thumb press does NOT activate Ctrl | **PASSED** (0 ms) |
| 9 | `modifier_chord_two_hrm_keys` | Chording two HRM keys (Cmd + Shift) with thumb emits combined chord | **PASSED** (0 ms) |
| 10 | `hrm_h_with_thumb_no_stuck_state` | Holding `HRM_H` with thumb falls back to auto-shift and returns to IDLE | **PASSED** (0 ms) |
| 11 | `release_by_position_cross_layer` | Matrix position tracking releases modifier even if layer changes underneath | **PASSED** (0 ms) |

**Total Execution Time: 4 ms.**

---

## 4. How to Run the Tests

To run the complete test suite locally:
```bash
cd ~/Moonlander/oryx-with-custom-qmk
./test.sh
```

To run a specific test filter:
```bash
cd ~/Moonlander/qmk_firmware
./.build/test/hrm.elf --gtest_filter=Hrm.cross_hand_shortcut_ctrl_c_option_a
```

---

## 5. How to Compile & Flash Firmware

1. **Compile**:
   ```bash
   cd ~/Moonlander/qmk_firmware
   export PATH="$HOME/.local/bin:$HOME/Library/Application Support/qmk/bin:$PATH"
   make zsa/moonlander/reva:AWDDG
   ```
   The binary is generated at: `~/Moonlander/qmk_firmware/zsa_moonlander_reva_AWDDG.bin` (Size: 63,108 bytes).

2. **Flash**:
   - Connect your Moonlander keyboard via USB.
   - Open **Keymapp**, click **Flash Firmware**, and select `zsa_moonlander_reva_AWDDG.bin`.
