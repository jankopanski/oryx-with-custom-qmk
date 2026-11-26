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

// ----------------------------------------------------------------------------
// Test 1: Quick Tap Sends Lowercase Letter
// ----------------------------------------------------------------------------
// Scenario: Quick tap of an HRM key on Layer 0 during normal alpha typing.
// Sequence:
//   1. Press HRM_A (t=0ms) -> enters PENDING state, starts 180ms auto-shift timer.
//   2. Wait 50ms (well under AUTO_SHIFT_TIMEOUT = 180ms).
//   3. Release HRM_A (t=50ms).
// Expected HID Reports:
//   - On release: tap_code(KC_A) sends report [KC_A], followed by empty report.
// Rationale:
//   Validates baseline typing: quick taps must output lowercase letters with zero delay
//   upon physical release, never modifiers or shifted letters.
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

// ----------------------------------------------------------------------------
// Test 2: Hold Past Timeout Sends Shifted Letter (Auto-Shift)
// ----------------------------------------------------------------------------
// Scenario: Holding an HRM key on Layer 0 past 180ms without any thumb key held.
// Sequence:
//   1. Press HRM_A (t=0ms) -> enters PENDING state.
//   2. Wait 180ms (AUTO_SHIFT_TIMEOUT).
//   3. Auto-shift callback fires: transitions to TAP_FIRED.
//   4. Release HRM_A.
// Expected HID Reports:
//   - At t=180ms: sends [KC_LEFT_SHIFT, KC_A] via weak Shift (single report, never clobbers physical Shift).
//   - On release: clears weak Shift and unregisters KC_A (empty report).
// Rationale:
//   Validates Layer 0 auto-shift: holding a letter past 180ms produces uppercase 'A'
//   and supports OS key repeat while held.
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

    // At 180ms timeout, callback fires: registers Shift+A via weak Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_A));
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // Release -> unregisters A and clears weak Shift in a single empty report
    EXPECT_EMPTY_REPORT(driver);
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ----------------------------------------------------------------------------
// Test 3: Home-Row First Modifier Activation
// ----------------------------------------------------------------------------
// Scenario: "Home-Row First" chord — pressing HRM key first, then realizing a modifier
//           is needed and pressing the thumb layer key before tapping the target key.
// Sequence:
//   1. Press HRM_D (t=0ms) -> enters PENDING state.
//   2. Press Left Thumb (LT(1, KC_ENTER)) at t=50ms (< 180ms).
//   3. Tap target key KC_C at t=80ms.
// Expected HID Reports:
//   - Tapping KC_C resolves thumb hold under PERMISSIVE_HOLD.
//   - Host receives Cmd (KC_LEFT_GUI), then [Cmd, KC_C], then Cmd.
//   - On release of HRM_D: empty report (Cmd releases cleanly, no letter output).
// Rationale:
//   Validates bidirectional chord timing: the user does not need to press the thumb
//   strictly first; pressing HRM key first also activates the modifier cleanly.
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

// ----------------------------------------------------------------------------
// Test 4: Bug 1 Fix — Fast Roll Key Inversion ("from")
// ----------------------------------------------------------------------------
// Scenario: Fast rolling an HRM key into a regular alpha key (e.g. typing "from").
// Sequence:
//   1. Press HRM_F (t=0ms) -> enters PENDING state.
//   2. Press regular KC_R at t=30ms (before HRM_F timeout expires).
//   3. Release HRM_F at t=50ms.
//   4. Release KC_R at t=80ms.
// Expected HID Reports (with AUTO_SHIFT_ENABLE = yes):
//   - On KC_R press: hrm_flush_pending_keys() eagerly flushes HRM_F as 'f' [KC_F],
//     followed by empty report; QMK Auto Shift buffers KC_R until release/next press.
//   - On HRM_F release: ignored (already marked SETTLED_TAP).
//   - On KC_R release: QMK Auto Shift emits 'r' [KC_R], followed by empty report.
//   - Host output is strictly "fr", NEVER inverted to "rf"!
// Rationale:
//   Directly proves resolution of Bug 1: regular keys eagerly flush pending HRM keys
//   in chronological order so fast typing never suffers out-of-order characters.
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

    // Press KC_R -> HRM_F is eagerly flushed as 'f'; QMK Auto Shift buffers KC_R until release
    EXPECT_REPORT(driver, (KC_F));
    EXPECT_EMPTY_REPORT(driver);
    regular_r.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release HRM_F at t=50ms -> was already tapped; release is ignored!
    EXPECT_NO_REPORT(driver);
    hrm_f.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // Release KC_R -> QMK Auto Shift taps 'r'
    EXPECT_REPORT(driver, (KC_R));
    EXPECT_EMPTY_REPORT(driver);
    regular_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ----------------------------------------------------------------------------
