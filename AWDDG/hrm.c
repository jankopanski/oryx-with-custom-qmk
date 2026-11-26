#include "hrm.h"

// State array for all home-row keys
hrm_key_state_t hrm_states[HRM_COUNT] = {
    {HRM_A,    KC_A,    KC_LCTL, true,  0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_S,    KC_S,    KC_LALT, true,  0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_D,    KC_D,    KC_LGUI, true,  0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_F,    KC_F,    KC_LSFT, true,  0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_H,    KC_H,    0,       false, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0}, // No modifier
    {HRM_J,    KC_J,    KC_LSFT, false, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_K,    KC_K,    KC_LGUI, false, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_L,    KC_L,    KC_LALT, false, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_SCLN, KC_SCLN, KC_LCTL, false, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
};

bool thumb_1_pressed = false;
bool thumb_2_pressed = false;

static bool hrm_mod_layer_active(void) {
  uint8_t layer = get_highest_layer(layer_state);
  return layer == 1 || layer == 2 || layer == 4;
}

static bool hrm_is_thumb_2_keycode(uint16_t keycode) {
  return keycode == HRM_THUMB_2_KEYCODE || keycode == HRM_THUMB_2_ALT_KEYCODE;
}

// Helper to find state by custom keycode
hrm_key_state_t *hrm_get_state(uint16_t keycode) {
  for (int i = 0; i < HRM_COUNT; i++) {
    if (hrm_states[i].custom_keycode == keycode) {
      return &hrm_states[i];
    }
  }
  return NULL;
}

// Helper to find state by matrix position
hrm_key_state_t *hrm_get_state_by_position(uint8_t row, uint8_t col) {
  for (int i = 0; i < HRM_COUNT; i++) {
    if (hrm_states[i].state != HRM_STATE_IDLE && hrm_states[i].row == row && hrm_states[i].col == col) {
      return &hrm_states[i];
    }
  }
  return NULL;
}

// Returns true if any active HRM modifier is held on the opposite hand
bool hrm_opposite_hand_mod_active(bool is_left) {
  for (int i = 0; i < HRM_COUNT; i++) {
    if (hrm_states[i].state == HRM_STATE_MODIFIER_ACTIVE &&
        hrm_states[i].modifier != 0 &&
        hrm_states[i].is_left != is_left) {
      return true;
    }
  }
  return false;
}

// Auto-shift callback - fires when AUTO_SHIFT_TIMEOUT expires
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg) {
  hrm_key_state_t *state = (hrm_key_state_t *)cb_arg;

  // Only fire if still in pending state
  if (state->state == HRM_STATE_PENDING) {
    if ((thumb_1_pressed || thumb_2_pressed || hrm_mod_layer_active()) &&
        state->modifier != 0) {
      // Activate modifier instead of auto-shifting
      register_code(state->modifier);
      state->state = HRM_STATE_MODIFIER_ACTIVE;
    } else {
#ifdef AUTO_SHIFT_ENABLE
      if (!get_autoshift_state()) {
        // Auto Shift is toggled off: leave PENDING so release sends lowercase
        // (and a subsequent thumb press can still turn it into a modifier).
        state->token = INVALID_DEFERRED_TOKEN;
        return 0;
      }
#endif
      // Hold shifted version of the key via weak Shift so physical Shift is never clobbered
      add_weak_mods(MOD_BIT(KC_LSFT));
      register_code(state->letter_keycode);

      // Transition to tap fired state
      state->state = HRM_STATE_TAP_FIRED;
    }
    state->token = INVALID_DEFERRED_TOKEN;
  }

  return 0; // Don't reschedule
}

// Handle home-row key press
void hrm_handle_press(hrm_key_state_t *state, uint8_t row, uint8_t col) {
  // Opposite-hand letter resolution (Layer 4 Mod-Alpha & cross-hand chords):
  // If an HRM modifier is already held on the opposite hand, send this key as a letter immediately.
  if (state->modifier != 0 && hrm_opposite_hand_mod_active(state->is_left)) {
    register_code(state->letter_keycode);
    state->state = HRM_STATE_DIRECT_KEY;
    state->row = row;
    state->col = col;
    state->press_time = timer_read32();
    return;
  }

  // If thumb is already pressed or on layer 1/2/4 (Thumb-First mode):
  // Activate modifier immediately without waiting 180ms!
  if ((thumb_1_pressed || thumb_2_pressed || hrm_mod_layer_active()) &&
      state->modifier != 0) {
    register_code(state->modifier);
    state->state = HRM_STATE_MODIFIER_ACTIVE;
    state->row = row;
    state->col = col;
    state->press_time = timer_read32();
    return;
  }

  // Active modifier bypass (Cmd/Enter, physical Shift/Ctrl/Alt, OSM, Option A cross-hand mods):
  // Bypass HRM delay and Auto Shift so shortcuts register immediately on press.
  uint8_t active_mods = get_mods();
#ifndef NO_ACTION_ONESHOT
  active_mods |= get_oneshot_mods();
#endif
  if (active_mods != 0) {
    register_code(state->letter_keycode);
#ifndef NO_ACTION_ONESHOT
    clear_oneshot_mods();
#endif
    state->state = HRM_STATE_DIRECT_KEY;
    state->row = row;
    state->col = col;
    state->press_time = timer_read32();
    return;
  }

  state->press_time = timer_read32();
  state->state = HRM_STATE_PENDING;
  state->row = row;
  state->col = col;

  // Schedule auto-shift callback
  state->token = defer_exec(AUTO_SHIFT_TIMEOUT, hrm_autoshift_callback, state);
}

