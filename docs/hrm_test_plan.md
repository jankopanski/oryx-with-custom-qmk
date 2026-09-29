# HRM Unit Test & Bug Fix Implementation Plan

## Goal

Add comprehensive unit tests for the custom home-row mod (HRM) state machine using QMK's built-in Google Test framework, **implement the fix for Bug 1 (Key Inversion & Out-of-Order Rolling)**, and **support cross-hand modifier shortcuts across layer transitions (Option A)**. All changes and test code reside permanently in the `oryx-with-custom-qmk` repository across **4 atomic git commits**.

---

## Architectural Decisions & Design Alignment

### 1. Repository Organization & Version Control
All source code and test files are kept and tracked in `~/Moonlander/oryx-with-custom-qmk/`:
- `AWDDG/`: Keymap definitions and extracted HRM module (`hrm.h`, `hrm.c`, `keymap.c`).
- `tests/hrm/`: The complete unit test suite (`test.mk`, `config.h`, `test_hrm.cpp`).
- `test.sh`: One-command script that syncs sources to `~/Moonlander/qmk_firmware/` and executes `make test:hrm`.

### 2. Bug 1 Fix: Eager Flush on Regular Key Press
In fast typing (e.g., typing "from"):
- Pressing `HRM_F` enters `HRM_STATE_PENDING` (starts 180ms timer).
- When a subsequent regular key (like `KC_R`) is pressed while an HRM key is pending on Layer 0:
  1. We know `HRM_F` cannot be part of a modifier chord (modifiers require a thumb key, and letter keys in a shortcut are pressed after the thumb, not before).
  2. `HRM_F` is immediately flushed as a tap (`tap_code(KC_F)`) and marked `HRM_STATE_SETTLED_TAP`.
  3. Its auto-shift timer is cancelled.
  4. `KC_R` is then processed normally by QMK.
  5. When `HRM_F` is physically released later, its release event is ignored.
- **Result**: The host receives "f" then "r" in true press order, eliminating key inversion.

### 3. Inverted Release Roll Protection
If two HRM keys are pressed in sequence (`HRM_S` at t=0, `HRM_D` at t=30), but released in reverse order (`HRM_D` at t=50, `HRM_S` at t=80):
- Releasing `HRM_D` flushes any older pending HRM key (`HRM_S`) first, then taps `HRM_D`.
- Output is "sd", preserving press order.

### 4. Bug 5 Decision: Option A (Modifier Persists Until HRM Key Release)
**Selected Decision**: When a thumb key activates a modifier on an HRM key, the modifier remains active as long as the HRM key is physically held down, even if the thumb key is released and the keyboard returns to Layer 0.
- When the HRM key is finally released, the modifier unregisters cleanly, and **no letter is typed**.
- **Crucial for Cross-Hand Shortcuts**:
  1. Press Right Thumb (`LT(2, KC_SPACE)`) $\rightarrow$ enters Layer 2.
  2. Press Right Pinky (`HRM_SCLN`) $\rightarrow$ registers `Control` immediately.
  3. Release Right Thumb $\rightarrow$ keyboard returns to Layer 0 (Base Alphas) while `Control` stays held.
  4. Press Left Hand `C` (`KC_C`) $\rightarrow$ host receives `Control + C`!
  5. Release Left Hand `C`.
  6. Release Right Pinky $\rightarrow$ `Control` unregisters cleanly, no semicolon (`;`) emitted.

---

## 4-Commit Staged Execution Plan

### Commit 1: Refactor — Extract HRM Logic into Standalone Module
- **Purpose**: Pure modularization with zero behavioral changes.
- **Files Modified / Created**:
  - `AWDDG/hrm.h` [NEW]
  - `AWDDG/hrm.c` [NEW]
  - `AWDDG/keymap.c` [MODIFIED]
- **Verification**:
  - Sync to `~/Moonlander/qmk_firmware/keyboards/zsa/moonlander/keymaps/AWDDG/`.
  - Compile firmware: `make zsa/moonlander/reva:AWDDG`. Verify compilation succeeds without warnings.

### Commit 2: Test Infrastructure — Add QMK Unit Test Harness & Baseline Tests
- **Purpose**: Set up deterministic in-memory testing using QMK Google Test framework.
- **Files Modified / Created**:
  - `tests/hrm/test.mk` [NEW]
  - `tests/hrm/config.h` [NEW]
  - `tests/hrm/test_hrm.cpp` [NEW] (Baseline tests: quick tap, 180ms auto-shift, home-row-first modifier activation)
  - `test.sh` [NEW] (Executable runner script)
- **Verification**:
  - Run `./test.sh`. All baseline tests pass.

### Commit 3: Fix Bug 1 — Key Inversion & Rolling Order
- **Purpose**: Eagerly flush pending HRM keys upon regular keypress; preserve press order on inverted roll releases.
- **Files Modified**:
  - `AWDDG/hrm.c` [MODIFIED]: Add `hrm_flush_pending_keys()` and call it on regular keypress. Add chronological order check in `hrm_handle_release()`.
  - `tests/hrm/test_hrm.cpp` [MODIFIED]: Add `fix_bug1_fast_roll_key_inversion` ("fr") and `fix_bug1_inverted_release_roll` ("sd").