// Test 5: Bug 1 Fix — Inverted Release Roll Between Two HRM Keys
// ----------------------------------------------------------------------------
// Scenario: Rolling two HRM keys where keys are pressed in order (S, then D),
//           but released in reverse order (D released before S).
// Sequence:
//   1. Press HRM_S at t=0ms -> PENDING.
//   2. Press HRM_D at t=20ms -> PENDING.
//   3. Release HRM_D at t=40ms (before HRM_S is released).
//   4. Release HRM_S at t=60ms.
// Expected HID Reports:
//   - On HRM_D release: release handler detects older pending key HRM_S, flushes 's'
//     first [KC_S], then sends 'd' [KC_D].
//   - On HRM_S release: ignored (already marked SETTLED_TAP).
//   - Host output is strictly "sd", NEVER inverted to "ds"!
// Rationale:
//   Validates chronological release protection across multiple pending HRM keys.
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

// ----------------------------------------------------------------------------
// Test 6: Cross-Hand Shortcut Ctrl+C (Option A with Thumb Release)
// ----------------------------------------------------------------------------
// Scenario: User triggers Ctrl+C across hands, releasing the thumb key before tapping 'C'.
// Sequence:
//   1. Hold Right Thumb (LT(2, KC_SPACE)) past TAPPING_TERM (200ms) -> Layer 2 active.
//   2. Press Right Pinky (HRM_SCLN) -> registers KC_LEFT_CTRL immediately (0ms lag).
//   3. Release Right Thumb -> keyboard returns to Layer 0. Under Option A, Ctrl stays held!
//   4. Press Left Hand KC_C on Layer 0 -> host receives Ctrl + C.
//   5. Release Left Hand KC_C -> host sees Ctrl still held.
//   6. Release Right Pinky -> Ctrl unregisters cleanly; NO semicolon (';') emitted!
// Expected HID Reports:
//   - On pinky press: [KC_LEFT_CTRL].
//   - On C press: [KC_LEFT_CTRL, KC_C].
//   - On C release: [KC_LEFT_CTRL].
//   - On pinky release: empty report.
// Rationale:
//   Validates the user's primary cross-hand shortcut workflow (Option A) across layer
//   transitions, ensuring zero modifier dropout and zero trailing character leaks.
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

// ----------------------------------------------------------------------------
// Test 7: Cross-Hand Shortcut with Thumb Held Throughout
// ----------------------------------------------------------------------------
// Scenario: Cross-hand shortcut (Ctrl+C) where user keeps Right Thumb held throughout.
// Sequence:
//   1. Hold Right Thumb (LT(2, KC_SPACE)) past TAPPING_TERM (200ms) -> Layer 2 active.
//   2. Press Right Pinky (HRM_SCLN) -> activates KC_LEFT_CTRL immediately.
//   3. Press Left Hand KC_C while thumb is still held -> sends Ctrl + C.
//   4. Release Left Hand KC_C.
//   5. Release Right Pinky -> Ctrl unregisters cleanly.
//   6. Release Right Thumb.
// Expected HID Reports:
//   - Same as Test 6, confirming seamless operation when thumb is not released early.
// Rationale:
//   Ensures thumb release is purely optional; holding thumb throughout behaves identically.
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

// ----------------------------------------------------------------------------
// Test 8: Bug 4 — 180ms Auto-Shift Lockout Prevents Modifier Activation
// ----------------------------------------------------------------------------
// Scenario: Key held past 180ms auto-shifts to uppercase; subsequent thumb press
//           must NOT turn that key into a modifier.
// Sequence:
//   1. Press HRM_A (t=0ms) on Layer 0 -> enters PENDING.
//   2. Hold past 180ms -> auto-shift callback fires Shift+A (state TAP_FIRED).
//   3. Press thumb key (LT(1, KC_ENTER)) and hold past 200ms.
//      Because HRM_A is in TAP_FIRED state, hrm_activate_modifiers() ignores it.
//   4. Release HRM_A -> unregisters A, then unregisters Shift.
//   5. Release thumb key.
// Expected HID Reports:
//   - At t=180ms: [KC_LEFT_SHIFT, KC_A].
//   - On thumb press: NO report (Ctrl must NOT activate).
//   - On HRM_A release: empty report.
// Rationale:
//   Proves boundary condition of auto-shift: once a key has auto-shifted to uppercase,
//   it cannot be retroactively repurposed into a modifier chord.
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

    // 2. Hold past AUTO_SHIFT_TIMEOUT (180ms) -> fires auto-shift: Shift+A via weak Shift
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

    // 4. Release HRM_A -> unregisters A and clears weak Shift
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