// Handle home-row key release
void hrm_handle_release(hrm_key_state_t *state) {
  // Cancel any pending timer
  if (state->token != INVALID_DEFERRED_TOKEN) {
    cancel_deferred_exec(state->token);
    state->token = INVALID_DEFERRED_TOKEN;
  }

  switch (state->state) {
  case HRM_STATE_PENDING:
    // Inverted release roll protection:
    // If an older HRM key is still pending, flush it first!
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
    // Released before timeout (or while Auto Shift is off) - send lowercase letter
    tap_code(state->letter_keycode);
    break;

  case HRM_STATE_SETTLED_TAP:
    // Already tapped earlier during roll/flush; nothing to do
    break;

  case HRM_STATE_TAP_FIRED:
    // Unregister codes for auto-shift (weak shift preserves any physical Shift)
    del_weak_mods(MOD_BIT(KC_LSFT));
    unregister_code(state->letter_keycode);
    break;

  case HRM_STATE_DIRECT_KEY:
    // Unregister directly-registered letter (active-modifier / opposite-hand bypass)
    unregister_code(state->letter_keycode);
    break;

  case HRM_STATE_MODIFIER_ACTIVE:
    // Deactivate modifier
    if (state->modifier != 0) {
      unregister_code(state->modifier);
    }
    break;

  default:
    break;
  }

  state->state = HRM_STATE_IDLE;
}

// Transition pending home-row keys to modifier mode
void hrm_activate_modifiers(void) {
  for (int i = 0; i < HRM_COUNT; i++) {
    hrm_key_state_t *state = &hrm_states[i];

    // If key is pending and has a modifier assigned
    if (state->state == HRM_STATE_PENDING && state->modifier != 0) {
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

// Flush all pending HRM keys in chronological order of press
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

#ifdef REPEAT_KEY_ENABLE
// Do not remember HRM presses that act as modifiers (Layers 1/2/4 without opposite-hand mod)
bool remember_last_key_user(uint16_t keycode, keyrecord_t *record, uint8_t *remembered_mods) {
  hrm_key_state_t *hrm_state = hrm_get_state(keycode);
  if (hrm_state != NULL && hrm_state->modifier != 0) {
    if ((thumb_1_pressed || thumb_2_pressed || hrm_mod_layer_active()) &&
        !hrm_opposite_hand_mod_active(hrm_state->is_left)) {
      return false;
    }
  }
  return true;
}
#endif

// Central processing logic for HRM
bool hrm_process_record(uint16_t keycode, keyrecord_t *record) {
#ifdef REPEAT_KEY_ENABLE
  // Fast-path when Repeat Key replays an HRM keycode: send the letter immediately
  // without mutating HRM state/matrix tracking or waiting on the Auto Shift timer.
  if (get_repeat_key_count() != 0) {
    hrm_key_state_t *hrm_state = hrm_get_state(keycode);
    if (hrm_state != NULL) {
      if (record->event.pressed) {
        hrm_flush_pending_keys();
#ifdef AUTO_SHIFT_ENABLE
        process_auto_shift(KC_NO, record);
#endif
        register_code(hrm_state->letter_keycode);
      } else {
        unregister_code(hrm_state->letter_keycode);
      }
      return false;
    }
  }
#endif

  // Track only the actual thumb layer-tap keycodes, and distinguish hold vs tap
  if (keycode == HRM_THUMB_1_KEYCODE || hrm_is_thumb_2_keycode(keycode)) {
    if (record->tap.count == 0) {
      // Actual hold: update thumb tracking and activate pending HRM modifiers
      if (keycode == HRM_THUMB_1_KEYCODE) {
        thumb_1_pressed = record->event.pressed;
      } else {
        thumb_2_pressed = record->event.pressed;
      }
      if (record->event.pressed) {
        hrm_activate_modifiers();
      }
      return true;
    } else {
      // Thumb key was tapped (Bspc / Space / 0): do NOT activate modifiers;
      // flush pending HRM keys on press so fast rolls output letter + Bspc/Space/0.
      if (record->event.pressed) {
        hrm_flush_pending_keys();
      }
      return true;
    }
  }

  if (record->event.pressed) {
    // Check if key is pressed while already on Layer 1, 2, or 4
    if (hrm_mod_layer_active()) {
      hrm_activate_modifiers();
    }
  }

  // Handle custom home-row mod keycodes on press
  if (record->event.pressed) {
    hrm_key_state_t *hrm_state = hrm_get_state(keycode);
    if (hrm_state != NULL) {
#ifdef AUTO_SHIFT_ENABLE
      // Flush any in-progress QMK Auto Shift regular key before handling HRM press
      // (since we return false below, QMK's process_auto_shift will not see this press)
      process_auto_shift(KC_NO, record);
#endif
      hrm_handle_press(hrm_state, record->event.key.row, record->event.key.col);
      return false; // Skip further processing for HRM keys
    }

    // A regular key (non-HRM, non-thumb) was pressed while on Layer 0:
    // FIX FOR BUG 1: Eagerly flush all pending HRM keys in order of press before this key!
    hrm_flush_pending_keys();
  } else {
    // Handle release by matrix position to prevent state leaks across layers
    hrm_key_state_t *hrm_state = hrm_get_state_by_position(record->event.key.row, record->event.key.col);
    if (hrm_state != NULL) {
      hrm_handle_release(hrm_state);
      return false; // Skip further processing for HRM keys
    }
  }

  return true;
}
