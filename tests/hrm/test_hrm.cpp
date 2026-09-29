#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "test_fixture.hpp"
#include "test_keymap_key.hpp"

extern "C" {
#include "hrm.h"
#include "deferred_exec.h"

void set_time(uint32_t t);

// Hook deferred_exec into the test harness scan loop
void housekeeping_task_user(void) {
    deferred_exec_task();
}

// Mirror keymap.c: activate modifiers on layer changes
layer_state_t layer_state_set_user(layer_state_t state) {
    uint8_t layer = get_highest_layer(state);
    if (layer == 1 || layer == 2) {
        hrm_activate_modifiers();
    }
    return state;
}

// Mirror keymap.c: delegate to hrm_process_record
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return hrm_process_record(keycode, record);
}

} // extern "C"

using testing::_;
using testing::InSequence;

// ============================================================================
// Test Fixture
// ============================================================================

class Hrm : public TestFixture {
   public:
    void SetUp() override {
        // Advance time monotonically across test cases so deferred_exec never throttles
        static uint32_t monotonic_time = 1000;
        monotonic_time += 10000;
        set_time(monotonic_time);

        // Reset all HRM states and thumb tracking between tests
        for (int i = 0; i < HRM_COUNT; i++) {
            hrm_states[i].state = HRM_STATE_IDLE;
            hrm_states[i].press_time = 0;
            hrm_states[i].token = INVALID_DEFERRED_TOKEN;
            hrm_states[i].row = 0;
            hrm_states[i].col = 0;
        }
        thumb_1_pressed = false;
        thumb_2_pressed = false;
    }
};

// ============================================================================
// Baseline Tests
// ============================================================================

// Quick tap on HRM key should send lowercase letter on release
TEST_F(Hrm, quick_tap_sends_letter) {
    TestDriver driver;
    InSequence s;
    auto hrm_a = KeymapKey(0, 1, 0, HRM_A);
    set_keymap({hrm_a});

    // Press
    EXPECT_NO_REPORT(driver);
    hrm_a.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Wait less than AUTO_SHIFT_TIMEOUT (180ms)
    EXPECT_NO_REPORT(driver);
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    // Release -> sends 'a'
    EXPECT_REPORT(driver, (KC_A));
    EXPECT_EMPTY_REPORT(driver);
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// Hold past AUTO_SHIFT_TIMEOUT on Layer 0 -> sends Shift+letter
TEST_F(Hrm, hold_past_timeout_sends_shifted_letter) {
    TestDriver driver;
    InSequence s;
    auto hrm_a = KeymapKey(0, 1, 0, HRM_A);
    set_keymap({hrm_a});

    // Press
    EXPECT_NO_REPORT(driver);
    hrm_a.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // At 180ms timeout, callback fires: registers Shift+A
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_A));
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // Release -> unregisters Shift and A
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// Home-Row First: Press HRM_D, press thumb, tap target key -> modifier resolves via PERMISSIVE_HOLD
TEST_F(Hrm, home_row_first_modifier_activation) {
    TestDriver driver;
    InSequence s;
    auto hrm_d   = KeymapKey(0, 3, 0, HRM_D);
    auto thumb_l = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    auto kc_c    = KeymapKey(0, 5, 0, KC_C);
    auto layer_1_c = KeymapKey(1, 5, 0, KC_C);
    set_keymap({hrm_d, thumb_l, kc_c, layer_1_c});

    // Press HRM_D -> enters PENDING state
    EXPECT_NO_REPORT(driver);
    hrm_d.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 50ms later, press left thumb
    EXPECT_NO_REPORT(driver);
    idle_for(50);
    thumb_l.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 30ms later (t=80ms, well before 180ms timeout), tap target key KC_C
    // Under PERMISSIVE_HOLD, tapping KC_C resolves the thumb hold
    EXPECT_NO_REPORT(driver);
    kc_c.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // On release of KC_C, PERMISSIVE_HOLD resolves: Cmd activates, C is sent, C releases
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_C));
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    kc_c.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release HRM_D -> Cmd unregisters cleanly
    EXPECT_EMPTY_REPORT(driver);
    hrm_d.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release thumb
    thumb_l.release();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
}

// ============================================================================
// Bug 1 Fix Tests: Key Inversion & Rolling Order
// ============================================================================

// Fast roll "from" (HRM_F followed quickly by regular KC_R):
// Must output 'f' then 'r' ("fr"), never "rf"
TEST_F(Hrm, fix_bug1_fast_roll_key_inversion) {
    TestDriver driver;
    InSequence s;
    auto hrm_f     = KeymapKey(0, 4, 0, HRM_F);
    auto regular_r = KeymapKey(0, 5, 0, KC_R);
    set_keymap({hrm_f, regular_r});

    // Press HRM_F -> enters PENDING (no report)
    EXPECT_NO_REPORT(driver);
    hrm_f.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    // Press KC_R -> HRM_F is eagerly flushed as 'f', then 'r' is pressed!
    EXPECT_REPORT(driver, (KC_F));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_R));
    regular_r.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release HRM_F at t=50ms -> was already tapped; release is ignored!
    EXPECT_NO_REPORT(driver);
    hrm_f.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release KC_R -> sends empty report
    EXPECT_EMPTY_REPORT(driver);
    regular_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// Inverted release roll between two HRM keys:
