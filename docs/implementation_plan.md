# Conditional Home-Row Mods Implementation

This plan implements a custom home-row mod system where home-row keys act as regular letters on the base Alpha 0 layer, but become dual-function keys (letter on tap, modifier on hold) when a layer-switch thumb key is held.

## User Review Required

> [!IMPORTANT]
> **Breaking Change to Existing DUAL_FUNC Keys**: The current `DUAL_FUNC_0` through `DUAL_FUNC_3` keycodes in the keymap will be replaced with new custom handling. The existing dual-function behavior on layer 2 will be preserved but implemented differently.

> [!WARNING]
> **Timing Sensitivity**: This implementation relies on the `TAPPING_TERM` (default 200ms). You may need to adjust this value based on your typing speed. The current `AUTO_SHIFT_TIMEOUT` of 150ms will also interact with this system.

## Problem Analysis

The user wants:
1. **Base layer (0)**: Home-row keys (A, S, D, F, J, K, L, ;) act as regular letters
2. **Layer 1 (Numbers)**: Left thumb hold activates layer; home-row keys become mod-taps (letter/modifier)
3. **Layer 2 (Symbols)**: Right thumb hold activates layer; home-row keys become mod-taps (letter/modifier)
4. **Key sequence flexibility**: Pressing home-row key first, then adding thumb key within tapping term should activate modifier
5. **Release ordering**: Releasing thumb key before home-row key should result in no action
6. **Same-hand chording**: Multiple home-row keys can be held simultaneously
7. **Rolling detection**: Press order determines letter order (not release order)
8. **Auto Shift compatibility**: Auto Shift should work for all Alpha 0 keys

This requires **completely custom key handling** since QMK's standard tap-hold mechanism decides tap/hold based on the key's definition, not the current layer state.

## Proposed Changes

### Configuration

#### [MODIFY] [config.h](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/config.h)

Add required defines for the custom home-row mod system:
- `TAPPING_TERM` - Define explicit tapping term (200ms recommended)
- `CHORDAL_HOLD` - Enable chordal hold for same-hand detection
- `TAPPING_TERM_PER_KEY` - Allow per-key tapping term customization
- Keep existing `PERMISSIVE_HOLD` and `AUTO_SHIFT_TIMEOUT`

---

### Rules

#### [MODIFY] [rules.mk](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/rules.mk)

Add:
- `DEFERRED_EXEC_ENABLE = yes` - Required for timer-based tap/hold decisions

---

### Keymap Implementation

#### [MODIFY] [keymap.c](file:///Users/jkopanski/oryx-with-custom-qmk/AWDDG/keymap.c)

Major refactoring to implement custom home-row mod handling:

**1. Define new custom keycodes for home-row keys:**
```c
enum custom_keycodes {
  RGB_SLD = ZSA_SAFE_RANGE,
  // Left hand home-row keys
  HRM_A,  // Ctrl on hold (layer 1/2)
  HRM_S,  // Alt on hold (layer 1/2)
  HRM_D,  // GUI on hold (layer 1/2)
  HRM_F,  // Shift on hold (layer 1/2)
  // Right hand home-row keys
  HRM_J,  // Shift on hold (layer 1/2)
  HRM_K,  // GUI on hold (layer 1/2)
  HRM_L,  // Alt on hold (layer 1/2)
  HRM_SCLN, // Ctrl on hold (layer 1/2)
};
```

**2. State tracking structures:**
```c
typedef struct {
  bool pressed;           // Is key currently pressed?
  uint16_t press_time;    // When was it pressed?
  bool layer_was_active;  // Was layer 1/2 active during the press?
  bool hold_decided;      // Has tap/hold decision been made?
  bool is_held;           // Is this key held (modifier mode)?
} hrm_state_t;

static hrm_state_t hrm_states[8]; // One for each home-row key
static bool thumb_layer_active;   // Is left or right thumb layer key held?
```

**3. Core processing logic in `process_record_user()`:**

On home-row key **press**:
- Record press time
- Check if thumb layer is already active → if yes, start tracking for hold
- If no thumb layer active, wait for either:
  - Release (emit tap character)
  - Tapping term expiry (do nothing on base layer)
  - Thumb layer activation within tapping term (transition to hold tracking)

On home-row key **release**:
- If hold was decided and layer is still active → unregister modifier
- If tap was decided → emit character
- If thumb was released first → do nothing

On thumb layer key **press**:
- Set layer active flag
- Check all pending home-row keys and potentially transition them to hold mode

On thumb layer key **release**:
- Clear layer active flag
- Any pending home-row keys become "invalidated" (no action on release)

**4. Update layer definitions:**

Replace `MT()` home-row mod definitions on layers 1 and 2 with transparent keys (`KC_TRANSPARENT`) since the home-row mod logic is now handled in `process_record_user()`.

**5. Modifier mappings (macOS order: Control/Option/Command/Shift):**
```
Left hand:  A→Ctrl, S→Option, D→Command, F→Shift
Right hand: J→Shift, K→Command, L→Option, ;→Ctrl
```

**6. Chordal Hold layout for same-hand detection:**
```c
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT_moonlander(
    'L', 'L', 'L', 'L', 'L', 'L', '*',   '*', 'R', 'R', 'R', 'R', 'R', 'R',
    // ... full layout definition
);
```

## Verification Plan

### Manual Verification

Since this is keyboard firmware, testing must be performed on actual hardware:

1. **Build the firmware**: Run the GitHub Action "Fetch and build layout" to compile the new firmware
2. **Flash the firmware**: Use Keymapp to flash the compiled firmware to the Moonlander
3. **Test scenarios**:

| Test | Action | Expected Result |
|------|--------|-----------------|
| Base layer letter | Tap home-row key (e.g., 'A') on layer 0 | Outputs 'a' |
| Auto Shift letter | Hold 'A' past AUTO_SHIFT_TIMEOUT on layer 0 | Outputs 'A' (shifted) |
| Layer 1 modifier | Hold left thumb, hold 'A', release 'A', release thumb | Ctrl is held then released |
| Layer 2 modifier | Hold right thumb, hold 'J', press another key, release all | Shift+key is output |
| Home-row first, then thumb | Press 'A', then within tapping term press left thumb, hold both | Transitions to Ctrl hold |
| Thumb released first | Hold left thumb, press 'A', release thumb, release 'A' | No action |
| Rolling keys | Quickly roll 'A' → 'S' → 'D' on layer 0 | "asd" in press order |
| Same-hand multiple modifiers | On layer 1, hold 'A' and 'S' together | Ctrl+Alt held |

### Automated Tests

QMK itself does not have a standard test harness for keymap behavior. Testing is manual with physical hardware.

### Build Verification

```bash
# From the repository root, the GitHub Action will run:
# qmk compile -kb moonlander -km AWDDG
# 
# If building locally with Docker:
docker run --rm -v $(pwd):/workdir -w /workdir qmkfm/qmk_cli qmk compile -kb moonlander -km AWDDG
```
