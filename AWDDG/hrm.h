#pragma once
#include "quantum.h"

// In firmware builds with ZSA modules, ZSA_SAFE_RANGE is defined.
// In test builds or standard builds, fallback to SAFE_RANGE.
#ifdef ZSA_SAFE_RANGE
#define HRM_KEYCODE_BASE ZSA_SAFE_RANGE
#else
#define HRM_KEYCODE_BASE SAFE_RANGE
#endif

enum hrm_custom_keycodes {
    HRM_A = HRM_KEYCODE_BASE, // Home-row mod: A (Ctrl on Layer 1/4)
    HRM_S,                    // Home-row mod: S (Alt on Layer 1/4)
    HRM_D,                    // Home-row mod: D (Cmd on Layer 1/4)
    HRM_F,                    // Home-row mod: F (Shift on Layer 1/4)
    HRM_H,                    // Home-row mod: H (no modifier)
    HRM_J,                    // Home-row mod: J (Shift on Layer 2/4)
    HRM_K,                    // Home-row mod: K (Cmd on Layer 2/4)
    HRM_L,                    // Home-row mod: L (Alt on Layer 2/4)
    HRM_SCLN,                 // Home-row mod: ; (Ctrl on Layer 2/4)
    HRM_SAFE_RANGE,           // Base for subsequent keycodes in keymap.c
};

// ============================================================================
// Home-Row Mod State Tracking Types
// ============================================================================

typedef enum {
    HRM_STATE_IDLE,           // Key not pressed
    HRM_STATE_PENDING,        // Pressed, waiting for decision
    HRM_STATE_SETTLED_TAP,    // Flushed early due to roll/regular key press; release ignored
    HRM_STATE_TAP_FIRED,      // Auto-shift fired, shifted letter held
    HRM_STATE_DIRECT_KEY,     // Registered immediately on press (active mod / opposite-hand bypass)
    HRM_STATE_MODIFIER_ACTIVE // Modifier mode active
} hrm_state_t;

// Exact thumb layer-tap keycodes that control HRM modifier activation
#define HRM_THUMB_1_KEYCODE     LT(1, KC_BSPC)
#define HRM_THUMB_2_KEYCODE     LT(2, KC_SPACE)
#define HRM_THUMB_2_ALT_KEYCODE LT(2, KC_0)

typedef struct {
    uint16_t custom_keycode; // Custom keycode (HRM_A, HRM_S, etc.)
    uint16_t letter_keycode; // Letter keycode (KC_A, KC_S, etc.)
    uint16_t modifier;       // Modifier keycode (KC_LCTL, KC_LALT, etc.) - 0 if none
    bool is_left;            // True for left-hand HRM keys (A, S, D, F), false for right-hand
    uint32_t press_time;     // When key was pressed
    hrm_state_t state;       // Current state
    deferred_token token;    // Deferred execution token for auto-shift timer
    uint8_t row;             // Matrix row
    uint8_t col;             // Matrix column
} hrm_key_state_t;

#define HRM_COUNT 9

extern hrm_key_state_t hrm_states[HRM_COUNT];
extern bool thumb_1_pressed;
extern bool thumb_2_pressed;

hrm_key_state_t *hrm_get_state(uint16_t keycode);
hrm_key_state_t *hrm_get_state_by_position(uint8_t row, uint8_t col);
bool hrm_opposite_hand_mod_active(bool is_left);
uint32_t hrm_autoshift_callback(uint32_t trigger_time, void *cb_arg);
void hrm_handle_press(hrm_key_state_t *state, uint8_t row, uint8_t col);
void hrm_handle_release(hrm_key_state_t *state);
void hrm_activate_modifiers(void);
void hrm_flush_pending_keys(void);

// Central processing function to be called from process_record_user.
// Returns false if the key event was consumed by the HRM state machine.
// Returns true if the key event should continue through QMK processing.
bool hrm_process_record(uint16_t keycode, keyrecord_t *record);
