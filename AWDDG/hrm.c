#include "hrm.h"

// State array for all home-row keys
hrm_key_state_t hrm_states[HRM_COUNT] = {
    {HRM_A,    KC_A,    KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_S,    KC_S,    KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_D,    KC_D,    KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_F,    KC_F,    KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_H,    KC_H,    0,       0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0}, // No modifier
    {HRM_J,    KC_J,    KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_K,    KC_K,    KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_L,    KC_L,    KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
    {HRM_SCLN, KC_SCLN, KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN, 0, 0},
};

bool thumb_1_pressed = false;
bool thumb_2_pressed = false;

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

// Auto-shift callback - fires when AUTO_SHIFT_TIMEOUT expires
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg) {
  hrm_key_state_t *state = (hrm_key_state_t *)cb_arg;

  // Only fire if still in pending state
  if (state->state == HRM_STATE_PENDING) {
    if ((thumb_1_pressed || thumb_2_pressed ||
         get_highest_layer(layer_state) == 1 ||
         get_highest_layer(layer_state) == 2) && state->modifier != 0) {
      // Activate modifier instead of auto-shifting
      register_code(state->modifier);
      state->state = HRM_STATE_MODIFIER_ACTIVE;
    } else {
      // Hold shifted version of the key to support repeat
      register_code(KC_LSFT);
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
  // If thumb is already pressed or on layer 1/2 (Thumb-First mode):
  // Activate modifier immediately without waiting 180ms!
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
    // Released before timeout - send lowercase letter
    tap_code(state->letter_keycode);
    break;

  case HRM_STATE_SETTLED_TAP:
    // Already tapped earlier during roll/flush; nothing to do
    break;

  case HRM_STATE_TAP_FIRED:
    // Unregister codes for auto-shift
    unregister_code(state->letter_keycode);
    unregister_code(KC_LSFT);
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

// Central processing logic for HRM
bool hrm_process_record(uint16_t keycode, keyrecord_t *record) {
  // Track physical press state of thumb keys
  if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
    uint8_t layer = QK_LAYER_TAP_GET_LAYER(keycode);
    if (layer == 1) {
      thumb_1_pressed = record->event.pressed;
    } else if (layer == 2) {
      thumb_2_pressed = record->event.pressed;
    }
  }

  if (record->event.pressed) {
    // Force activation on thumb key press to support Home-Row First with LT keys
    if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
      uint8_t layer = QK_LAYER_TAP_GET_LAYER(keycode);
      if (layer == 1 || layer == 2) {
        hrm_activate_modifiers();
      }
    }

    // Check if key is pressed while already on Layer 1 or 2
    uint8_t current_layer = get_highest_layer(layer_state);
    if (current_layer == 1 || current_layer == 2) {
      hrm_activate_modifiers();
    }
  }

  // Handle custom home-row mod keycodes on press
  if (record->event.pressed) {
    hrm_key_state_t *hrm_state = hrm_get_state(keycode);
    if (hrm_state != NULL) {
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