// ----------------------------------------------------------------------------
// Test 9: Multi-HRM Modifier Chording (Cmd + Shift + C)
// ----------------------------------------------------------------------------
// Scenario: Pressing two HRM keys simultaneously during a thumb hold to create a
//           multi-modifier chord (e.g. Cmd + Shift + C).
// Sequence:
//   1. Hold Left Thumb (LT(1, KC_ENTER)) -> Layer 1 active.
//   2. Press HRM_D (Cmd) -> registers KC_LEFT_GUI immediately.
//   3. Press HRM_F (Shift) -> registers KC_LEFT_SHIFT immediately alongside Cmd.
//   4. Tap KC_C -> sends Cmd + Shift + C.
//   5. Release HRM_F -> Shift unregisters, Cmd remains held.
//   6. Release HRM_D -> Cmd unregisters (empty report).
//   7. Release thumb.
// Expected HID Reports:
//   - Step 2: [KC_LEFT_GUI]
//   - Step 3: [KC_LEFT_GUI, KC_LEFT_SHIFT]
//   - Step 4: [KC_LEFT_GUI, KC_LEFT_SHIFT, KC_C], then [KC_LEFT_GUI, KC_LEFT_SHIFT]
//   - Step 5: [KC_LEFT_GUI]
//   - Step 6: empty report
// Rationale:
//   Validates that the HRM engine supports arbitrary multi-modifier chording without
//   interference between home-row keys.
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

// ----------------------------------------------------------------------------
// Test 10: Bug 6 Fix — HRM_H With Thumb Fallback to Auto-Shift (No Stuck State)
// ----------------------------------------------------------------------------
// Scenario: HRM_H has no assigned modifier (modifier == 0). When pressed during a thumb
//           hold and held past 180ms, it should cleanly fall back to auto-shift (Shift+H)
//           instead of becoming stuck in PENDING state.
// Sequence:
//   1. Hold Left Thumb (LT(1, KC_ENTER)) -> Layer 1 active.
//   2. Press HRM_H -> modifier is 0, so enters PENDING with 180ms timer.
//   3. Hold past 180ms -> auto-shift callback fires: sends Shift + H (TAP_FIRED).
//   4. Release HRM_H -> unregisters H and Shift (empty report).
//   5. Release thumb.
//   6. Verify state machine: hrm_states[HRM_H].state == HRM_STATE_IDLE.
// Expected HID Reports:
//   - At t=180ms: [KC_LEFT_SHIFT, KC_H].
//   - On release: empty report.
// Rationale:
//   Guarantees that keys without modifiers (HRM_H) never lock up or get trapped in an
//   unreachable state during thumb layer activation.
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

    // 3. Hold past AUTO_SHIFT_TIMEOUT (180ms) -> fires auto-shift: Shift+H via weak Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_H));
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // 4. Release HRM_H -> unregisters H and clears weak Shift
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

// ----------------------------------------------------------------------------
// Test 11: Cross-Layer Release by Matrix Position
// ----------------------------------------------------------------------------
// Scenario: HRM key pressed on Layer 0, thumb switches layer where the same matrix
//           key position maps to a different keycode (e.g. KC_1). When physically released,
//           hrm_get_state_by_position() must locate and release the modifier cleanly.
// Sequence:
//   1. Press HRM_A (col 1, row 0) on Layer 0 -> enters PENDING.
//   2. Press thumb key (LT(1, KC_ENTER)) -> switches to Layer 1.
//   3. Tap KC_C to resolve thumb hold -> Ctrl activates, C sent, C released.
//   4. Release matrix key (col 1, row 0) while on Layer 1 (where matrix is mapped to KC_1).
//   5. Release thumb key.
//   6. Verify HRM_A state returned to HRM_STATE_IDLE.
// Expected HID Reports:
//   - Step 3: [KC_LEFT_CTRL], [KC_LEFT_CTRL, KC_C], [KC_LEFT_CTRL]
//   - Step 4: empty report (Ctrl unregisters cleanly, KC_1 is NOT emitted).
// Rationale:
//   Validates physical matrix position tracking (row, col) across layer changes,
//   preventing stuck modifiers caused by keycode divergence across layers.
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

// ============================================================================
// Bug A Fix Tests: Fast Roll Into Thumb Space / Enter
// ============================================================================

