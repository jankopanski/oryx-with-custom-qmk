#include QMK_KEYBOARD_H
#include "version.h"
#include "hrm.h"
#define MOON_LED_LEVEL LED_LEVEL
#ifndef ZSA_SAFE_RANGE
#define ZSA_SAFE_RANGE SAFE_RANGE
#endif

enum custom_keycodes {
  RGB_SLD = HRM_SAFE_RANGE,
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // Layer 0: Base (macOS Virtual Colemak + Option 1 Thumbs)
    [0] = LAYOUT_moonlander(
        KC_ESCAPE,      KC_1,           KC_2,           KC_3,           KC_4,           KC_5,           TOGGLE_LAYER_COLOR,
        KC_AUDIO_MUTE,  KC_6,           KC_7,           KC_8,           KC_9,           KC_0,           KC_TRANSPARENT,

        KC_DELETE,      KC_Q,           KC_W,           KC_E,           KC_R,           KC_T,           KC_GRAVE,
        KC_AUDIO_VOL_UP,KC_Y,           KC_U,           KC_I,           KC_O,           KC_P,           KC_TRANSPARENT,

        KC_BSPC,        HRM_A,          HRM_S,          HRM_D,          HRM_F,          KC_G,           LT(3, KC_TAB),
        KC_AUDIO_VOL_DOWN,KC_H,         HRM_J,          HRM_K,          HRM_L,          HRM_SCLN,       KC_QUOTE,

        KC_TRANSPARENT, KC_Z,           KC_X,           KC_C,           KC_V,           KC_B,
                        KC_N,           KC_M,           KC_COMMA,       KC_DOT,         KC_SLASH,       KC_TRANSPARENT,

        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_LEFT,        KC_RIGHT,       MO(3),
        AS_TOGG,                        KC_DOWN,        KC_UP,          QK_REP,         KC_TRANSPARENT, KC_TRANSPARENT,

                                        LT(1, KC_BSPC), MT(MOD_LGUI, KC_ENTER), KC_LEFT_CTRL,
        KC_LEFT_ALT,    OSM(MOD_LSFT),  LT(2, KC_SPACE)
    ),

    // Layer 1: Numbers + Mirrored Symbols + Left HRMs (Hold Left Primary Thumb)
    [1] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1,          KC_F2,          KC_F3,          KC_F4,          KC_F5,          KC_F6,
        KC_F7,          KC_F8,          KC_F9,          KC_F10,         KC_F11,         KC_F12,         KC_TRANSPARENT,

        KC_TRANSPARENT, KC_EXLM,        KC_AT,          KC_HASH,        KC_DLR,         KC_PERC,        KC_HOME,
        KC_PAGE_UP,     KC_PLUS,        KC_7,           KC_8,           KC_9,           KC_ASTR,        KC_SLASH,

        KC_TRANSPARENT, HRM_A,          HRM_S,          HRM_D,          HRM_F,          KC_PIPE,        KC_END,
        KC_PGDN,        KC_MINUS,       KC_4,           KC_5,           KC_6,           KC_EQUAL,       KC_GRAVE,

