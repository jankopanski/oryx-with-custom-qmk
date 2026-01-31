# Custom Home-Row Mods with Auto-Shift Priority

Implement custom home-row modifier behavior where auto-shift takes priority over tapping term, providing immediate uppercase output at 180ms while still supporting modifier activation via thumb layer switches.

## User Review Required

> [!IMPORTANT]
> **Breaking Change from Standard MT() Behavior**
> 
> This implementation replaces standard QMK `MT()` (Mod-Tap) keys with fully custom keycodes. The new behavior differs significantly:
> - Auto-shift will **immediately output** uppercase letters at 180ms (not deferred to key release)
> - Modifier function only activates when thumb layer switch is pressed (not from holding the key alone)
> - Once auto-shift fires, the key cannot transition to modifier mode

> [!WARNING]
> **Corner Case Decisions**
> 
> The following design decisions have been made for edge cases:
> 
> 1. **Thumb press near auto-shift timeout**: Thumb press cancels auto-shift timer even if pressed 1ms before 180ms timeout
> 2. **Auto-shift already fired**: Once uppercase letter is sent, thumb press has no effect on that key
> 3. **Multiple home-row keys**: All held home-row keys transition to modifier mode when thumb is pressed
> 4. **Release ordering**: Once in modifier mode, key remains modifier until released regardless of layer changes
> 5. **PERMISSIVE_HOLD**: Only applies when on Layers 1/2, not on Layer 0 during normal typing

## Proposed Changes

### Custom Keycode Definitions

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Add custom keycodes for home-row keys:

```c
enum custom_keycodes {
  RGB_SLD = ZSA_SAFE_RANGE,
  HRM_A,    // Home-row mod: A
  HRM_S,    // Home-row mod: S  
  HRM_D,    // Home-row mod: D
  HRM_F,    // Home-row mod: F
  HRM_H,    // Home-row mod: H
  HRM_J,    // Home-row mod: J
  HRM_K,    // Home-row mod: K
  HRM_L,    // Home-row mod: L
  HRM_SCLN, // Home-row mod: ;
};
```

Update keymaps to use custom keycodes on Layers 1 and 2, while Layer 0 keeps plain letter keys.

---

### State Tracking Infrastructure

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Add state tracking structure:

```c
typedef enum {
    HRM_STATE_IDLE,           // Key not pressed
    HRM_STATE_PENDING,        // Pressed, waiting for decision
    HRM_STATE_TAP_FIRED,      // Auto-shift fired, letter sent
    HRM_STATE_MODIFIER_ACTIVE // Modifier mode active
} hrm_state_t;

typedef struct {
    uint16_t keycode;         // Letter keycode (KC_A, KC_S, etc.)
    uint16_t modifier;        // Modifier keycode (KC_LCTL, KC_LALT, etc.)
    uint32_t press_time;      // When key was pressed
    hrm_state_t state;        // Current state
    deferred_token token;     // Deferred execution token for auto-shift timer
} hrm_key_state_t;

// State array for all home-row keys
hrm_key_state_t hrm_states[9]; // One for each HRM_* keycode
```

The state machine ensures proper transitions:
- `IDLE` → `PENDING` on key press
- `PENDING` → `TAP_FIRED` when auto-shift fires (180ms)
- `PENDING` → `MODIFIER_ACTIVE` when thumb pressed (before 180ms)
- `TAP_FIRED` / `MODIFIER_ACTIVE` → `IDLE` on key release

---

### Auto-Shift Timer Implementation

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Implement deferred callback for auto-shift:

```c
// Callback fired when auto-shift timeout expires
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg) {
    hrm_key_state_t *state = (hrm_key_state_t *)cb_arg;
    
    // Only fire if still in pending state
    if (state->state == HRM_STATE_PENDING) {
        // Send shifted version of the key
        register_code(KC_LSFT);
        tap_code(state->keycode);
        unregister_code(KC_LSFT);
        
        // Transition to tap fired state
        state->state = HRM_STATE_TAP_FIRED;
    }
    
    return 0; // Don't reschedule
}
```

This provides the same immediate output behavior as normal keys with auto-shift.

---

### Key Press Handler

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Handle home-row key press:

```c
void hrm_handle_press(hrm_key_state_t *state, uint16_t keycode, uint16_t modifier) {
    state->keycode = keycode;
    state->modifier = modifier;
    state->press_time = timer_read32();
    state->state = HRM_STATE_PENDING;
    
    // Schedule auto-shift callback
    state->token = defer_exec(AUTO_SHIFT_TIMEOUT, hrm_autoshift_callback, state);
}
```