// ----------------------------------------------------------------------------
// Test 12: Bug A Fix — Fast Roll HRM_K (Colemak 'e') Into Quick Tap Space
// ----------------------------------------------------------------------------
// Scenario: Fast rolling a right home-row key (HRM_K, Colemak 'e') into a quick tap
//           of LT(2, KC_SPACE) where Space releases before HRM_K releases.
// Sequence:
//   1. Press HRM_K (t=0ms) -> enters PENDING state.
//   2. Press LT(2, KC_SPACE) at t=30ms -> buffered by QMK tapping.
//   3. Release LT(2, KC_SPACE) at t=60ms -> resolves as tap (tap.count == 1).
//      hrm_process_record() must NOT set thumb_2_pressed or activate Cmd; it must
//      flush HRM_K as KC_K before KC_SPACE is sent.
//   4. Release HRM_K at t=80ms -> already SETTLED_TAP, ignored.
// Expected HID Reports:
//   - On Space release: [KC_K], empty report, [KC_SPACE], empty report.
//   - NEVER [KC_LEFT_GUI, KC_SPACE] (Spotlight misfire)!
// Rationale:
//   Prevents rolling word-ending 'e' into Space from hijacking HRM_K into Cmd+Space.
TEST_F(Hrm, fix_bug_a_fast_roll_hrm_k_to_space) {
    TestDriver driver;
    InSequence s;
    auto hrm_k   = KeymapKey(0, 2, 1, HRM_K);
    auto thumb_r = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    set_keymap({hrm_k, thumb_r});

    // 1. Press HRM_K -> enters PENDING
    EXPECT_NO_REPORT(driver);
    hrm_k.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    // 2. Press LT(2, KC_SPACE) -> buffered by QMK tapping
    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    // 3. Release LT(2, KC_SPACE) before HRM_K -> resolves as TAP (tap.count == 1):
    //    flushes HRM_K as 'k' first, then emits KC_SPACE (NEVER Cmd+Space!)
    EXPECT_REPORT(driver, (KC_K));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_FALSE(thumb_2_pressed);

    idle_for(20);

    // 4. Release HRM_K -> already settled as tap, no report
    EXPECT_NO_REPORT(driver);
    hrm_k.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    hrm_key_state_t *state = hrm_get_state(HRM_K);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

// ----------------------------------------------------------------------------
// Test 13: Bug A Fix — Fast Roll HRM_SCLN (Colemak 'o') Into Quick Tap Space
// ----------------------------------------------------------------------------
// Scenario: Fast rolling right pinky (HRM_SCLN, Colemak 'o') into a quick tap of
//           LT(2, KC_SPACE) where Space releases before HRM_SCLN releases.
// Sequence:
//   1. Press HRM_SCLN (t=0ms) -> PENDING.
//   2. Press LT(2, KC_SPACE) (t=30ms) -> buffered by QMK tapping.
//   3. Release LT(2, KC_SPACE) (t=60ms) -> tap (tap.count == 1); flushes KC_SCLN then KC_SPACE.
//   4. Release HRM_SCLN (t=80ms) -> already SETTLED_TAP, ignored.
// Expected HID Reports:
//   - On Space release: [KC_SCLN], empty report, [KC_SPACE], empty report.
//   - NEVER [KC_LEFT_CTRL, KC_SPACE]!
// Rationale:
//   Prevents rolling word-ending 'o' into Space from firing Ctrl+Space.
TEST_F(Hrm, fix_bug_a_fast_roll_hrm_scln_to_space) {
    TestDriver driver;
    InSequence s;
    auto hrm_scln = KeymapKey(0, 1, 1, HRM_SCLN);
    auto thumb_r  = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    set_keymap({hrm_scln, thumb_r});

    EXPECT_NO_REPORT(driver);
    hrm_scln.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    EXPECT_REPORT(driver, (KC_SCLN));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_FALSE(thumb_2_pressed);

    idle_for(20);

    EXPECT_NO_REPORT(driver);
    hrm_scln.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    hrm_key_state_t *state = hrm_get_state(HRM_SCLN);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

// ----------------------------------------------------------------------------
// Test 14: Bug A Fix — Fast Roll HRM_D Into Quick Tap Enter
// ----------------------------------------------------------------------------
// Scenario: Fast rolling left home-row HRM_D into a quick tap of LT(1, KC_ENTER)
//           where Enter releases before HRM_D releases.
// Sequence:
//   1. Press HRM_D (t=0ms) -> PENDING.
//   2. Press LT(1, KC_ENTER) (t=30ms) -> buffered by QMK tapping.
//   3. Release LT(1, KC_ENTER) (t=60ms) -> tap (tap.count == 1); flushes KC_D then KC_ENTER.
//   4. Release HRM_D (t=80ms) -> already SETTLED_TAP, ignored.
// Expected HID Reports:
//   - On Enter release: [KC_D], empty report, [KC_ENTER], empty report.
//   - NEVER [KC_LEFT_GUI, KC_ENTER]!
// Rationale:
//   Prevents rolling a left-hand HRM letter into Enter from firing Cmd+Enter.
TEST_F(Hrm, fix_bug_a_fast_roll_hrm_d_to_enter) {
    TestDriver driver;
    InSequence s;
    auto hrm_d   = KeymapKey(0, 3, 0, HRM_D);
    auto thumb_l = KeymapKey(0, 4, 0, LT(1, KC_ENTER));
    set_keymap({hrm_d, thumb_l});

    EXPECT_NO_REPORT(driver);
    hrm_d.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    EXPECT_NO_REPORT(driver);
    thumb_l.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    EXPECT_REPORT(driver, (KC_D));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_ENTER));
    EXPECT_EMPTY_REPORT(driver);
    thumb_l.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_FALSE(thumb_1_pressed);

    idle_for(20);

    EXPECT_NO_REPORT(driver);
    hrm_d.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    hrm_key_state_t *state = hrm_get_state(HRM_D);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

// ============================================================================
// Bug D1 Fix Tests: Active Modifiers Bypass HRM Delay & Auto-Shift
// ============================================================================

// ----------------------------------------------------------------------------
// Test 15: Bug D1 Fix — MT(MOD_LGUI, KC_TAB) Held + HRM_S (Held >180ms & Early Mod Release)
// ----------------------------------------------------------------------------
// Scenario: User holds Cmd/Tab (MT(MOD_LGUI, KC_TAB)) and presses HRM_S (Cmd+S),
//           holds HRM_S past AUTO_SHIFT_TIMEOUT (180ms), and releases Cmd BEFORE HRM_S.
// Sequence:
//   1. Hold MT(MOD_LGUI, KC_TAB) past TAPPING_TERM -> KC_LEFT_GUI registered.
//   2. Press HRM_S -> active_mods != 0 -> registers KC_S immediately (HRM_STATE_DIRECT_KEY).
//   3. Hold HRM_S for 220ms (> 180ms) -> no Shift injected!
//   4. Release MT(MOD_LGUI, KC_TAB) BEFORE HRM_S -> Cmd releases, KC_S remains held.
//   5. Release HRM_S -> KC_S unregisters (empty report).
// Expected HID Reports:
//   - Step 1: [KC_LEFT_GUI]
//   - Step 2: [KC_LEFT_GUI, KC_S] immediately on press
//   - Step 3: NO report (no auto-shift Shift injection)
//   - Step 4: [KC_S]
//   - Step 5: empty report
// Rationale:
//   Fixes dropped shortcuts (when Cmd releases before HRM_S) and stray Shift injection
//   (when Cmd+S is held past 180ms).
TEST_F(Hrm, fix_bug_d1_cmd_tab_with_hrm_s) {
    TestDriver driver;
    InSequence s;
    auto cmd_tab = KeymapKey(0, 6, 0, MT(MOD_LGUI, KC_TAB));
    auto hrm_s   = KeymapKey(0, 2, 0, HRM_S);
    set_keymap({cmd_tab, hrm_s});

    // 1. Hold MT(MOD_LGUI, KC_TAB) past TAPPING_TERM -> activates Cmd
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    cmd_tab.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press HRM_S -> immediately sends Cmd+S (no HRM delay!)
    EXPECT_REPORT(driver, (KC_LEFT_GUI, KC_S));
    hrm_s.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Hold past AUTO_SHIFT_TIMEOUT (180ms) -> must NOT inject Shift!
    EXPECT_NO_REPORT(driver);
    idle_for(AUTO_SHIFT_TIMEOUT + 40);
    VERIFY_AND_CLEAR(driver);

    // 4. Release Cmd/Tab BEFORE releasing HRM_S -> shortcut already fired, S stays held
    EXPECT_REPORT(driver, (KC_S));
    cmd_tab.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release HRM_S -> unregisters S cleanly
    EXPECT_EMPTY_REPORT(driver);
    hrm_s.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    hrm_key_state_t *state = hrm_get_state(HRM_S);
    EXPECT_NE(state, nullptr);
    EXPECT_EQ(state->state, HRM_STATE_IDLE);
}

// ----------------------------------------------------------------------------
// Test 16: Bug D1 Fix — Physical KC_LSFT With HRM_S (No Delay, No Shift Clobber)
// ----------------------------------------------------------------------------
// Scenario: User holds physical KC_LEFT_SHIFT and presses HRM_S past 180ms, then
//           releases HRM_S while still holding physical KC_LEFT_SHIFT.
// Sequence:
//   1. Press physical KC_LEFT_SHIFT -> [KC_LEFT_SHIFT].
//   2. Press HRM_S -> active_mods != 0 -> immediately sends [KC_LEFT_SHIFT, KC_S].
//   3. Hold 220ms (> 180ms) -> no extra report.
//   4. Release HRM_S -> unregisters KC_S only; [KC_LEFT_SHIFT] remains intact!
//   5. Release physical KC_LEFT_SHIFT -> empty report.
// Rationale:
//   Ensures holding physical Shift with HRM keys registers shifted letters immediately
//   and never clobbers the physically held Shift modifier on key release.
TEST_F(Hrm, fix_bug_d1_physical_shift_with_hrm) {
    TestDriver driver;
    InSequence s;
    auto lshift = KeymapKey(0, 7, 0, KC_LEFT_SHIFT);
    auto hrm_s  = KeymapKey(0, 2, 0, HRM_S);
    set_keymap({lshift, hrm_s});

    // 1. Hold physical Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    lshift.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 2. Press HRM_S -> registers immediately with Shift
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT, KC_S));
    hrm_s.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Hold past 180ms -> no auto-shift interference
    EXPECT_NO_REPORT(driver);
    idle_for(AUTO_SHIFT_TIMEOUT + 40);
    VERIFY_AND_CLEAR(driver);

    // 4. Release HRM_S -> Shift MUST stay held!
    EXPECT_REPORT(driver, (KC_LEFT_SHIFT));
    hrm_s.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release physical Shift
    EXPECT_EMPTY_REPORT(driver);
    lshift.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ----------------------------------------------------------------------------