// Press HRM_S -> Press HRM_D -> Release HRM_D -> Release HRM_S:
// Must output "sd" in press order, never "ds"!
TEST_F(Hrm, fix_bug1_inverted_release_roll) {
    TestDriver driver;
    InSequence s;
    auto hrm_s = KeymapKey(0, 2, 0, HRM_S);
    auto hrm_d = KeymapKey(0, 3, 0, HRM_D);
    set_keymap({hrm_s, hrm_d});

    // Press HRM_S (t=0)
    EXPECT_NO_REPORT(driver);
    hrm_s.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(20);

    // Press HRM_D (t=20)
    EXPECT_NO_REPORT(driver);
    hrm_d.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(20);

    // Release HRM_D (t=40) before HRM_S!
    // Release handler detects older pending key HRM_S and flushes 's' first, then 'd'!
    EXPECT_REPORT(driver, (KC_S));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_D));
    EXPECT_EMPTY_REPORT(driver);
    hrm_d.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(20);

    // Release HRM_S (t=60) -> already settled, no report
    EXPECT_NO_REPORT(driver);
    hrm_s.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ============================================================================
// Cross-Hand Shortcut Tests (Option A)
// ============================================================================

// Sequence:
// 1. Right Thumb layer switch (LT(2, KC_SPACE) -> Layer 2)
// 2. Right hand pinky modifier (HRM_SCLN -> Ctrl registered immediately)
// 3. Right Thumb release (returns to Layer 0; Ctrl stays held under Option A)
// 4. Left hand C letter (KC_C -> sends Ctrl+C)
// 5. Left hand C release
// 6. Right pinky release -> Ctrl unregisters cleanly, no ';' output!
TEST_F(Hrm, cross_hand_shortcut_ctrl_c_option_a) {
    TestDriver driver;
    InSequence s;
    auto thumb_r      = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    auto hrm_scln     = KeymapKey(0, 1, 1, HRM_SCLN);
    auto kc_c         = KeymapKey(0, 3, 0, KC_C);
    auto layer_2_scln = KeymapKey(2, 1, 1, HRM_SCLN);
    set_keymap({thumb_r, hrm_scln, kc_c, layer_2_scln});

    // 1. Hold Right Thumb past tapping term to enter Layer 2
    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press Right Pinky (HRM_SCLN) -> activates Ctrl (KC_LEFT_CTRL) immediately!
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    hrm_scln.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Release Right Thumb -> drops back to Layer 0
    // Under Option A: Ctrl remains active as long as HRM_SCLN is held!
    EXPECT_NO_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Press Left Hand C on Layer 0 -> host receives Ctrl + C!
    EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_C));
    kc_c.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release Left Hand C -> host sees Ctrl still held
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    kc_c.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 6. Release Right Pinky (HRM_SCLN) -> Ctrl unregisters, no ';' emitted!
    EXPECT_EMPTY_REPORT(driver);
    hrm_scln.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// Same cross-hand shortcut, but keeping Right Thumb held throughout
TEST_F(Hrm, cross_hand_shortcut_thumb_held_throughout) {
    TestDriver driver;
    InSequence s;
    auto thumb_r      = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    auto hrm_scln     = KeymapKey(0, 1, 1, HRM_SCLN);
    auto kc_c         = KeymapKey(0, 3, 0, KC_C);
    auto layer_2_scln = KeymapKey(2, 1, 1, HRM_SCLN);
    auto layer_2_c    = KeymapKey(2, 3, 0, KC_C);
    set_keymap({thumb_r, hrm_scln, kc_c, layer_2_scln, layer_2_c});

    // 1. Hold Right Thumb past tapping term to enter Layer 2
    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press Right Pinky (HRM_SCLN) -> activates Ctrl immediately
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    hrm_scln.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Press Left Hand C while thumb is still held
    EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_C));
    kc_c.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Release Left Hand C
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    kc_c.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release Right Pinky -> Ctrl unregisters
    EXPECT_EMPTY_REPORT(driver);
    hrm_scln.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 6. Release Right Thumb
    EXPECT_NO_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ============================================================================
// Remaining Edge Case Tests
// ============================================================================

// Bug 4: Holding key past auto-shift timeout registers Shift+key.
// Subsequent thumb press while key is still held should NOT turn it into a modifier!
TEST_F(Hrm, bug4_autoshift_prevents_modifier) {
    TestDriver driver;
    InSequence s;
    auto hrm_a   = KeymapKey(0, 1, 0, HRM_A);
    auto thumb_l = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    set_keymap({hrm_a, thumb_l});

    // 1. Press HRM_A -> enters PENDING state
    EXPECT_NO_REPORT(driver);
    hrm_a.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 2. Hold past AUTO_SHIFT_TIMEOUT (180ms) -> fires auto-shift: Shift+A
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_A));
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // 3. Press thumb key while HRM_A is still held and hold past TAPPING_TERM.
    // HRM_A is in TAP_FIRED state, NOT PENDING, so hrm_activate_modifiers must NOT activate Ctrl!
    EXPECT_NO_REPORT(driver);
    thumb_l.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 4. Release HRM_A -> unregisters A, then Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release thumb
    EXPECT_NO_REPORT(driver);
    thumb_l.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// Multi-HRM modifier chords: e.g. Left Thumb + HRM_D (Cmd) + HRM_F (Shift) + C -> Cmd+Shift+C
