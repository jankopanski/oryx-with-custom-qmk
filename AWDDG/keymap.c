#include QMK_KEYBOARD_H
#include "version.h"
#define MOON_LED_LEVEL LED_LEVEL
#ifndef ZSA_SAFE_RANGE
#define ZSA_SAFE_RANGE SAFE_RANGE
#endif

enum custom_keycodes {
  RGB_SLD = ZSA_SAFE_RANGE,
  // Home-row mod keys (letter on tap, modifier on hold when layer 1/2 active)
  HRM_A,    // Ctrl on hold
  HRM_S,    // Option on hold
  HRM_D,    // Command on hold
  HRM_F,    // Shift on hold
  HRM_J,    // Shift on hold (right hand mirror)
  HRM_K,    // Command on hold
  HRM_L,    // Option on hold
  HRM_SCLN, // Ctrl on hold
};

// Preserve existing dual-function keycodes
#define DUAL_FUNC_0 LT(11, KC_R)
#define DUAL_FUNC_1 LT(5, KC_F21)
#define DUAL_FUNC_2 LT(11, KC_Q)
#define DUAL_FUNC_3 LT(11, KC_F20)

// ============================================================================
// Home-Row Mod State Tracking
// ============================================================================

// Number of home-row mod keys
#define HRM_KEY_COUNT 8

// Index mapping for home-row keys
#define HRM_IDX_A 0
#define HRM_IDX_S 1
#define HRM_IDX_D 2
#define HRM_IDX_F 3
#define HRM_IDX_J 4
#define HRM_IDX_K 5
#define HRM_IDX_L 6
#define HRM_IDX_SCLN 7

// State for each home-row key
typedef struct {
  bool pressed;               // Is key currently pressed?
  uint16_t press_time;        // When was it pressed?
  bool layer_active_on_press; // Was layer 1/2 active when pressed?
  bool decided;               // Has tap/hold decision been made?
  bool is_hold;               // True = modifier mode, False = tap mode
  bool invalidated; // True = thumb released before this key, no action
} hrm_state_t;

static hrm_state_t hrm_states[HRM_KEY_COUNT];

// Track if thumb layer keys are held
static bool thumb_layer_left_held = false;  // LT(1, ...) key
static bool thumb_layer_right_held = false; // LT(2, ...) key

// Helper: Check if either thumb layer key is active
static inline bool is_thumb_layer_active(void) {
  return thumb_layer_left_held || thumb_layer_right_held;
}

// Deferred execution tokens for each HRM key
// Note: 0 is used as "invalid" token since QMK tokens start from 1
#define HRM_TOKEN_INVALID 0
static deferred_token hrm_deferred_tokens[HRM_KEY_COUNT] = {0};

// Keycode to base letter mapping
static const uint16_t hrm_base_keycodes[HRM_KEY_COUNT] = {
    KC_A, KC_S, KC_D, KC_F, KC_J, KC_K, KC_L, KC_SCLN};

// Keycode to modifier mapping (macOS order: Ctrl, Opt, Cmd, Shift)
static const uint16_t hrm_mod_keycodes[HRM_KEY_COUNT] = {
    KC_LEFT_CTRL,  KC_LEFT_ALT, KC_LEFT_GUI, KC_LEFT_SHIFT,
    KC_LEFT_SHIFT, KC_LEFT_GUI, KC_LEFT_ALT, KC_LEFT_CTRL};

// Get HRM index from custom keycode
static int8_t get_hrm_index(uint16_t keycode) {
  switch (keycode) {
  case HRM_A:
    return HRM_IDX_A;
  case HRM_S:
    return HRM_IDX_S;
  case HRM_D:
    return HRM_IDX_D;
  case HRM_F:
    return HRM_IDX_F;
  case HRM_J:
    return HRM_IDX_J;
  case HRM_K:
    return HRM_IDX_K;
  case HRM_L:
    return HRM_IDX_L;
  case HRM_SCLN:
    return HRM_IDX_SCLN;
  default:
    return -1;
  }
}

// Forward declarations
static uint32_t hrm_tapping_term_callback(uint32_t trigger_time, void *cb_arg);
static void hrm_handle_press(int8_t idx);
static void hrm_handle_release(int8_t idx);
static void hrm_activate_modifier(int8_t idx);
static void hrm_activate_tap(int8_t idx);
static void hrm_check_pending_keys_for_hold(void);

// ============================================================================
// HRM Helper Function Implementations
// ============================================================================

// Activate the modifier for an HRM key
static void hrm_activate_modifier(int8_t idx) {
  if (idx < 0 || idx >= HRM_KEY_COUNT)
    return;
  hrm_state_t *state = &hrm_states[idx];
  if (!state->decided) {
    state->decided = true;
    state->is_hold = true;
    register_code(hrm_mod_keycodes[idx]);
  }
}