        KC_TRANSPARENT, KC_CIRC,        KC_RCBR,        KC_RBRC,        KC_RPRN,        KC_AMPR,
                        KC_UNDS,        KC_1,           KC_2,           KC_3,           KC_BSLS,        KC_EQUAL,

        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT,                 KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,

                                        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_BSPC,        KC_DOT,         LT(2, KC_0)
    ),

    // Layer 2: Mirrored Symbols/Numbers + Right HRMs (Hold Right Primary Thumb)
    [2] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_F1,          KC_F2,          KC_F3,          KC_F4,          KC_F5,          KC_F6,
        KC_F7,          KC_F8,          KC_F9,          KC_F10,         KC_F11,         KC_F12,         KC_TRANSPARENT,

        KC_TRANSPARENT, KC_EXLM,        KC_AT,          KC_HASH,        KC_DLR,         KC_PERC,        KC_HOME,
        KC_PAGE_UP,     KC_PLUS,        KC_7,           KC_8,           KC_9,           KC_ASTR,        KC_SLASH,

        KC_TRANSPARENT, KC_TILD,        KC_LCBR,        KC_LBRC,        KC_LPRN,        KC_PIPE,        KC_END,
        KC_PGDN,        KC_MINUS,       HRM_J,          HRM_K,          HRM_L,          HRM_SCLN,       KC_GRAVE,

        KC_TRANSPARENT, KC_CIRC,        KC_RCBR,        KC_RBRC,        KC_RPRN,        KC_AMPR,
                        KC_UNDS,        KC_1,           KC_2,           KC_3,           KC_BSLS,        KC_EQUAL,

        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT,                 KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,

                                        KC_TRANSPARENT, KC_TRANSPARENT, KC_BSPC,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT
    ),

    // Layer 3: macOS Navigation & Media (Hold TAB key or Left Red Thumb)
    [3] = LAYOUT_moonlander(
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, QK_BOOT,

        KC_TRANSPARENT, KC_MEDIA_PREV_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK, KC_AUDIO_MUTE, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, LALT(KC_LEFT),  LALT(KC_BSPC),  LALT(KC_RIGHT), KC_BSPC,        KC_DELETE,      KC_TRANSPARENT,

        KC_TRANSPARENT, KC_LEFT_CTRL,   KC_LEFT_ALT,    KC_LEFT_GUI,    KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, LGUI(KC_LEFT),  KC_LEFT,        KC_DOWN,        KC_UP,          KC_RIGHT,       KC_TRANSPARENT,

        KC_TRANSPARENT, KC_AUDIO_VOL_DOWN, KC_AUDIO_VOL_UP, AS_TOGG,    KC_TRANSPARENT, KC_TRANSPARENT,
                        LGUI(KC_RIGHT), KC_HOME,        KC_PGDN,        KC_PAGE_UP,     KC_END,         KC_TRANSPARENT,

        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT,                 KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,

                                        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT,
        KC_TRANSPARENT, KC_TRANSPARENT, KC_TRANSPARENT
    ),

    // Layer 4: Mod-Alpha Tri-Layer (Hold Both Primary Thumbs, Either Order)
    [4] = LAYOUT_moonlander(
        KC_ESCAPE,      KC_1,           KC_2,           KC_3,           KC_4,           KC_5,           KC_NO,
        KC_NO,          KC_6,           KC_7,           KC_8,           KC_9,           KC_0,           KC_NO,

        KC_DELETE,      KC_Q,           KC_W,           KC_E,           KC_R,           KC_T,           KC_GRAVE,
        KC_NO,          KC_Y,           KC_U,           KC_I,           KC_O,           KC_P,           KC_NO,

        KC_BSPC,        HRM_A,          HRM_S,          HRM_D,          HRM_F,          KC_G,           KC_TAB,
        KC_NO,          KC_H,           HRM_J,          HRM_K,          HRM_L,          HRM_SCLN,       KC_QUOTE,

        KC_NO,          KC_Z,           KC_X,           KC_C,           KC_V,           KC_B,
                        KC_N,           KC_M,           KC_COMMA,       KC_DOT,         KC_SLASH,       KC_NO,

        KC_NO,          KC_NO,          KC_NO,          KC_LEFT,        KC_RIGHT,       KC_NO,
        KC_NO,                          KC_DOWN,        KC_UP,          KC_NO,          KC_NO,          KC_NO,

                                        KC_TRANSPARENT, MT(MOD_LGUI, KC_ENTER), KC_LEFT_CTRL,
        KC_LEFT_ALT,    OSM(MOD_LSFT),  KC_TRANSPARENT
    ),
};
// clang-format on

layer_state_t layer_state_set_user(layer_state_t state) {
  state = update_tri_layer_state(state, 1, 2, 4);
  uint8_t layer = get_highest_layer(state);
  if (layer == 1 || layer == 2 || layer == 4) {
    hrm_activate_modifiers();
  }
  return state;
}

bool get_permissive_hold(uint16_t keycode, keyrecord_t *record) {
  return keycode != LT(2, KC_0);
}

uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
  return keycode == LT(2, KC_0) ? 150 : TAPPING_TERM;
}

uint16_t get_quick_tap_term(uint16_t keycode, keyrecord_t *record) {
  return keycode == LT(1, KC_BSPC) ? 150 : 0;
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

    [4] = {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},
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
    case 4:
      set_layer_color(4);
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
  if (!hrm_process_record(keycode, record)) {
    return false;
  }

  switch (keycode) {
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