// Test 17: Bug D1 Fix — One-Shot Modifier (OSM) Applied and Cleared With HRM Key
// ----------------------------------------------------------------------------
// Scenario: User taps OSM(MOD_LCTL) and then presses HRM_S.
// Sequence:
//   1. Tap OSM(MOD_LCTL) -> oneshot Ctrl becomes active.
//   2. Press HRM_S -> active_mods sees get_oneshot_mods() != 0 -> immediately sends
//      [KC_LEFT_CTRL, KC_S] and clears oneshot mods.
//   3. Hold >180ms -> no auto-shift.
//   4. Release HRM_S -> empty report.
//   5. Tap HRM_A -> plain [KC_A], empty report (oneshot Ctrl was cleanly cleared).
// Rationale:
//   Ensures Layer 3 OSM modifiers apply cleanly to HRM keys on press and do not leak
//   onto subsequent keypresses.
TEST_F(Hrm, fix_bug_d1_osm_with_hrm) {
    TestDriver driver;
    InSequence s;
    auto osm_ctl = KeymapKey(0, 7, 0, OSM(MOD_LCTL));
    auto hrm_s   = KeymapKey(0, 2, 0, HRM_S);
    auto hrm_a   = KeymapKey(0, 1, 0, HRM_A);
    set_keymap({osm_ctl, hrm_s, hrm_a});

    // 1. Tap OSM(MOD_LCTL)
    EXPECT_NO_REPORT(driver);
    tap_key(osm_ctl, 40);
    VERIFY_AND_CLEAR(driver);

    // 2. Press HRM_S -> immediately fires Ctrl+S and clears oneshot mod
    EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_S));
    hrm_s.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Hold past 180ms -> no auto-shift
    EXPECT_NO_REPORT(driver);
    idle_for(AUTO_SHIFT_TIMEOUT + 40);
    VERIFY_AND_CLEAR(driver);

    // 4. Release HRM_S -> empty report
    EXPECT_EMPTY_REPORT(driver);
    hrm_s.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Subsequent HRM_A tap -> plain 'a' (OSM was cleared)
    EXPECT_REPORT(driver, (KC_A));
    EXPECT_EMPTY_REPORT(driver);
    tap_key(hrm_a, 40);
    VERIFY_AND_CLEAR(driver);
}