- **Verification**:
  - Run `./test.sh`. Both rolling tests pass along with baseline tests.

### Commit 4: Cross-Hand Shortcuts — Option A Tests & Verification
- **Purpose**: Validate cross-hand shortcuts across layer switches and thumb releases.
- **Files Modified**:
  - `tests/hrm/test_hrm.cpp` [MODIFIED]: Add `cross_hand_shortcut_ctrl_c_option_a` and `cross_hand_shortcut_thumb_held_throughout`.
- **Verification**:
  - Run `./test.sh`. Entire test suite passes.
  - Compile final Moonlander firmware: `make zsa/moonlander/reva:AWDDG`.

---

## Source Implementation Details

### `AWDDG/hrm.h`

```c
#pragma once
#include "quantum.h"

#ifdef ZSA_SAFE_RANGE
#define HRM_KEYCODE_BASE ZSA_SAFE_RANGE
#else
#define HRM_KEYCODE_BASE SAFE_RANGE
#endif

enum hrm_custom_keycodes {
    HRM_A = HRM_KEYCODE_BASE,
    HRM_S,
    HRM_D,
    HRM_F,
    HRM_H,
    HRM_J,
    HRM_K,
    HRM_L,
    HRM_SCLN,
    HRM_SAFE_RANGE,
};

typedef enum {
    HRM_STATE_IDLE,
    HRM_STATE_PENDING,
    HRM_STATE_SETTLED_TAP,    // Flushed early due to roll/regular key press; release ignored
    HRM_STATE_TAP_FIRED,      // Auto-shift fired at 180ms; unregisters Shift+letter on release
    HRM_STATE_MODIFIER_ACTIVE // Modifier currently active (persists until HRM key physically released)
} hrm_state_t;

typedef struct {
    uint16_t custom_keycode;
    uint16_t letter_keycode;
    uint16_t modifier;       // 0 if none (e.g. HRM_H)
    uint32_t press_time;
    hrm_state_t state;
    deferred_token token;
    uint8_t row;
    uint8_t col;
} hrm_key_state_t;

#define HRM_COUNT 9

extern hrm_key_state_t hrm_states[HRM_COUNT];
extern bool thumb_1_pressed;
extern bool thumb_2_pressed;

hrm_key_state_t *hrm_get_state(uint16_t keycode);
hrm_key_state_t *hrm_get_state_by_position(uint8_t row, uint8_t col);
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg);
void hrm_handle_press(hrm_key_state_t *state, uint8_t row, uint8_t col);
void hrm_handle_release(hrm_key_state_t *state);
void hrm_activate_modifiers(void);
void hrm_flush_pending_keys(void);

// Central dispatcher called from process_record_user
bool hrm_process_record(uint16_t keycode, keyrecord_t *record);
```

---

### `AWDDG/hrm.c`