// Activate the tap (letter) for an HRM key
// Includes Auto Shift support when key was held long enough on base layer
static void hrm_activate_tap(int8_t idx) {
  if (idx < 0 || idx >= HRM_KEY_COUNT)
    return;
  hrm_state_t *state = &hrm_states[idx];
  if (!state->decided && !state->invalidated) {
    state->decided = true;
    state->is_hold = false;

    uint16_t elapsed = timer_elapsed(state->press_time);
    // Auto Shift: if held longer than AUTO_SHIFT_TIMEOUT, send shifted version
    // This only applies when thumb layer is NOT active
    if (!is_thumb_layer_active() && elapsed >= AUTO_SHIFT_TIMEOUT) {
      // Send shifted version
      register_code(KC_LSFT);
      tap_code(hrm_base_keycodes[idx]);
      unregister_code(KC_LSFT);
    } else {
      tap_code(hrm_base_keycodes[idx]);
    }
  }
}

// Deferred callback - called when tapping term expires
static uint32_t hrm_tapping_term_callback(uint32_t trigger_time, void *cb_arg) {
  int8_t idx = (int8_t)(intptr_t)cb_arg;
  if (idx < 0 || idx >= HRM_KEY_COUNT)
    return 0;

  hrm_state_t *state = &hrm_states[idx];

  // Only act if key is still pressed and undecided
  if (state->pressed && !state->decided && !state->invalidated) {
    // If thumb layer is active, decide as hold (modifier)
    if (is_thumb_layer_active()) {
      hrm_activate_modifier(idx);
    }
    // If thumb layer not active on base layer, tapping term expired = do
    // nothing (will decide on release)
  }

  return 0; // Don't repeat
}

// Handle HRM key press
static void hrm_handle_press(int8_t idx) {
  if (idx < 0 || idx >= HRM_KEY_COUNT)
    return;

  hrm_state_t *state = &hrm_states[idx];

  // Initialize state
  state->pressed = true;
  state->press_time = timer_read();
  state->layer_active_on_press = is_thumb_layer_active();
  state->decided = false;
  state->is_hold = false;
  state->invalidated = false;

  // Cancel any existing deferred token for this key
  if (hrm_deferred_tokens[idx] != HRM_TOKEN_INVALID) {
    cancel_deferred_exec(hrm_deferred_tokens[idx]);
  }

  // Schedule tapping term callback
  hrm_deferred_tokens[idx] = defer_exec(TAPPING_TERM, hrm_tapping_term_callback,
                                        (void *)(intptr_t)idx);
}

// Handle HRM key release
static void hrm_handle_release(int8_t idx) {
  if (idx < 0 || idx >= HRM_KEY_COUNT)
    return;

  hrm_state_t *state = &hrm_states[idx];

  // Cancel deferred callback if still pending
  if (hrm_deferred_tokens[idx] != HRM_TOKEN_INVALID) {
    cancel_deferred_exec(hrm_deferred_tokens[idx]);
    hrm_deferred_tokens[idx] = HRM_TOKEN_INVALID;
  }

  if (state->invalidated) {
    // Thumb was released before this key - no action
  } else if (state->decided && state->is_hold) {
    // Was decided as hold - unregister modifier
    unregister_code(hrm_mod_keycodes[idx]);
  } else if (!state->decided) {
    // Not decided yet - if thumb layer not active, emit tap
    if (!is_thumb_layer_active()) {
      hrm_activate_tap(idx);
    } else {
      // Thumb layer IS active but wasn't at press time - this is the
      // "home-row first, then thumb" case. Since we're releasing and
      // thumb is still held, activate modifier briefly then release
      // Actually per requirements: if thumb is STILL held when we release,
      // and we haven't decided, we should have transitioned to hold.
      // But if we get here, something went wrong - safest is to do nothing
    }
  }

  state->pressed = false;
}

// Check all pending HRM keys when thumb layer becomes active
static void hrm_check_pending_keys_for_hold(void) {
  for (int8_t i = 0; i < HRM_KEY_COUNT; i++) {
    hrm_state_t *state = &hrm_states[i];
    if (state->pressed && !state->decided && !state->invalidated) {
      // Key is pressed but undecided - check if within tapping term
      if (timer_elapsed(state->press_time) < TAPPING_TERM) {
        // Within tapping term, transition to hold
        hrm_activate_modifier(i);
      }
    }
  }
}

