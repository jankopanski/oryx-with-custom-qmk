# Moonlander (`AWDDG`) Workspace Rules

## Hardware & Build Target (Critical)

- **Always build for Moonlander Rev A (`zsa/moonlander/reva`)**:
  ```bash
  cd /Users/jankopanski/Moonlander/oryx-with-custom-qmk && ./test.sh
  make -C /Users/jankopanski/Moonlander/qmk_firmware zsa/moonlander/reva:AWDDG
  ```
  Output binary for Keymapp: `/Users/jankopanski/Moonlander/qmk_firmware/zsa_moonlander_reva_AWDDG.bin`.
- **Never build `zsa/moonlander/revb`**: Rev A uses the `stm32-dfu` bootloader (PID `0xDF11` / `57105`), while Rev B stamps a custom bootloader suffix (PID `0x2003`) that Keymapp rejects with `[DFU] no valid firmware data found for pid 57105`.

## Source of Truth & Verification Workflow

- `oryx-with-custom-qmk/AWDDG/` is the **single source of truth** for the layout — do not assume Oryx syncing or edit directly inside `qmk_firmware/keyboards/zsa/moonlander/keymaps/AWDDG/`.
- Always run `/Users/jankopanski/Moonlander/oryx-with-custom-qmk/test.sh` (syncs sources and runs the full QMK Google Test suite) and compile `zsa/moonlander/reva:AWDDG` with zero warnings in `AWDDG/` after firmware changes.
- Keep living docs (`README.md`, `docs/layout_analysis.md`, `docs/HOME_ROW_MODS.md`, `docs/hrm_test_plan.md`) in sync with layout/HRM changes.

## Host Setup & QMK Constraints

- **macOS virtual Colemak**: Firmware emits QWERTY keycodes; macOS maps them to Colemak (`HRM_A..HRM_SCLN` = `A R S T N E I O`, `KC_P` = `;`). Account for `KC_SCLN` (`O`) and `KC_P` (`;`) in any QMK feature (Auto Shift, Caps Word, Combos, Repeat Key).
- **Primary workload**: AI chat, English prose, and Markdown (not dense IDE coding).
- **Do not enable `FLOW_TAP_TERM`**: Because the symbol layer lives on the right `Space` thumb (`LT(2, KC_SPACE)`), Flow Tap breaks rapid prose-to-symbol typing (`word)`, `wow!`, `follow-up`).
- **Tri-layer quirk (`update_tri_layer_state`)**: To open Layer 4 (`L1 + L2`) when Left Thumb is pressed first, Layer 1's right Space thumb must hold for **Layer 2** (`LT(2, KC_0)`), not Layer 4 directly.

## Ergonomics & Communication Preferences

- **Use plain-English physical key descriptions** (e.g., "right middle thumb", "left of `H`", "beside `\`") and visual maps when discussing layouts — **never** refer to keys by raw matrix coordinates like `[11,5]` or `[8,0]`.
- **Minimize left-pinky load**: Avoid assigning frequent taps or holds to the left pinky column.
- **No mid-typing keys on red thumb keys**: Moonlander's red thumb keys are hard to reach while typing; reserve them for rare/out-of-band actions (`MO(3)`, `AS_TOGG`).
- **No same-thumb chords**: Never propose holding one thumb key while tapping another key on the same thumb.
- **Keep established anchors unless asked**: `ESC` stays top-left (hit with middle finger), `Mute / Vol+ / Vol-` stay left of `6 / J / H`, `LCtrl` / `LAlt` stay on inner thumbs for macOS Spaces/arrows, and RGB LEDs stay solid white across layers.