```c
#include "hrm.h"

hrm_key_state_t hrm_states[HRM_COUNT] = {
    {HRM_A,    KC_A,    KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_S,    KC_S,    KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_D,    KC_D,    KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_F,    KC_F,    KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_H,    KC_H,    0,       0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_J,    KC_J,    KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_K,    KC_K,    KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_L,    KC_L,    KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_SCLN, KC_SCLN, KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
};

bool thumb_1_pressed = false;
bool thumb_2_pressed = false;

hrm_key_state_t *hrm_get_state(uint16_t keycode) {
    for (int i = 0; i < HRM_COUNT; i++) {
        if (hrm_states[i].custom_keycode == keycode) {
            return &hrm_states[i];
        }
    }
    return NULL;
}

hrm_key_state_t *hrm_get_state_by_position(uint8_t row, uint8_t col) {
    for (int i = 0; i < HRM_COUNT; i++) {
        if (hrm_states[i].state != HRM_STATE_IDLE &&
            hrm_states[i].row == row &&
            hrm_states[i].col == col) {
            return &hrm_states[i];
        }
    }
    return NULL;
}

uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg) {
    hrm_key_state_t *state = (hrm_key_state_t *)cb_arg;

    if (state->state == HRM_STATE_PENDING) {
        if (thumb_1_pressed || thumb_2_pressed ||
            get_highest_layer(layer_state) == 1 ||
            get_highest_layer(layer_state) == 2) {
            if (state->modifier != 0) {
                register_code(state->modifier);
                state->state = HRM_STATE_MODIFIER_ACTIVE;
            }
        } else {
            register_code(KC_LSFT);
            register_code(state->letter_keycode);
            state->state = HRM_STATE_TAP_FIRED;
        }
        state->token = INVALID_DEFERRED_TOKEN;
    }

    return 0;
}

void hrm_handle_press(hrm_key_state_t *state, uint8_t row, uint8_t col) {
    if ((thumb_1_pressed || thumb_2_pressed ||
         get_highest_layer(layer_state) == 1 ||
         get_highest_layer(layer_state) == 2) && state->modifier != 0) {
        register_code(state->modifier);
        state->state = HRM_STATE_MODIFIER_ACTIVE;
        state->row = row;
        state->col = col;
        state->press_time = timer_read32();
        return;
    }

    state->press_time = timer_read32();
    state->state = HRM_STATE_PENDING;
    state->row = row;
    state->col = col;
    state->token = defer_exec(AUTO_SHIFT_TIMEOUT, hrm_autoshift_callback, state);
}

void hrm_handle_release(hrm_key_state_t *state) {
    if (state->token != INVALID_DEFERRED_TOKEN) {
        cancel_deferred_exec(state->token);
        state->token = INVALID_DEFERRED_TOKEN;
    }

    switch (state->state) {
    case HRM_STATE_PENDING:
        for (int i = 0; i < HRM_COUNT; i++) {
            if (&hrm_states[i] != state &&
                hrm_states[i].state == HRM_STATE_PENDING &&
                hrm_states[i].press_time < state->press_time) {
                if (hrm_states[i].token != INVALID_DEFERRED_TOKEN) {
                    cancel_deferred_exec(hrm_states[i].token);
                    hrm_states[i].token = INVALID_DEFERRED_TOKEN;
                }
                tap_code(hrm_states[i].letter_keycode);
                hrm_states[i].state = HRM_STATE_SETTLED_TAP;
            }
        }
        tap_code(state->letter_keycode);
        break;

    case HRM_STATE_SETTLED_TAP:
        break;

    case HRM_STATE_TAP_FIRED:
        unregister_code(state->letter_keycode);
        unregister_code(KC_LSFT);
        break;

    case HRM_STATE_MODIFIER_ACTIVE:
        if (state->modifier != 0) {
            unregister_code(state->modifier);
        }
        break;

    default:
        break;
    }

    state->state = HRM_STATE_IDLE;
}

void hrm_activate_modifiers(void) {
    for (int i = 0; i < HRM_COUNT; i++) {
        hrm_key_state_t *state = &hrm_states[i];
        if (state->state == HRM_STATE_PENDING && state->modifier != 0) {
            if (state->token != INVALID_DEFERRED_TOKEN) {
                cancel_deferred_exec(state->token);
                state->token = INVALID_DEFERRED_TOKEN;
            }
            register_code(state->modifier);
            state->state = HRM_STATE_MODIFIER_ACTIVE;
        }
    }
}

void hrm_flush_pending_keys(void) {
    while (true) {
        hrm_key_state_t *oldest = NULL;
        for (int i = 0; i < HRM_COUNT; i++) {
            if (hrm_states[i].state == HRM_STATE_PENDING) {
                if (!oldest || hrm_states[i].press_time < oldest->press_time) {
                    oldest = &hrm_states[i];
                }
            }
        }
        if (!oldest) break;

        if (oldest->token != INVALID_DEFERRED_TOKEN) {
            cancel_deferred_exec(oldest->token);
            oldest->token = INVALID_DEFERRED_TOKEN;
        }
        tap_code(oldest->letter_keycode);
        oldest->state = HRM_STATE_SETTLED_TAP;
    }
}

bool hrm_process_record(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
        uint8_t layer = QK_LAYER_TAP_GET_LAYER(keycode);
        if (layer == 1) thumb_1_pressed = record->event.pressed;
        else if (layer == 2) thumb_2_pressed = record->event.pressed;
    }

    if (record->event.pressed) {
        if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
            uint8_t layer = QK_LAYER_TAP_GET_LAYER(keycode);
            if (layer == 1 || layer == 2) {
                hrm_activate_modifiers();
                return true;
            }
        }

        hrm_key_state_t *hrm_state = hrm_get_state(keycode);
        if (hrm_state != NULL) {
            hrm_handle_press(hrm_state, record->event.key.row, record->event.key.col);
            return false;
        }

        hrm_flush_pending_keys();
        return true;
    } else {
        hrm_key_state_t *hrm_state =
            hrm_get_state_by_position(record->event.key.row, record->event.key.col);
        if (hrm_state != NULL) {
            hrm_handle_release(hrm_state);
            return false;
        }
        return true;
    }
}
```

---

### `test.sh`

```bash
#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QMK_DIR="${REPO_DIR}/../qmk_firmware"

export PATH="$HOME/.local/bin:$HOME/Library/Application Support/qmk/bin:$PATH"

echo "=== Syncing keymap and test files to QMK tree ==="
mkdir -p "${QMK_DIR}/keyboards/zsa/moonlander/keymaps/AWDDG"
cp -r "${REPO_DIR}/AWDDG/"* "${QMK_DIR}/keyboards/zsa/moonlander/keymaps/AWDDG/"

mkdir -p "${QMK_DIR}/tests/hrm"
cp -r "${REPO_DIR}/tests/hrm/"* "${QMK_DIR}/tests/hrm/"

echo "=== Running HRM Unit Tests ==="
cd "${QMK_DIR}"
make test:hrm "$@"
```
