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
