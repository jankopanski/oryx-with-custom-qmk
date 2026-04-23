#include QMK_KEYBOARD_H
#include "version.h"
#define MOON_LED_LEVEL LED_LEVEL
#ifndef ZSA_SAFE_RANGE
#define ZSA_SAFE_RANGE SAFE_RANGE
#endif

enum custom_keycodes {
  RGB_SLD = ZSA_SAFE_RANGE,
  HRM_A,    // Home-row mod: A (Ctrl on Layer 1)
  HRM_S,    // Home-row mod: S (Alt on Layer 1)
  HRM_D,    // Home-row mod: D (Cmd on Layer 1)
  HRM_F,    // Home-row mod: F (Shift on Layer 1)
  HRM_H,    // Home-row mod: H (plain on Layer 1)
  HRM_J,    // Home-row mod: J (Shift on Layer 2)
  HRM_K,    // Home-row mod: K (Cmd on Layer 2)
  HRM_L,    // Home-row mod: L (Alt on Layer 2)
  HRM_SCLN, // Home-row mod: ; (Ctrl on Layer 2)
};

// ============================================================================
// Home-Row Mod State Tracking
// ============================================================================

typedef enum {
  HRM_STATE_IDLE,           // Key not pressed
  HRM_STATE_PENDING,        // Pressed, waiting for decision
  HRM_STATE_TAP_FIRED,      // Auto-shift fired, letter sent
  HRM_STATE_MODIFIER_ACTIVE // Modifier mode active
} hrm_state_t;

typedef struct {
  uint16_t custom_keycode; // Custom keycode (HRM_A, HRM_S, etc.)
  uint16_t letter_keycode; // Letter keycode (KC_A, KC_S, etc.)
  uint16_t modifier;    // Modifier keycode (KC_LCTL, KC_LALT, etc.) - 0 if none
  uint32_t press_time;  // When key was pressed
  hrm_state_t state;    // Current state
  deferred_token token; // Deferred execution token for auto-shift timer
  uint8_t row;          // Matrix row
  uint8_t col;          // Matrix column
} hrm_key_state_t;