// Invalidate all pending HRM keys when thumb layer is released
static void hrm_invalidate_pending_keys(void) {
  for (int8_t i = 0; i < HRM_KEY_COUNT; i++) {
    hrm_state_t *state = &hrm_states[i];
    if (state->pressed && !state->decided) {
      // Key is pressed but undecided - invalidate it
      state->invalidated = true;
      // Cancel any pending callback
      if (hrm_deferred_tokens[i] != HRM_TOKEN_INVALID) {
        cancel_deferred_exec(hrm_deferred_tokens[i]);
        hrm_deferred_tokens[i] = HRM_TOKEN_INVALID;
      }
    }
  }
}

// ============================================================================
// Chordal Hold Layout for Moonlander
// ============================================================================
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM =
    LAYOUT_moonlander('L', 'L', 'L', 'L', 'L', 'L', '*', '*', 'R', 'R', 'R',
                      'R', 'R', 'R', 'L', 'L', 'L', 'L', 'L', 'L', 'L', 'R',
                      'R', 'R', 'R', 'R', 'R', 'R', 'L', 'L', 'L', 'L', 'L',
                      'L', 'L', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'L', 'L',
                      'L', 'L', 'L', 'L', 'R', 'R', 'R', 'R', 'R', 'R', 'L',
                      'L', 'L', 'L', 'L', '*', '*', 'R', 'R', 'R', 'R', 'R',
                      '*', '*', '*', '*', '*', '*');

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_moonlander(
        KC_ESCAPE, KC_1, KC_2, KC_3, KC_4, KC_5, TOGGLE_LAYER_COLOR,
        KC_AUDIO_MUTE, KC_6, KC_7, KC_8, KC_9, KC_0, QK_BOOT, KC_DELETE, KC_Q,
        KC_W, KC_E, KC_R, KC_T, KC_GRAVE, KC_AUDIO_VOL_UP, KC_Y, KC_U, KC_I,
        KC_O, KC_P, KC_TRANSPARENT, KC_BSPC, HRM_A, HRM_S, HRM_D, HRM_F, KC_G,
        KC_TAB, KC_AUDIO_VOL_DOWN, KC_H, HRM_J, HRM_K, HRM_L, HRM_SCLN,
        KC_QUOTE, KC_TRANSPARENT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M,
        KC_COMMA, KC_DOT, KC_SLASH, KC_TRANSPARENT, KC_LEFT_SHIFT, KC_LEFT_CTRL,
        KC_LEFT_ALT, KC_LEFT, KC_RIGHT, TT(3), KC_CAPS, KC_DOWN, KC_UP,
        KC_LEFT_ALT, KC_LEFT_CTRL, KC_LEFT_SHIFT, LT(1, KC_ENTER),
        MT(MOD_LGUI, KC_TAB), KC_LEFT_CTRL, KC_LEFT_ALT, DUAL_FUNC_0,
        LT(2, KC_SPACE)),
    [1] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8,
        KC_F9, KC_F10, KC_F11, KC_F12, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_HOME, KC_PAGE_UP, KC_PLUS, KC_7, KC_8, KC_9, KC_ASTR,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_END, KC_PGDN,
        KC_MINUS, KC_4, KC_5, KC_6, KC_EQUAL, KC_GRAVE, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_UNDS, KC_1, KC_2, KC_3, KC_BSLS, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_0),
    [2] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8,
        KC_F9, KC_F10, KC_F11, KC_F12, KC_TRANSPARENT, KC_TRANSPARENT, KC_EXLM,
        KC_AT, KC_HASH, KC_DLR, KC_PERC, KC_HOME, KC_PAGE_UP, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, DUAL_FUNC_1, DUAL_FUNC_2, KC_LBRC,
        DUAL_FUNC_3, KC_PIPE, KC_END, KC_PGDN, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
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
  // Handle HRM keys
  int8_t hrm_idx = get_hrm_index(keycode);
  if (hrm_idx >= 0) {
    if (record->event.pressed) {
      hrm_handle_press(hrm_idx);
    } else {
      hrm_handle_release(hrm_idx);
    }
    return false; // We handle these keys completely
  }

  // Handle thumb layer keys - track their state
  switch (keycode) {
  case LT(1, KC_ENTER):
    if (record->event.pressed) {
      thumb_layer_left_held = true;
      // Check for any pending HRM keys that should transition to hold
      hrm_check_pending_keys_for_hold();
    } else {
      thumb_layer_left_held = false;
      // Invalidate any pending HRM keys
      hrm_invalidate_pending_keys();
    }
    break; // Let QMK handle the LT() behavior

  case LT(2, KC_SPACE):
    if (record->event.pressed) {
      thumb_layer_right_held = true;
      // Check for any pending HRM keys that should transition to hold
      hrm_check_pending_keys_for_hold();
    } else {
      thumb_layer_right_held = false;
      // Invalidate any pending HRM keys
      hrm_invalidate_pending_keys();
    }
    break; // Let QMK handle the LT() behavior
  }

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