// ----------------------------------------------------------------------------
// Test 18: Bug D1 Fix — Option A Cross-Hand Shortcut Targeting Another HRM Key
// ----------------------------------------------------------------------------
// Scenario: User activates Ctrl via Right Thumb + Right HRM_SCLN, releases Right Thumb
//           (returning to Layer 0 with Ctrl held under Option A), and presses Left HRM_A
//           (Ctrl+A).
// Sequence:
//   1. Hold Right Thumb (LT(2, KC_SPACE)) past TAPPING_TERM -> Layer 2 active.
//   2. Press Right HRM_SCLN -> activates KC_LEFT_CTRL immediately.
//   3. Release Right Thumb -> returns to Layer 0; KC_LEFT_CTRL stays active.
//   4. Press Left HRM_A -> active_mods != 0 -> immediately sends [KC_LEFT_CTRL, KC_A]!
//   5. Hold past 180ms -> no auto-shift.
//   6. Release Left HRM_A -> [KC_LEFT_CTRL].
//   7. Release Right HRM_SCLN -> empty report.
// Expected HID Reports:
//   - Step 2: [KC_LEFT_CTRL]
//   - Step 4: [KC_LEFT_CTRL, KC_A] immediately on press
//   - Step 6: [KC_LEFT_CTRL]
//   - Step 7: empty report
// Rationale:
//   Fixes cross-hand Option A shortcuts whose target key is itself an HRM key (e.g.
//   Ctrl+A, Cmd+S, Cmd+F), ensuring immediate registration with zero auto-shift clash.
TEST_F(Hrm, fix_bug_d1_option_a_cross_hand_to_hrm_key) {
    TestDriver driver;
    InSequence s;
    auto thumb_r      = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    auto hrm_scln     = KeymapKey(0, 1, 1, HRM_SCLN);
    auto layer_2_scln = KeymapKey(2, 1, 1, HRM_SCLN);
    auto hrm_a        = KeymapKey(0, 1, 0, HRM_A);
    set_keymap({thumb_r, hrm_scln, layer_2_scln, hrm_a});

    // 1. Hold Right Thumb -> Layer 2
    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    // 2. Press Right HRM_SCLN -> activates Ctrl immediately
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    hrm_scln.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 3. Release Right Thumb -> returns to Layer 0; Ctrl remains held (Option A)
    EXPECT_NO_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Press Left HRM_A -> active_mods bypass registers Ctrl+A immediately on press!
    EXPECT_REPORT(driver, (KC_LEFT_CTRL, KC_A));
    hrm_a.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Hold past 180ms -> must NOT auto-shift!
    EXPECT_NO_REPORT(driver);
    idle_for(AUTO_SHIFT_TIMEOUT + 40);
    VERIFY_AND_CLEAR(driver);

    // 6. Release Left HRM_A -> Ctrl remains held
    EXPECT_REPORT(driver, (KC_LEFT_CTRL));
    hrm_a.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 7. Release Right HRM_SCLN -> Ctrl unregisters cleanly
    EXPECT_EMPTY_REPORT(driver);
    hrm_scln.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(hrm_get_state(HRM_A)->state, HRM_STATE_IDLE);
    EXPECT_EQ(hrm_get_state(HRM_SCLN)->state, HRM_STATE_IDLE);
}