// State array for all home-row keys
#define HRM_COUNT 9
hrm_key_state_t hrm_states[HRM_COUNT] = {
    {HRM_A, KC_A, KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_S, KC_S, KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_D, KC_D, KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_F, KC_F, KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_H, KC_H, 0, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN}, // No modifier
    {HRM_J, KC_J, KC_LSFT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_K, KC_K, KC_LGUI, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_L, KC_L, KC_LALT, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
    {HRM_SCLN, KC_SCLN, KC_LCTL, 0, HRM_STATE_IDLE, INVALID_DEFERRED_TOKEN},
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

hrm_key_state_t *hrm_get_state_by_position(uint8_t row, uint8_t col) {
  for (int i = 0; i < HRM_COUNT; i++) {
    if (hrm_states[i].state != HRM_STATE_IDLE && hrm_states[i].row == row && hrm_states[i].col == col) {
      return &hrm_states[i];
    }
  }
  return NULL;
}

#define DUAL_FUNC_0 LT(8, KC_F16)
#define DUAL_FUNC_1 LT(2, KC_F22)
#define DUAL_FUNC_2 LT(2, KC_Z)
#define DUAL_FUNC_3 LT(4, KC_P)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_moonlander(
        KC_ESCAPE, KC_1, KC_2, KC_3, KC_4, KC_5, TOGGLE_LAYER_COLOR,
        KC_AUDIO_MUTE, KC_6, KC_7, KC_8, KC_9, KC_0, QK_BOOT, KC_DELETE, KC_Q,
        KC_W, KC_E, KC_R, KC_T, KC_GRAVE, KC_AUDIO_VOL_UP, KC_Y, KC_U, KC_I,
        KC_O, KC_P, KC_TRANSPARENT, KC_BSPC, HRM_A, HRM_S, HRM_D, HRM_F, KC_G,
        KC_TAB, KC_AUDIO_VOL_DOWN, KC_H, HRM_J, HRM_K, HRM_L, HRM_SCLN, KC_QUOTE,
        KC_TRANSPARENT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMMA,
        KC_DOT, KC_SLASH, KC_TRANSPARENT, KC_LEFT_SHIFT, KC_LEFT_CTRL,
        KC_LEFT_ALT, KC_LEFT, KC_RIGHT, TT(3), KC_CAPS, KC_DOWN, KC_UP,
        KC_LEFT_ALT, KC_LEFT_CTRL, KC_LEFT_SHIFT, LT(1, KC_ENTER),
        MT(MOD_LGUI, KC_TAB), KC_LEFT_CTRL, KC_LEFT_ALT, DUAL_FUNC_0,
        LT(2, KC_SPACE)),
    [1] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8,
        KC_F9, KC_F10, KC_F11, KC_F12, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_HOME, KC_PAGE_UP, KC_PLUS, KC_7, KC_8, KC_9, KC_ASTR,
        KC_TRANSPARENT, KC_TRANSPARENT, HRM_A, HRM_S, HRM_D, HRM_F,
        KC_TRANSPARENT, KC_END, KC_PGDN, KC_MINUS, MT(MOD_LSFT, KC_4),
        MT(MOD_LGUI, KC_5), MT(MOD_LALT, KC_6), MT(MOD_LCTL, KC_EQUAL),
        KC_GRAVE, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_UNDS, KC_1, KC_2,
        KC_3, KC_BSLS, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_0),
    [2] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8,
        KC_F9, KC_F10, KC_F11, KC_F12, KC_TRANSPARENT, KC_TRANSPARENT, KC_EXLM,
        KC_AT, KC_HASH, KC_DLR, KC_PERC, KC_HOME, KC_PAGE_UP, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, DUAL_FUNC_1, DUAL_FUNC_2,
        MT(MOD_LGUI, KC_LBRC), DUAL_FUNC_3, KC_PIPE, KC_END, KC_PGDN,
        KC_TRANSPARENT, HRM_J, HRM_K, HRM_L, HRM_SCLN, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_CIRC, KC_RCBR, KC_RBRC, KC_RPRN, KC_AMPR,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT),
    [3] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, QK_BOOT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_MEDIA_PREV_TRACK, KC_MEDIA_STOP,
        KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK, KC_TRANSPARENT,
        KC_TRANSPARENT, OSM(MOD_LCTL), OSM(MOD_LALT), OSM(MOD_LGUI),
        OSM(MOD_LSFT), KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_LEFT, KC_DOWN, KC_UP, KC_RIGHT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_HOME, KC_PGDN,
        KC_PAGE_UP, KC_END, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT),
};

// ============================================================================
// Home-Row Mod Implementation
// ============================================================================

// Auto-shift callback - fires when AUTO_SHIFT_TIMEOUT expires
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg) {
  hrm_key_state_t *state = (hrm_key_state_t *)cb_arg;

  // Only fire if still in pending state
  if (state->state == HRM_STATE_PENDING) {
    if (thumb_1_pressed || thumb_2_pressed || get_highest_layer(layer_state) == 1 || get_highest_layer(layer_state) == 2) {
      // Activate modifier instead of auto-shifting
      if (state->modifier != 0) {
        register_code(state->modifier);
        state->state = HRM_STATE_MODIFIER_ACTIVE;
      }
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
    // Released before timeout - send lowercase letter
    tap_code(state->letter_keycode);
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

layer_state_t layer_state_set_user(layer_state_t state) {
  uint8_t layer = get_highest_layer(state);
  if (layer == 1 || layer == 2) {
    hrm_activate_modifiers();
  }
  return state;
}



extern rgb_config_t rgb_matrix_config;

RGB hsv_to_rgb_with_value(HSV hsv) {
  RGB rgb = hsv_to_rgb(hsv);
  float f = (float)rgb_matrix_config.hsv.v / UINT8_MAX;
  return (RGB){f * rgb.r, f * rgb.g, f * rgb.b};
}

void keyboard_post_init_user(void) { rgb_matrix_enable(); }

const uint8_t PROGMEM ledmap[][RGB_MATRIX_LED_COUNT][3] = {
    [0] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}},

    [1] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}},

    [2] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}},

    [3] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
           {0, 0, 255}, {0, 0, 255}},

};