---

### Key Release Handler

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Handle home-row key release:

```c
void hrm_handle_release(hrm_key_state_t *state) {
    // Cancel any pending timer
    if (state->token != INVALID_DEFERRED_TOKEN) {
        cancel_deferred_exec(state->token);
        state->token = INVALID_DEFERRED_TOKEN;
    }
    
    switch (state->state) {
        case HRM_STATE_PENDING:
            // Released before timeout - send lowercase letter
            tap_code(state->keycode);
            break;
            
        case HRM_STATE_TAP_FIRED:
            // Auto-shift already fired, nothing to do
            break;
            
        case HRM_STATE_MODIFIER_ACTIVE:
            // Deactivate modifier
            unregister_code(state->modifier);
            break;
            
        default:
            break;
    }
    
    state->state = HRM_STATE_IDLE;
}
```

---

### Layer Transition Logic

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Detect thumb layer activation and transition pending keys:

```c
void hrm_check_pending_keys_for_modifier(void) {
    for (int i = 0; i < 9; i++) {
        hrm_key_state_t *state = &hrm_states[i];
        
        // If key is pending and hasn't timed out yet
        if (state->state == HRM_STATE_PENDING) {
            uint32_t elapsed = timer_elapsed32(state->press_time);
            
            // Cancel auto-shift timer
            if (state->token != INVALID_DEFERRED_TOKEN) {
                cancel_deferred_exec(state->token);
                state->token = INVALID_DEFERRED_TOKEN;
            }
            
            // Activate modifier
            register_code(state->modifier);
            state->state = HRM_STATE_MODIFIER_ACTIVE;
        }
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Detect thumb layer keys
    if (keycode == LT(1, KC_ENTER) || keycode == LT(2, KC_SPACE)) {
        if (record->event.pressed) {
            // Thumb pressed - transition any pending home-row keys
            hrm_check_pending_keys_for_modifier();
        }
    }
    
    // ... rest of processing
}
```

---

### PERMISSIVE_HOLD Implementation

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Implement permissive hold for modifier mode:

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // ... thumb layer detection ...
    
    // Check for other key presses while home-row keys are active
    uint8_t current_layer = get_highest_layer(layer_state);
    
    if (record->event.pressed && 
        (current_layer == 1 || current_layer == 2)) {
        
        // Check if any home-row keys are in modifier mode
        for (int i = 0; i < 9; i++) {
            if (hrm_states[i].state == HRM_STATE_MODIFIER_ACTIVE) {
                // PERMISSIVE_HOLD: another key pressed, modifier stays active
                // QMK will naturally send modifier + this key
                break;
            }
        }
    }
    
    // ... rest of processing
}
```

---

### Configuration Updates

#### [MODIFY] [config.h](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/config.h)

Ensure deferred execution is available:

```c
// Already defined:
// #define PERMISSIVE_HOLD
// #define AUTO_SHIFT_TIMEOUT 180

// No additional config needed - DEFERRED_EXEC enabled in rules.mk
```

#### [MODIFY] [rules.mk](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/rules.mk)

Enable deferred execution:

```makefile
DEFERRED_EXEC_ENABLE = yes
```

---

## Verification Plan

### Automated Tests

Manual testing with systematic scenarios:

1. **Normal typing on Layer 0**
   - Quick tap: lowercase letter
   - Hold 180ms+: uppercase letter appears immediately
   - Verify repeating behavior works

2. **Modifier activation on Layers 1 & 2**
   - Press home-row, press thumb <180ms: modifier active
   - Verify modifier + other key combinations work
   - Test all 4 home-row modifiers on each hand

3. **Corner cases**
   - Thumb press at 179ms: should cancel auto-shift
   - Auto-shift fires, then thumb press: letter sent, thumb ignored
   - Multiple home-row keys + thumb: all become modifiers
   - Release thumb first: modifiers stay active until home-row released

4. **PERMISSIVE_HOLD**
   - Verify modifier resolves immediately when another key pressed on Layer 1/2
   - Verify normal typing on Layer 0 not affected

### Manual Verification

Test typical usage patterns:
- Fast typing paragraphs
- Keyboard shortcuts (Cmd+C, Ctrl+Alt+Del equivalents)
- Modifier chords (Ctrl+Shift+Letter)
- Holding keys for uppercase in normal typing