TEST_F(Hrm, modifier_chord_two_hrm_keys) {
    TestDriver driver;
    InSequence s;
    auto thumb_l      = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    auto hrm_d        = KeymapKey(0, 3, 0, HRM_D);
    auto hrm_f        = KeymapKey(0, 2, 0, HRM_F);
    auto layer_1_d    = KeymapKey(1, 3, 0, HRM_D);
    auto layer_1_f    = KeymapKey(1, 2, 0, HRM_F);
    auto kc_c         = KeymapKey(0, 5, 0, KC_C);
    auto layer_1_c    = KeymapKey(1, 5, 0, KC_C);
    set_keymap({thumb_l, hrm_d, hrm_f, layer_1_d, layer_1_f, kc_c, layer_1_c});

    // 1. Hold thumb to enter Layer 1
    EXPECT_NO_REPORT(driver);
    thumb_l.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press HRM_D -> registers Cmd (KC_LEFT_GUI) immediately
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    hrm_d.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Press HRM_F -> registers Shift (KC_LEFT_SHIFT) immediately alongside Cmd
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_SHIFT));
    hrm_f.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Tap KC_C -> sends Cmd + Shift + C
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_SHIFT, KC_C));
    kc_c.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_LEFT_SHIFT));
    kc_c.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release HRM_F -> Shift unregisters, Cmd remains
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    hrm_f.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 6. Release HRM_D -> Cmd unregisters
    EXPECT_EMPTY_REPORT(driver);
    hrm_d.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 7. Release thumb
    EXPECT_NO_REPORT(driver);
    thumb_l.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// HRM_H has no modifier assigned (state->modifier == 0).
// When pressed during thumb hold and held past AUTO_SHIFT_TIMEOUT,
// it should fallback to auto-shift (Shift+H) rather than entering a stuck state.
TEST_F(Hrm, hrm_h_with_thumb_no_stuck_state) {
    TestDriver driver;
    InSequence s;
    auto thumb_l      = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    auto hrm_h        = KeymapKey(0, 0, 1, HRM_H);
    auto layer_1_h    = KeymapKey(1, 0, 1, HRM_H);
    set_keymap({thumb_l, hrm_h, layer_1_h});

    // 1. Hold thumb to enter Layer 1
    EXPECT_NO_REPORT(driver);
    thumb_l.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press HRM_H -> modifier is 0, so enters PENDING with auto-shift timer
    EXPECT_NO_REPORT(driver);
    hrm_h.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Hold past AUTO_SHIFT_TIMEOUT (180ms) -> fires auto-shift: Shift+H
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_H));
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // 4. Release HRM_H -> unregisters H, then Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    EXPECT_EMPTY_REPORT(driver);
    hrm_h.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release thumb
    EXPECT_NO_REPORT(driver);
    thumb_l.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 6. Verify HRM_H state returned to IDLE
    hrm_key_state_t *state = hrm_get_state(HRM_H);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

// Cross-layer release:
// Press HRM_A on Layer 0, switch to Layer 1 (where matrix position has different keycode KC_1),
// and release the key. hrm_get_state_by_position must find and release the modifier cleanly.
TEST_F(Hrm, release_by_position_cross_layer) {
    TestDriver driver;
    InSequence s;
    auto hrm_a        = KeymapKey(0, 1, 0, HRM_A);
    auto layer_1_key  = KeymapKey(1, 1, 0, KC_1);
    auto thumb_l      = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    auto kc_c         = KeymapKey(0, 5, 0, KC_C);
    auto layer_1_c    = KeymapKey(1, 5, 0, KC_C);
    set_keymap({hrm_a, layer_1_key, thumb_l, kc_c, layer_1_c});

    // 1. Press HRM_A on Layer 0
    EXPECT_NO_REPORT(driver);
    hrm_a.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 2. Press thumb key to switch to Layer 1
    // Tap KC_C to resolve thumb hold under PERMISSIVE_HOLD
    EXPECT_NO_REPORT(driver);
    thumb_l.press();
    run_one_scan_loop();
    kc_c.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Resolves: Ctrl activates, C sent, C released
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_C));
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    kc_c.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Now on Layer 1, release matrix key (col 1, row 0) via hrm_a.release()
    // Matrix position matches hrm_a, even though Layer 1 maps it to KC_1!
    EXPECT_EMPTY_REPORT(driver);
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Release thumb key
    thumb_l.release();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);

    // 5. Verify state is IDLE
    hrm_key_state_t *state = hrm_get_state(HRM_A);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