void set_layer_color(int layer) {
  for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
    HSV hsv = {
        .h = pgm_read_byte(&ledmap[layer][i][0]),
        .s = pgm_read_byte(&ledmap[layer][i][1]),
        .v = pgm_read_byte(&ledmap[layer][i][2]),
    };
    if (!hsv.h && !hsv.s && !hsv.v) {
      rgb_matrix_set_color(i, 0, 0, 0);
    } else {
      RGB rgb = hsv_to_rgb_with_value(hsv);
      rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
  }
}

bool rgb_matrix_indicators_user(void) {
  if (rawhid_state.rgb_control) {
    return false;
  }
  if (!keyboard_config.disable_layer_led) {
    switch (biton32(layer_state)) {
    case 0:
      set_layer_color(0);
      break;
    case 1:
      set_layer_color(1);
      break;
    case 2:
      set_layer_color(2);
      break;
    case 3:
      set_layer_color(3);
      break;
    default:
      if (rgb_matrix_get_flags() == LED_FLAG_NONE) {
        rgb_matrix_set_color_all(0, 0, 0);
      }
    }
  } else {
    if (rgb_matrix_get_flags() == LED_FLAG_NONE) {
      rgb_matrix_set_color_all(0, 0, 0);
    }
  }

  return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  // Track physical press state of thumb keys
  if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
    uint8_t layer = QK_LAYER_TAP_GET_LAYER(keycode);
    if (layer == 1) {
      thumb_1_pressed = record->event.pressed;
    } else if (layer == 2) {
      thumb_2_pressed = record->event.pressed;
    }
  }

  // ============================================================================
  // Home-Row Mod Processing
  // ============================================================================

  // Detect thumb layer keys and activate modifiers for pending home-row keys
  // We check for both the layer-tap keys themselves and when we're on layers 1
  // or 2
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
  } else {
    // Handle release by matrix position to prevent state leaks across layers
    hrm_key_state_t *hrm_state = hrm_get_state_by_position(record->event.key.row, record->event.key.col);
    if (hrm_state != NULL) {
      hrm_handle_release(hrm_state);
      return false; // Skip further processing for HRM keys
    }
  }

  // ============================================================================
  // Standard Keycode Processing
  // ============================================================================

  switch (keycode) {

  case DUAL_FUNC_0:
    if (record->tap.count > 0) {
      if (record->event.pressed) {
        register_code16(LALT(KC_BSPC));
      } else {
        unregister_code16(LALT(KC_BSPC));
      }
    } else {
      if (record->event.pressed) {
        register_code16(KC_LEFT_SHIFT);
      } else {
        unregister_code16(KC_LEFT_SHIFT);
      }
    }
    return false;
  case DUAL_FUNC_1:
    if (record->tap.count > 0) {
      if (record->event.pressed) {
        register_code16(KC_TILD);
      } else {
        unregister_code16(KC_TILD);
      }
    } else {
      if (record->event.pressed) {
        register_code16(KC_LEFT_CTRL);
      } else {
        unregister_code16(KC_LEFT_CTRL);
      }
    }
    return false;
  case DUAL_FUNC_2:
    if (record->tap.count > 0) {
      if (record->event.pressed) {
        register_code16(KC_LCBR);
      } else {
        unregister_code16(KC_LCBR);
      }
    } else {
      if (record->event.pressed) {
        register_code16(KC_LEFT_ALT);
      } else {
        unregister_code16(KC_LEFT_ALT);
      }
    }
    return false;
  case DUAL_FUNC_3:
    if (record->tap.count > 0) {
      if (record->event.pressed) {
        register_code16(KC_LPRN);
      } else {
        unregister_code16(KC_LPRN);
      }
    } else {
      if (record->event.pressed) {
        register_code16(KC_LEFT_SHIFT);
      } else {
        unregister_code16(KC_LEFT_SHIFT);
      }
    }
    return false;
  case RGB_SLD:
    if (rawhid_state.rgb_control) {
      return false;
    }
    if (record->event.pressed) {
      rgblight_mode(1);
    }
    return false;
  }
  return true;
}