// ============================================================================
// Bug D2 Fix Tests: Non-HRM -> HRM Roll Under AUTO_SHIFT_ENABLE
// ============================================================================

// ----------------------------------------------------------------------------
// Test 19: Bug D2 Fix — Non-HRM -> HRM Inverted Roll Under QMK Auto Shift
// ----------------------------------------------------------------------------
// Scenario: Rolling a regular alpha key (KC_H) into an HRM key (HRM_K) with inverted
//           release order (HRM_K released before KC_H, and KC_H held past 180ms).
// Sequence:
//   1. Press KC_H (t=0ms) -> QMK Auto Shift buffers KC_H (in_progress = true).
//   2. Press HRM_K (t=30ms) -> hrm_process_record() calls process_auto_shift(KC_NO, record)
//      to flush KC_H as lowercase 'h' BEFORE HRM_K enters PENDING!
//   3. Release HRM_K (t=60ms) -> outputs lowercase 'k'.
//   4. Continue holding KC_H past 180ms -> must NOT falsely auto-shift 'H'!
//   5. Release KC_H (t=220ms) -> no duplicate report.
// Expected HID Reports:
//   - On HRM_K press: [KC_H], empty report.
//   - On HRM_K release: [KC_K], empty report.
//   - While holding KC_H past 180ms and on release: NO report.
//   - Strictly "hk", NEVER inverted to "kh" and NEVER auto-shifted to "Hk"!
// Rationale:
//   Proves that returning false for HRM_* keys in process_record_user() no longer
//   starves QMK's process_auto_shift() of flush notifications.
TEST_F(Hrm, fix_bug_d2_non_hrm_to_hrm_roll_with_autoshift) {
    TestDriver driver;
    InSequence s;
    auto kc_h  = KeymapKey(0, 0, 1, KC_H);
    auto hrm_k = KeymapKey(0, 2, 1, HRM_K);
    set_keymap({kc_h, hrm_k});

    // 1. Press regular KC_H -> buffered by QMK Auto Shift
    EXPECT_NO_REPORT(driver);
    kc_h.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    // 2. Press HRM_K -> flushes QMK Auto Shift's pending KC_H immediately as lowercase 'h'
    EXPECT_REPORT(driver, (KC_H));
    EXPECT_EMPTY_REPORT(driver);
    hrm_k.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    idle_for(30);

    // 3. Release HRM_K -> sends lowercase 'k'
    EXPECT_REPORT(driver, (KC_K));
    EXPECT_EMPTY_REPORT(driver);
    hrm_k.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 4. Keep holding KC_H well past AUTO_SHIFT_TIMEOUT (total >210ms) -> NO false Auto Shift!
    EXPECT_NO_REPORT(driver);
    idle_for(AUTO_SHIFT_TIMEOUT);
    VERIFY_AND_CLEAR(driver);

    // 5. Release KC_H -> already flushed cleanly, no duplicate report
    EXPECT_NO_REPORT(driver);
    kc_h.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// ============================================================================
// Bug C Fix Tests: QUICK_TAP_TERM 0 Allows Rapid Tap-Then-Hold Layer Switch
// ============================================================================

// ----------------------------------------------------------------------------
// Test 20: Bug C Fix — Rapid Space Tap Then Hold Enters Layer 2 (No Repeat)
// ----------------------------------------------------------------------------
// Scenario: User taps Space (LT(2, KC_SPACE)) and within 200ms immediately presses
//           and holds LT(2, KC_SPACE) to type a Layer 2 symbol or activate a modifier.
// Sequence:
//   1. Tap LT(2, KC_SPACE) (t=0..40ms) -> sends [KC_SPACE], empty report.
//   2. Wait 30ms (well within the default 200ms QUICK_TAP_TERM!).
//   3. Press and hold LT(2, KC_SPACE) past TAPPING_TERM (200ms).
//      With QUICK_TAP_TERM 0, QMK must enter Layer 2 instead of repeating Space!
//   4. Press HRM_K on Layer 2 -> activates KC_LEFT_GUI immediately.
//   5. Release HRM_K, then release LT(2, KC_SPACE) -> returns to Layer 0.
// Expected HID Reports:
//   - Step 1: [KC_SPACE], empty report.
//   - Step 3: NO report (no Space auto-repeat!) and highest layer == 2.
//   - Step 4: [KC_LEFT_GUI].
//   - Step 5: empty report, then highest layer == 0.
// Rationale:
//   Validates QUICK_TAP_TERM 0 so typing a space before a symbol (e.g. " (foo)")
//   enters Layer 2 reliably instead of spamming spaces.
TEST_F(Hrm, fix_bug_c_quick_tap_then_hold_enters_layer_2) {
    TestDriver driver;
    InSequence s;
    auto thumb_r   = KeymapKey(0, 4, 1, LT(2, KC_SPACE));
    auto hrm_k     = KeymapKey(0, 2, 1, HRM_K);
    auto layer_2_k = KeymapKey(2, 2, 1, HRM_K);
    set_keymap({thumb_r, hrm_k, layer_2_k});

    // 1. Quick tap Space
    EXPECT_REPORT(driver, (KC_SPACE));
    EXPECT_EMPTY_REPORT(driver);
    tap_key(thumb_r, 40);
    VERIFY_AND_CLEAR(driver);

    // 2. Wait only 30ms (well under default 200ms quick-tap window)
    EXPECT_NO_REPORT(driver);
    idle_for(30);
    VERIFY_AND_CLEAR(driver);

    // 3. Press and hold LT(2, KC_SPACE) past TAPPING_TERM -> enters Layer 2 (NO Space repeat!)
    EXPECT_NO_REPORT(driver);
    thumb_r.press();
    run_one_scan_loop();
    idle_for(TAPPING_TERM);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(get_highest_layer(layer_state), 2);
    EXPECT_TRUE(thumb_2_pressed);

    // 4. Press HRM_K on Layer 2 -> activates Cmd immediately
    EXPECT_REPORT(driver, (KC_LEFT_GUI));
    hrm_k.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    // 5. Release HRM_K and thumb
    EXPECT_EMPTY_REPORT(driver);
    hrm_k.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_NO_REPORT(driver);
    thumb_r.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EQ(get_highest_layer(layer_state), 0);
    EXPECT_FALSE(thumb_2_pressed);
}



